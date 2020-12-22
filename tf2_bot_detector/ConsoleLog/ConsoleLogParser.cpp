#include "ConsoleLogParser.h"
#include "Config/ChatWrappers.h"
#include "ConsoleLog/ConsoleLineListener.h"
#include "ConsoleLines.h"
#include "Log.h"
#include "Util/RegexUtils.h"
#include "Config/Settings.h"
#include "WorldState.h"
#include "Platform/Platform.h"

#include <mh/text/format.hpp>
#include <mh/text/formatters/error_code.hpp>
#include <mh/future.hpp>

#include <regex>

using namespace std::chrono_literals;
using namespace std::string_literals;
using namespace tf2_bot_detector;

void ConsoleLogParser::TrySnapshot(bool& snapshotUpdated)
{
	if ((!snapshotUpdated || !m_CurrentTimestamp.IsSnapshotValid()) && m_CurrentTimestamp.IsRecordedValid())
	{
		m_CurrentTimestamp.Snapshot();
		snapshotUpdated = true;
		m_WorldState->UpdateTimestamp(*this);
	}
}

ConsoleLogParser::ConsoleLogParser(IWorldState& world, const Settings& settings, std::filesystem::path conLogFile) :
	m_Settings(&settings), m_WorldState(&world), m_FileName(std::move(conLogFile))
{
}

void ConsoleLogParser::Update()
{
	const auto now = clock_t::now();
	if (!m_File && (now - m_LastFileLoadAttempt) > 1s)
	{
		m_LastFileLoadAttempt = now;

		// Try to truncate
		{
			std::error_code ec;
			const auto filesize = std::filesystem::file_size(m_FileName, ec);
			if (ec)
				LogWarning("Failed to get size of {}: {}", m_FileName, ec);
			else if (std::filesystem::resize_file(m_FileName, 0, ec); ec)
				Log("Unable to truncate {}, current size is {}", m_FileName, filesize);
			else
				Log("Truncated console log file");
		}

		std::error_code ec;
		{
			FILE* temp = _wfsopen(m_FileName.c_str(), L"r", _SH_DENYNO);
			if (!temp)
			{
				auto e = errno;
				ec = std::error_code(e, std::generic_category());
			}
			m_File.reset(temp);
		}

		if (!m_File)
			DebugLog("Failed to open {}: {}", m_FileName, ec);
		else
			Log("Successfully opened {}", m_FileName);
	}

	bool snapshotUpdated = false;

	bool linesProcessed = false;
	bool consoleLinesUpdated = false;
	if (m_File)
	{
		Parse(linesProcessed, snapshotUpdated, consoleLinesUpdated);

		// Parse progress
		{
			const auto pos = ftell(m_File.get());
			const auto length = std::filesystem::file_size(m_FileName);
			m_ParseProgress = float(double(pos) / length);
		}
	}

	TrySnapshot(snapshotUpdated);

	if (linesProcessed)
		m_WorldState->GetConsoleLineListenerBroadcaster().OnConsoleLogChunkParsed(*m_WorldState, consoleLinesUpdated);
}

void ConsoleLogParser::CustomDeleters::operator()(FILE* f) const
{
	fclose(f);
}

void ConsoleLogParser::Parse(bool& linesProcessed, bool& snapshotUpdated, bool& consoleLinesUpdated)
{
	char buf[4096];
	size_t readCount;
	using clock = std::chrono::steady_clock;
	const auto startTime = clock::now();
	do
	{
		readCount = fread(buf, sizeof(buf[0]), std::size(buf), m_File.get());
		if (readCount > 0)
		{
			m_FileLineBuf.append(buf, readCount);
			ILogManager::GetInstance().LogConsoleOutput(std::string_view(buf, readCount));

			auto parseEnd = m_FileLineBuf.cbegin();
			ParseChunk(parseEnd, linesProcessed, snapshotUpdated, consoleLinesUpdated);

			m_FileLineBuf.erase(m_FileLineBuf.begin(), parseEnd);
		}

		if (auto elapsed = clock::now() - startTime; elapsed >= 50ms)
			break;

	} while (readCount > 0);
}

