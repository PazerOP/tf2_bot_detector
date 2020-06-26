#include "ActionManager.h"
#include "Actions.h"
#include "Config/Settings.h"
#include "ConsoleLines.h"
#include "Log.h"
#include "Actions/ActionGenerators.h"
#include "WorldState.h"

#include <mh/text/insertion_conversion.hpp>
#include <mh/text/string_insertion.hpp>

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <regex>

#ifdef _WIN32
#include <WinSock2.h>
#else
#include <unistd.h>
#endif

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
	return m_Settings->GetTFDir();// / "custom/tf2_bot_detector";
}
auto ActionManager::absolute_cfg() const
{
	return absolute_root() / "cfg" / tfbd_paths::local::cfg();
}
auto ActionManager::absolute_cfg_temp() const
{
	return absolute_root() / "cfg" / tfbd_paths::local::cfg_temp();
}

static std::string GetHostName()
{
	// Same basic logic that the source engine uses to figure out what ip to bind to
	// (attempting to connect to rcon at 127.0.0.1 doesn't work)
	char buf[512];
	if (auto result = gethostname(buf, sizeof(buf)); result < 0)
		throw std::runtime_error(std::string(__FUNCTION__) << "(): gethostname() returned " << result);

	buf[sizeof(buf) - 1] = 0;

	auto host = gethostbyname(buf);
	if (!host)
		throw std::runtime_error(std::string(__FUNCTION__) << "(): gethostbyname returned nullptr");

	in_addr addr{};
	addr.s_addr = *(u_long*)host->h_addr_list[0];

	std::string retVal;
	if (auto addrStr = inet_ntoa(addr); addrStr && addrStr[0])
		retVal = addrStr;
	else
		throw std::runtime_error(std::string(__FUNCTION__) << "(): inet_ntoa returned null or empty string");

	Log(std::string(__FUNCTION__) << "(): ip = " << retVal);
	return retVal;
}

ActionManager::ActionManager(const Settings& settings) :
	m_Settings(&settings)
{
	try
	{
		m_RCONClient.connect(GetHostName(), "");
	}
	catch (const std::exception& e)
	{
		DebugLogWarning("Failed initial RCON connection attempt: "s << e.what());
	}

	std::filesystem::remove_all(absolute_cfg_temp());

	m_RCONThread = std::thread(&ActionManager::RCONThreadFunc, this, m_RCONCancellationSource.token());
}

