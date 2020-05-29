#include "ActionManager.h"
#include "Actions.h"
#include "ConsoleLines.h"
#include "Log.h"
#include "PeriodicActions.h"
#include "Settings.h"

#include <mh/text/insertion_conversion.hpp>
#include <mh/text/string_insertion.hpp>

#include <filesystem>
#include <fstream>
#include <regex>

#define WIN32_LEAN_AND_MEAN 1
#include <Windows.h>

#undef min
#undef max

static const std::regex s_SingleCommandRegex(R"regex(((?:\w+)(?:\ +\w+)?)(?:\n)?)regex", std::regex::optimize);

using namespace tf2_bot_detector;
using namespace std::chrono_literals;
using namespace std::string_literals;
using namespace std::string_view_literals;

namespace tfbd_paths
{
	namespace local
	{
		static std::filesystem::path cfg()
		{
			return "tf2_bot_detector";
		}
		static std::filesystem::path cfg_temp()
		{
			return cfg() / "temp";
		}
	}
}

auto ActionManager::absolute_root() const
{
	return m_Settings->m_TFDir / "custom/tf2_bot_detector";
}
auto ActionManager::absolute_cfg() const
{
	return absolute_root() / "cfg" / tfbd_paths::local::cfg();
}
auto ActionManager::absolute_cfg_temp() const
{
	return absolute_root() / "cfg" / tfbd_paths::local::cfg_temp();
}

ActionManager::ActionManager(const Settings& settings) :
	m_Settings(&settings)
{
	std::filesystem::remove_all(absolute_cfg_temp());
}

ActionManager::~ActionManager()
{
}

bool ActionManager::QueueAction(std::unique_ptr<IAction>&& action)
{
	if (const auto maxQueuedCount = action->GetMaxQueuedCount();
		maxQueuedCount <= m_Actions.size())
	{
		const ActionType curActionType = action->GetType();
		size_t count = 0;
		for (const auto& queuedAction : m_Actions)
		{
			if (queuedAction->GetType() == curActionType)
			{
				if (++count >= maxQueuedCount)
					return false;
			}
		}
	}

	m_Actions.push_back(std::move(action));
	return true;
}

void ActionManager::AddPeriodicAction(std::unique_ptr<IPeriodicAction>&& action)
{
	m_PeriodicActions.push_back({ std::move(action), {} });
}

void ActionManager::AddPiggybackAction(std::unique_ptr<IAction> action)
{
	m_PiggybackActions.push_back(std::move(action));
}

