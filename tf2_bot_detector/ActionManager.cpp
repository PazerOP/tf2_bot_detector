#include "ActionManager.h"
#include "Actions.h"
#include "ConsoleLines.h"
#include "Log.h"

#include <mh/text/string_insertion.hpp>

#include <filesystem>
#include <fstream>
#include <regex>

#define WIN32_LEAN_AND_MEAN 1
#include <Windows.h>

#undef min
#undef max

static const std::regex s_SingleCommandRegex(R"regex(([a-zA-Z_]+)(?:\n)?)regex", std::regex::optimize);

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
	namespace absolute
	{
		static std::filesystem::path root()
		{
			return "C:/Program Files (x86)/Steam/steamapps/common/Team Fortress 2/tf/custom/tf2_bot_detector/";
		}
		static std::filesystem::path cfg()
		{
			return root() / "cfg" / local::cfg();
		}
		static std::filesystem::path cfg_temp()
		{
			return root() / "cfg" / local::cfg_temp();
		}
	}
}

ActionManager::ActionManager()
{
	std::filesystem::remove_all(tfbd_paths::absolute::cfg_temp());
}

ActionManager::~ActionManager()
{
}

bool ActionManager::QueueAction(std::unique_ptr<IAction>&& action)
{
#if 0
	static uint32_t s_LastAction = 0;
	const uint32_t curAction = ++s_LastAction;
	{
		std::ofstream file(s_UpdateCFGPath / (""s << curAction << ".cfg"), std::ios_base::trunc);
		action->WriteCommands(file);
	}
	SendCommandToGame("+exec tf2_bot_detector/temp/"s << curAction << ".cfg");
#endif

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

void ActionManager::Update()
{
	const auto now = clock_t::now();
	if (now < (m_LastUpdateTime + UPDATE_INTERVAL))
		return;

#if 0
	struct FileDeleter
	{
		void operator()(FILE* f) const { fclose(f); }
	};

	std::unique_ptr<FILE, FileDeleter> file(_wfsopen(s_UpdateCFGPath.c_str(), L"w", _SH_DENYRW));
	if (!file)
		return;

	if (!m_Actions.empty())
	{
		std::string fileContents;
		mh::strwrapperstream stream(fileContents);

		bool actionTypes[(int)ActionType::COUNT]{};

		for (const auto& action : m_Actions)
		{
			{
				const ActionType type = action.second->GetType();
				auto& previousMsg = actionTypes[(int)type];
				const auto minInterval = action.second->GetMinInterval();
				if (previousMsg && minInterval.count() > 0 && (m_CurrentTime - m_LastTriggerTime[type]) >= action.second->GetMinInterval())
					continue;
				else
					previousMsg = true;
			}

			action.second->WriteCommands(stream);
			stream << "tfbd_update_count " << action.first << '\n';
			stream << "cvarlist tfbd_update_count\n";
		}

		const auto count = fwrite(fileContents.data(), fileContents.size(), 1, file.get());
		assert(count == 1);
		if (count != 1)
			throw std::runtime_error("Failed to write update.cfg, even though we should have already guarenteed exclusive access");
	}
#else
	if (!m_Actions.empty())
	{
		bool actionTypes[(int)ActionType::COUNT]{};

		std::string fileContents;
		mh::strwrapperstream fileContentsStream(fileContents);

		for (auto it = m_Actions.begin(); it != m_Actions.end(); )
		{
			const IAction* action = it->get();
			const ActionType type = action->GetType();
			{
				auto& previousMsg = actionTypes[(int)type];
				const auto minInterval = action->GetMinInterval();
				if (previousMsg && minInterval.count() > 0 && (m_CurrentTime - m_LastTriggerTime[type]) < action->GetMinInterval())
				{
					++it;
					continue;
				}
				else
				{
					previousMsg = true;
				}
			}

			action->WriteCommands(fileContentsStream);
			it = m_Actions.erase(it);
			m_LastTriggerTime[type] = m_CurrentTime;
		}

		if (std::smatch match; std::regex_match(fileContents, match, s_SingleCommandRegex))
		{
			// A simple command, we can pass this directly to the engine
			SendCommandToGame("+"s << match[1]);
		}
		else
		{
			// More complicated, write out a file and exec it
			const std::string cfgFilename = ""s << ++m_LastUpdateIndex << ".cfg";
			auto globalPath = tfbd_paths::absolute::cfg_temp();
			std::filesystem::create_directories(globalPath); // TODO: this is not ok for release
			globalPath /= cfgFilename;
			{
				std::ofstream file(globalPath, std::ios_base::trunc);
				assert(file.good());
				file << fileContents;
				assert(file.good());
			}

			SendCommandToGame("+exec "s << (tfbd_paths::local::cfg_temp() / cfgFilename).generic_string());
		}
	}
#endif

	m_LastUpdateTime = now;
}

void ActionManager::OnConsoleLineParsed(IConsoleLine& line)
{
	m_CurrentTime = line.GetTimestamp();

#if 0
	if (line.GetType() == ConsoleLineType::CvarlistConvar)
	{
		const auto& cvar = static_cast<const CvarlistConvarLine&>(line);
		if (cvar.GetConvarName() == "tfbd_update_count"sv)
		{
			const auto value = static_cast<uint32_t>(cvar.GetConvarValue());
			m_LastUpdateIndex = std::max(m_LastUpdateIndex, value);
			const auto upperBound = m_Actions.upper_bound(value);

			for (auto it = m_Actions.begin(); it != upperBound; ++it)
				m_LastTriggerTime[it->second->GetType()] = m_CurrentTime;

			m_Actions.erase(m_Actions.begin(), upperBound);
		}
	}
#endif
}

void ActionManager::OnConsoleLineUnparsed(time_point_t timestamp, const std::string_view& text)
{
	m_CurrentTime = timestamp;
}

void ActionManager::SendCommandToGame(const std::string_view& cmd)
{
	if (!FindWindowA("Valve001", nullptr))
	{
		Log("Attempted to send command \""s << cmd << "\" to game, but game is not running", { 1, 1, 0.8f });
		return;
	}

	static constexpr const char HL2_DIR[] = "C:\\Program Files (x86)\\Steam\\steamapps\\common\\Team Fortress 2\\";

	std::string cmdLine;
	cmdLine << '"' << HL2_DIR << "hl2.exe\" -game tf -hijack " << cmd;

	STARTUPINFOA si{};
	si.cb = sizeof(si);

	PROCESS_INFORMATION pi{};

	auto result = CreateProcessA(
		nullptr,             // application/module name
		cmdLine.data(),      // command line
		nullptr,             // process attributes
		nullptr,             // thread attributes
		FALSE,               // inherit handles
		IDLE_PRIORITY_CLASS, // creation flags
		nullptr,             // environment
		HL2_DIR,             // working directory
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
}