ActionManager::~ActionManager()
{
	assert(m_RCONCancellationSource.can_be_cancelled());
	m_RCONCancellationSource.request_cancellation();
	m_RCONThread.join();
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

void ActionManager::AddPeriodicActionGenerator(std::unique_ptr<IPeriodicActionGenerator>&& action)
{
	m_PeriodicActionGenerators.push_back(std::move(action));
}

void ActionManager::AddPiggybackActionGenerator(std::unique_ptr<IActionGenerator>&& action)
{
	m_PiggybackActionGenerators.push_back(std::move(action));
}

struct ActionManager::Writer final : ICommandWriter
{
	void Write(std::string cmd, std::string args) override
	{
		assert(!cmd.empty());

		if (cmd.empty() && !args.empty())
			throw std::runtime_error("Empty command with non-empty args");

		if (!std::all_of(cmd.begin(), cmd.end(), [](char c) { return c == '_' || isalpha(c) || isdigit(c); }))
			throw std::runtime_error("Command contains invalid characters");

		if (!m_ComplexCommands && !std::all_of(args.begin(), args.end(),
			[](char c) { return isalpha(c) || isdigit(c) || isspace(c) || c == '.' || c == '_'; }))
		{
			m_ComplexCommands = true;
		}

		m_CommandArgCount++;
		if (!args.empty())
			m_CommandArgCount++;

		m_Commands.push_back({ std::move(cmd), std::move(args) });
	}

	std::vector<std::pair<std::string, std::string>> m_Commands;
	bool m_ComplexCommands = false;
	size_t m_CommandArgCount = 0;
};

ActionManager::RCONCommand::RCONCommand(std::string cmd) :
	m_Command(std::move(cmd)),
	m_Promise(std::make_shared<std::promise<std::string>>()),
	m_Future(m_Promise->get_future())
{
}

void ActionManager::RCONThreadFunc(cppcoro::cancellation_token cancellationToken)
{
	while (!cancellationToken.is_cancellation_requested())
	{
		std::this_thread::sleep_for(250ms);

		while (!m_RCONCommands.empty() && !cancellationToken.is_cancellation_requested())
		{
			std::optional<RCONCommand> cmd;
			{
				std::lock_guard lock(m_RCONCommandsMutex);
				if (m_RCONCommands.empty())
					break;

				cmd = m_RCONCommands.front();
			}

			try
			{
				const auto startTime = clock_t::now();

				auto resultStr = RunCommand(cmd->m_Command);
				//DebugLog("Setting promise for "s << std::quoted(cmd->m_Command) << " to " << std::quoted(resultStr));
				cmd->m_Promise->set_value(resultStr);
#if 0
				if (auto waitResult = result.wait_for(7s); waitResult == std::future_status::timeout)
				{
					LogWarning("SRCON reconnecting after timing out waiting for game command "s << std::quoted(cmd));
					std::lock_guard lock(m_RCONClientMutex);
					m_RCONClient.reconnect();
					return false;
				}
				const auto resultStr = result.get();
#endif

				if (m_Settings->m_Unsaved.m_DebugShowCommands)
				{
					const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(clock_t::now() - startTime);
					std::string msg = "Game command processed in "s << elapsed.count() << "ms : " << std::quoted(cmd->m_Command);

					if (!resultStr.empty())
						msg << ", response " << resultStr.size() << " bytes";

					Log(std::move(msg), { 1, 1, 1, 0.6f });
				}

				if (!resultStr.empty())
				{
					if (m_WorldState)
					{
						m_WorldState->AddConsoleOutputChunk(resultStr);
					}
					else
					{
						LogError("WorldState was nullptr when we tried to give it the result: "s << resultStr);
					}
				}

				{
					std::lock_guard lock(m_RCONCommandsMutex);
					assert(!m_RCONCommands.empty());
					if (!m_RCONCommands.empty())
					{
						assert(m_RCONCommands.front() == cmd);
						m_RCONCommands.pop();
					}
				}
			}
			catch (const std::exception& e)
			{
				LogError(std::string(__FUNCTION__) << "(): Unhandled exception: " << e.what());
				std::this_thread::sleep_for(1s);

				try
				{
					std::lock_guard lock(m_RCONClientMutex);
					m_RCONClient.reconnect();
				}
				catch (const std::exception& e2)
				{
					LogError(std::string(__FUNCTION__) << "(): Failed to reconnect after exception: " << e2.what());
					std::this_thread::sleep_for(1s);
				}
			}
		}
	}
}

bool ActionManager::ProcessSimpleCommands(const Writer& writer)
{
	std::string cmdLine;
	bool firstCmd = true;

	for (const auto& cmd : writer.m_Commands)
	{
		if (firstCmd)
			firstCmd = false;
		else
			cmdLine << ' ';

		if (!SendCommandToGame(cmd.first + " " + cmd.second))
			return false;

		//cmdLine << '+' << cmd.first;

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
	//return SendCommandToGame(cmdLine);
	return false;
}

bool ActionManager::ProcessComplexCommands(const Writer& writer)
{
	// More complicated, exec commands from a cfg file

	std::string cfgFileContents;
	for (const auto& cmd : writer.m_Commands)
		cfgFileContents << cmd.first << ' ' << cmd.second << '\n';

	const auto globalPath = absolute_cfg_temp();
	std::filesystem::create_directories(globalPath);

	const auto hash = std::hash<std::string>{}(cfgFileContents);
	std::string cfgFilename;
	{
		auto found = m_TempCfgFiles.equal_range(hash);
		for (auto it = found.first; it != found.second; ++it)
		{
			// Make sure file contents are identical
			std::string localCfgFilename = ""s << it->second << ".cfg";
			std::filesystem::path path = globalPath / localCfgFilename;

			std::ifstream file(path);
			size_t comparePos = 0;
			while (true)
			{
				char buf[4096];
				file.read(buf, std::size(buf));
				if (file.bad())
					break;

				const auto bufCount = static_cast<size_t>(file.gcount());
				if (bufCount == 0)
					break;

				if (cfgFileContents.compare(comparePos, bufCount, buf, bufCount))
				{
					// Not equal
					break;
				}

				comparePos += bufCount;
			}

			if (comparePos == cfgFileContents.size())
			{
				cfgFilename = std::move(localCfgFilename);
				break;
			}
		}
	}

	if (cfgFilename.empty())
	{
		// Create a new temp cfg file
		cfgFilename = ""s << ++m_LastUpdateIndex << ".cfg";
		//Log("Creating new temp cfg file "s << std::quoted(cfgFilename));

		std::ofstream file(globalPath / cfgFilename, std::ios_base::trunc);
		assert(file.good());
		file << cfgFileContents;
		assert(file.good());

		m_TempCfgFiles.emplace(std::hash<std::string>{}(cfgFileContents), m_LastUpdateIndex);
	}

	return SendCommandToGame("+exec "s << (tfbd_paths::local::cfg_temp() / cfgFilename).generic_string());
}

void ActionManager::Update()
{
	const auto curTime = clock_t::now();
	if (curTime < (m_LastUpdateTime + UPDATE_INTERVAL))
		return;

	// Update periodic actions
	for (const auto& generator : m_PeriodicActionGenerators)
		generator->Execute(*this);

	if (!m_Actions.empty())
	{
		bool actionTypes[(int)ActionType::COUNT]{};

		Writer writer;

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

		const auto ProcessActions = [&]()
		{
			for (auto it = m_Actions.begin(); it != m_Actions.end(); )
			{
				const IAction* action = it->get();
				if (ProcessAction(it->get()))
					it = m_Actions.erase(it);
				else
					++it;
			}
		};

		// Handle normal actions
		ProcessActions();

		if (!writer.m_Commands.empty())
		{
			// Handle piggyback commands
			for (const auto& generator : m_PiggybackActionGenerators)
				generator->Execute(*this);

			// Process any actions added by piggyback action generators
			ProcessActions();
		}

#if 0
		// Is this a "simple" command, aka nothing to confuse the engine/OS CLI arg parser?
		if (!writer.m_ComplexCommands && writer.m_CommandArgCount < 200)
		{
			ProcessSimpleCommands(writer);
		}
		else
		{
			ProcessComplexCommands(writer);
		}
#else
		for (const auto& cmd : writer.m_Commands)
		{
			if (cmd.second.empty())
				SendCommandToGame(cmd.first);
			else
				SendCommandToGame(cmd.first + " " + cmd.second);
		}
#endif
	}

	m_LastUpdateTime = curTime;
}

std::string ActionManager::RunCommand(std::string cmd)
{
	std::unique_lock lock(m_RCONClientMutex, 5s);
	if (!lock.owns_lock())
		throw std::runtime_error("Failed to acquire rcon client mutex");

	if (!m_RCONClient.is_connected())
	{
		DebugLog(std::string(__FUNCTION__) << "(): SRCON not connected, reconnecting for command " << std::quoted(cmd));
		m_RCONClient.reconnect();
	}

	return m_RCONClient.send(cmd);
}

std::shared_future<std::string> ActionManager::RunCommandAsync(std::string cmd)
{
	std::lock_guard lock(m_RCONCommandsMutex);
	return m_RCONCommands.emplace(std::move(cmd)).m_Future;
}

bool ActionManager::SendCommandToGame(std::string cmd)
{
#if 0
	if (cmd.empty())
		return true;

	if (!FindWindowA("Valve001", nullptr))
	{
		Log("Attempted to send command \""s << cmd << "\" to game, but game is not running", { 1, 1, 0.8f });
		return false;
	}

	const std::filesystem::path hl2Dir = m_Settings->GetTFDir() / "..";
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
		LogError("Failed to send command to hl2.exe: CreateProcess returned "s
			<< result << ", GetLastError returned " << error << ": " << std::error_code(error, std::system_category()).message());

		return false;
	}

	if (m_Settings->m_Unsaved.m_DebugShowCommands)
		Log("Game command: "s << std::quoted(cmd), { 1, 1, 1, 0.6f });

	m_RunningCommands.emplace_back(pi.hProcess, std::move(cmd));

	if (!CloseHandle(pi.hThread))
		throw std::runtime_error(__FUNCTION__ ": Failed to close process thread");

	return true;
#else
	RunCommandAsync(std::move(cmd));
	return true;
#endif
}

ActionManager::InitSRCON::InitSRCON()
{
	srcon::SetLogFunc([](std::string&& msg)
		{
			DebugLog("[SRCON] "s << std::move(msg));
		});
}