void ActionManager::Update()
{
	const auto curTime = clock_t::now();
	if (curTime < (m_LastUpdateTime + UPDATE_INTERVAL))
		return;

	// Update periodic actions
	for (auto& action : m_PeriodicActions)
	{
		const auto interval = action.m_Action->GetInterval();
		if (action.m_LastRunTime == time_point_t{})
		{
			const auto delay = action.m_Action->GetInitialDelay();
			action.m_LastRunTime = curTime - interval + delay;
		}

		if ((curTime - action.m_LastRunTime) >= interval)
		{
			if (action.m_Action->Execute(*this))
				action.m_LastRunTime = curTime;
			else
				Log("Couldn't run periodic action");
		}
	}

	if (!m_Actions.empty())
	{
		bool actionTypes[(int)ActionType::COUNT]{};

		struct Writer final : ICommandWriter
		{
			void Write(std::string cmd, std::string args) override
			{
				assert(!cmd.empty());

				if (cmd.empty() && !args.empty())
					throw std::runtime_error("Empty command with non-empty args");

				if (!std::all_of(cmd.begin(), cmd.end(), [](char c) { return c == '_' || isalpha(c) || isdigit(c); }))
					throw std::runtime_error("Command contains invalid characters");

				if (!m_ComplexCommands && !std::all_of(args.begin(), args.end(), [](char c) { return isalpha(c) || isdigit(c) || isspace(c); }))
					m_ComplexCommands = true;

				m_CommandArgCount++;
				if (!args.empty())
					m_CommandArgCount++;

				m_Commands.push_back({ std::move(cmd), std::move(args) });
			}

			std::vector<std::pair<std::string, std::string>> m_Commands;
			bool m_ComplexCommands = false;
			size_t m_CommandArgCount = 0;

		} writer;

		const auto ProcessAction = [&](const IAction* action)
		{
			const ActionType type = action->GetType();
			{
				auto& previousMsg = actionTypes[(int)type];
				const auto minInterval = action->GetMinInterval();

				if (minInterval.count() > 0 && (previousMsg || (curTime - m_LastTriggerTime[type]) < minInterval))
					return false;

				previousMsg = true;
			}

			action->WriteCommands(writer);
			//it = m_Actions.erase(it);
			m_LastTriggerTime[type] = curTime;
			return true;
		};

		// Handle normal actions
		for (auto it = m_Actions.begin(); it != m_Actions.end(); )
		{
			const IAction* action = it->get();
			if (ProcessAction(it->get()))
				it = m_Actions.erase(it);
			else
				++it;
		}

		if (!writer.m_Commands.empty())
		{
			// Handle piggyback commands
			for (const auto& action : m_PiggybackActions)
				ProcessAction(action.get());
		}

		// Is this a "simple" command, aka nothing to confuse the engine/OS CLI arg parser?
		if (!writer.m_ComplexCommands && writer.m_CommandArgCount < 200)
		{
			std::string cmdLine;
			bool firstCmd = true;

			for (const auto& cmd : writer.m_Commands)
			{
				if (firstCmd)
					firstCmd = false;
				else
					cmdLine << ' ';

				cmdLine << '+' << cmd.first;

				if (!cmd.second.empty())
				{
					cmdLine << ' ';

					const bool needsQuotes = !std::all_of(cmd.second.begin(), cmd.second.end(), [](char c) { return isalpha(c) || isdigit(c); });
					if (needsQuotes)
						cmdLine << '"';

					cmdLine << cmd.second;

					if (needsQuotes)
						cmdLine << '"';
				}
			}

			// A simple command, we can pass this directly to the engine
			SendCommandToGame(cmdLine);
		}
		else
		{
			// More complicated, write out a file and exec it
			const std::string cfgFilename = ""s << ++m_LastUpdateIndex << ".cfg";
			auto globalPath = absolute_cfg_temp();
			std::filesystem::create_directories(globalPath);
			globalPath /= cfgFilename;
			{
				std::ofstream file(globalPath, std::ios_base::trunc);
				assert(file.good());

				for (const auto& cmd : writer.m_Commands)
					file << cmd.first << ' ' << cmd.second << '\n';

				assert(file.good());
			}

			SendCommandToGame("+exec "s << (tfbd_paths::local::cfg_temp() / cfgFilename).generic_string());
		}
	}

	m_LastUpdateTime = curTime;
}

bool ActionManager::SendCommandToGame(const std::string_view& cmd) const
{
	if (cmd.empty())
		return true;

	if (!FindWindowA("Valve001", nullptr))
	{
		Log("Attempted to send command \""s << cmd << "\" to game, but game is not running", { 1, 1, 0.8f });
		return false;
	}

	const std::filesystem::path hl2Dir = m_Settings->m_TFDir / "..";
	const std::filesystem::path hl2ExePath = hl2Dir / "hl2.exe";

	std::wstring cmdLine;
	cmdLine << hl2ExePath << " -game tf -hijack " << cmd;

	STARTUPINFOW si{};
	si.cb = sizeof(si);

	PROCESS_INFORMATION pi{};

	auto result = CreateProcessW(
		nullptr,             // application/module name
		cmdLine.data(),      // command line
		nullptr,             // process attributes
		nullptr,             // thread attributes
		FALSE,               // inherit handles
		IDLE_PRIORITY_CLASS, // creation flags
		nullptr,             // environment
		hl2Dir.c_str(),      // working directory
		&si,                 // STARTUPINFO
		&pi                  // PROCESS_INFORMATION
		);

	if (!result)
	{
		const auto error = GetLastError();
		std::string msg = "Failed to send command to hl2.exe: CreateProcess returned "s
			<< result << ", GetLastError returned " << error;
		throw std::runtime_error(msg);
	}

	//WaitForSingleObject(pi.hProcess, INFINITE);

	// We will never need these, close them now
	if (!CloseHandle(pi.hProcess))
		throw std::runtime_error(__FUNCTION__ ": Failed to close process");
	if (!CloseHandle(pi.hThread))
		throw std::runtime_error(__FUNCTION__ ": Failed to close process thread");

	return true;
}