bool ConsoleLogParser::ParseChatMessage(const std::string_view& lineStr, striter& parseEnd, std::shared_ptr<IConsoleLine>& parsed)
{
	for (int i = 0; i < (int)ChatCategory::COUNT; i++)
	{
		const auto category = ChatCategory(i);

		auto& type = m_Settings->m_Unsaved.m_ChatMsgWrappers.value().m_Types[i];
		if (lineStr.starts_with(type.m_Full.m_Start.m_Narrow))
		{
			auto searchBuf = std::string_view(m_FileLineBuf).substr(
				&*lineStr.begin() - m_FileLineBuf.data() + type.m_Full.m_Start.m_Narrow.size());

			if (auto found = searchBuf.find(type.m_Full.m_End.m_Narrow); found != lineStr.npos)
			{
				if (found > 512)
				{
					LogError("Searched more than 512 characters ({}) for the end of the chat msg string, something is terribly wrong!", found);
				}

				searchBuf = searchBuf.substr(0, found);

				auto nameBegin = searchBuf.find(type.m_Name.m_Start.m_Narrow);
				auto nameEnd = searchBuf.find(type.m_Name.m_End.m_Narrow);
				auto msgBegin = searchBuf.find(type.m_Message.m_Start.m_Narrow);
				auto msgEnd = searchBuf.find(type.m_Message.m_End.m_Narrow);

				if (nameBegin != searchBuf.npos && nameEnd != searchBuf.npos && msgBegin != searchBuf.npos && msgEnd != searchBuf.npos)
				{
					const auto name = searchBuf.substr(
						nameBegin + type.m_Name.m_Start.m_Narrow.size(),
						nameEnd - nameBegin - type.m_Name.m_Start.m_Narrow.size());

					const auto msg = searchBuf.substr(
						msgBegin + type.m_Message.m_Start.m_Narrow.size(),
						msgEnd - msgBegin - type.m_Message.m_Start.m_Narrow.size());

					TeamShareResult teamShareResult = TeamShareResult::Neither;
					bool isSelf = false;
					if (auto player = m_WorldState->FindSteamIDForName(name))
					{
						teamShareResult = m_WorldState->GetTeamShareResult(*player);
						isSelf = (player == m_Settings->GetLocalSteamID());
					}

					parsed = std::make_unique<ChatConsoleLine>(m_WorldState->GetCurrentTime(),
						std::string(name), std::string(msg), IsDead(category), IsTeam(category), isSelf, teamShareResult);
				}
				else
				{
					if (nameBegin == searchBuf.npos)
						LogError("Failed to find name begin sequence in chat message of type {}", mh::enum_fmt(category));
					if (nameEnd == searchBuf.npos)
						LogError("Failed to find name end sequence in chat message of type {}", mh::enum_fmt(category));
					if (msgBegin == searchBuf.npos)
						LogError("Failed to find message begin sequence in chat message of type {}", mh::enum_fmt(category));
					if (msgEnd == searchBuf.npos)
						LogError("Failed to find message end sequence in chat message of type {}", mh::enum_fmt(category));
				}

				parseEnd += type.m_Full.m_Start.m_Narrow.size() + found + type.m_Full.m_End.m_Narrow.size();
				return true;
			}
			else
			{
				LogError("Failed to locate chat message wrapper end");
				return false; // Not enough characters in m_FileLineBuf. Try again later.
			}
		}
	}

	return true;
}

void ConsoleLogParser::ParseChunk(striter& parseEnd, bool& linesProcessed, bool& snapshotUpdated, bool& consoleLinesUpdated)
{
	static const std::regex s_TimestampRegex(R"regex(\n(\d\d)\/(\d\d)\/(\d\d\d\d) - (\d\d):(\d\d):(\d\d):[ \n])regex", std::regex::optimize);

	std::smatch match;
	while (std::regex_search(parseEnd, m_FileLineBuf.cend(), match, s_TimestampRegex))
	{
		auto regexBegin = parseEnd;

		ParseLineResult result = ParseLineResult::Unparsed;
		bool skipTimestampParse = false;
		if (m_CurrentTimestamp.IsRecordedValid())
		{
			// If we have a valid snapshot, that means that there was a previously parsed
			// timestamp. The contents of that line is the current timestamp match's prefix
			// (the previous timestamp match was erased from the string)

			TrySnapshot(snapshotUpdated);
			linesProcessed = true;

			std::shared_ptr<IConsoleLine> parsed;

			const auto prefix = match.prefix();
			//const auto suffix = match.suffix();
			const std::string_view lineStr(&*prefix.first, prefix.length());

			if (ParseChatMessage(lineStr, regexBegin, parsed))
			{
				if (parsed)
					result = ParseLineResult::Modified;
			}
			else
			{
				return; // Try again later (not enough chars in buffer)
			}

			if (!parsed && result == ParseLineResult::Unparsed)
			{
				parsed = IConsoleLine::ParseConsoleLine(lineStr, m_CurrentTimestamp.GetSnapshot());
				if (parsed && parsed->GetType() == ConsoleLineType::Chat)
					LogError("Line was parsed as a chat message via old code path, this should never happen!");

				result = ParseLineResult::Success;
			}

			if (parsed)
			{
				if (result == ParseLineResult::Success || result == ParseLineResult::Modified)
				{
					m_WorldState->GetConsoleLineListenerBroadcaster().OnConsoleLineParsed(*m_WorldState, *parsed);
					consoleLinesUpdated = true;
				}
			}
			else
			{
				m_WorldState->GetConsoleLineListenerBroadcaster().OnConsoleLineUnparsed(*m_WorldState, lineStr);
			}
		}

		if (result != ParseLineResult::Modified)
		{
			std::tm time{};
			time.tm_isdst = -1;
			from_chars_throw(match[1], time.tm_mon);
			time.tm_mon -= 1;

			from_chars_throw(match[2], time.tm_mday);
			from_chars_throw(match[3], time.tm_year);
			time.tm_year -= 1900;

			from_chars_throw(match[4], time.tm_hour);
			from_chars_throw(match[5], time.tm_min);
			from_chars_throw(match[6], time.tm_sec);

			m_CurrentTimestamp.SetRecorded(clock_t::from_time_t(std::mktime(&time)));
			regexBegin = match[0].second;
		}
		else
		{
			m_CurrentTimestamp.InvalidateRecorded();
		}

		parseEnd = regexBegin;
	}
}
