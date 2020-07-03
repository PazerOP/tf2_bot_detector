#ifdef _WIN32
#include "HijackActionManager.h"
#include "Actions.h"
#include "ICommandSource.h"
#include "Log.h"
#include "Config/Settings.h"

#include <mh/text/insertion_conversion.hpp>
#include <mh/text/string_insertion.hpp>

#include <cassert>
#include <fstream>

#include <Windows.h>

using namespace std::chrono_literals;
using namespace std::string_literals;
using namespace tf2_bot_detector;

struct HijackActionManager::RunningCommand
{
	RunningCommand(HANDLE procHandle, std::string command) : m_ProcHandle(procHandle), m_Command(command) {}
	RunningCommand(const RunningCommand&) = delete;
	RunningCommand& operator=(const RunningCommand&) = delete;
	~RunningCommand()
	{
		if (!CloseHandle(m_ProcHandle))
			LogError(std::string(__FUNCTION__ ": Failed to close handle for ") << std::quoted(m_Command));
	}

	duration_t GetElapsed() const { return clock_t::now() - m_StartTime; }
	bool IsComplete() const
	{
		return WaitForSingleObject(m_ProcHandle, 0) == WAIT_OBJECT_0;
	}

	void Terminate()
	{
		if (!TerminateProcess(m_ProcHandle, 1))
		{
			const auto error = GetLastError();
			LogError("Failed to terminate stuck hl2.exe process ("s << std::quoted(m_Command)
				<< "): GetLastError returned "s << error << ": " << std::error_code(error, std::system_category()).message());
		}
	}

	HANDLE m_ProcHandle;
	std::string m_Command;

private:
	time_point_t m_StartTime = clock_t::now();
};

struct HijackActionManager::Writer final : ICommandWriter
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

void HijackActionManager::ProcessRunningCommands()
{
	for (auto it = m_RunningCommands.begin(); it != m_RunningCommands.end();)
	{
		bool shouldRemove = false;
		if (it->IsComplete())
		{
			shouldRemove = true;
		}
		else if (it->GetElapsed() > 5s)
		{
			LogWarning("Command timed out: "s << std::quoted(it->m_Command));
			it->Terminate();
			shouldRemove = true;
		}

		if (shouldRemove)
			it = m_RunningCommands.erase(it);
		else
			++it;
	}
}

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

auto HijackActionManager::absolute_root() const
{
	return m_Settings->GetTFDir();// / "custom/tf2_bot_detector";
}
auto HijackActionManager::absolute_cfg() const
{
	return absolute_root() / "cfg" / tfbd_paths::local::cfg();
}
auto HijackActionManager::absolute_cfg_temp() const
{
	return absolute_root() / "cfg" / tfbd_paths::local::cfg_temp();
}

HijackActionManager::HijackActionManager(const Settings& settings) :
	m_Settings(&settings)
{
	std::filesystem::remove_all(absolute_cfg_temp());
}

HijackActionManager::~HijackActionManager()
{
}

bool HijackActionManager::RunCommand(std::string cmd, std::string args)
{
	Writer writer;
	writer.Write(std::move(cmd), std::move(args));
	return SendHijackCommands(writer);
}

void HijackActionManager::Update()
{
	ProcessRunningCommands();
}

bool HijackActionManager::ProcessSimpleCommands(const Writer& writer)
{
	assert(!writer.m_ComplexCommands);

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
	return SendHijackCommand(cmdLine);
}

bool HijackActionManager::ProcessComplexCommands(const Writer& writer)
{
	// More complicated, exec commands from a cfg file
	assert(writer.m_ComplexCommands);

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

	return SendHijackCommand("+exec "s << (tfbd_paths::local::cfg_temp() / cfgFilename).generic_string());
}

bool HijackActionManager::SendHijackCommands(const Writer& writer)
{
	// Is this a "simple" command, aka nothing to confuse the engine/OS CLI arg parser?
	if (!writer.m_ComplexCommands && writer.m_CommandArgCount < 200)
		return ProcessSimpleCommands(writer);
	else
		return ProcessComplexCommands(writer);
}

bool HijackActionManager::SendHijackCommand(std::string cmd)
{
	if (cmd.empty())
		return true;

	if (!FindWindowA("Valve001", nullptr))
	{
		DebugLogWarning("Attempted to send command \""s << cmd << "\" to game, but game is not running");
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
		LogError(__FUNCTION__ ": Failed to close process thread");

	return true;
}
#endif
