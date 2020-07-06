#include "Config/ChatWrappers.h"
#include "ConsoleLogParser.h"
#include "ConsoleLines.h"
#include "Log.h"
#include "RegexHelpers.h"
#include "Config/Settings.h"
#include "WorldState.h"
#include "Platform/Platform.h"

#include <mh/future.hpp>
#include <mh/text/string_insertion.hpp>

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

ConsoleLogParser::ConsoleLogParser(WorldState& world, const Settings& settings, std::filesystem::path conLogFile) :
	m_Settings(&settings), m_WorldState(&world), m_FileName(std::move(conLogFile))
{
}

void ConsoleLogParser::Update()
{
	if (!m_File)
	{
		// Try to truncate
		{
			std::error_code ec;
			const auto filesize = std::filesystem::file_size(m_FileName, ec);
			if (!ec)
			{
				std::filesystem::resize_file(m_FileName, 0, ec);
				if (ec)
					Log("Unable to truncate console log file, current size is "s << filesize);
				else
					Log("Truncated console log file");
			}
		}

		{
			FILE* temp = _fsopen(m_FileName.string().c_str(), "r", _SH_DENYNO);
			m_File.reset(temp);
		}
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
	{
		for (IConsoleLineListener* listener : m_WorldState->m_ConsoleLineListeners)
			listener->OnConsoleLogChunkParsed(*m_WorldState, consoleLinesUpdated);
	}
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

			auto parseEnd = m_FileLineBuf.cbegin();
			ParseChunk(parseEnd, linesProcessed, snapshotUpdated, consoleLinesUpdated);

			m_ParsedLineCount += std::count(m_FileLineBuf.cbegin(), parseEnd, '\n');
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
		if (lineStr.starts_with(type.m_Full.m_Start))
		{
			auto searchBuf = std::string_view(m_FileLineBuf).substr(
				&*lineStr.begin() - m_FileLineBuf.data() + type.m_Full.m_Start.size());

			if (auto found = searchBuf.find(type.m_Full.m_End); found != lineStr.npos)
			{
				if (found > 512)
				{
					LogError("Searched more than 512 characters ("s << found
						<< ") for the end of the chat msg string, something is terribly wrong!");
				}

				searchBuf = searchBuf.substr(0, found);

				auto nameBegin = searchBuf.find(type.m_Name.m_Start);
				auto nameEnd = searchBuf.find(type.m_Name.m_End);
				auto msgBegin = searchBuf.find(type.m_Message.m_Start);
				auto msgEnd = searchBuf.find(type.m_Message.m_End);

				if (nameBegin != searchBuf.npos && nameEnd != searchBuf.npos && msgBegin != searchBuf.npos && msgEnd != searchBuf.npos)
				{
					parsed = std::make_unique<ChatConsoleLine>(m_WorldState->GetCurrentTime(),
						std::string(searchBuf.substr(nameBegin + type.m_Name.m_Start.size(), nameEnd - nameBegin - type.m_Name.m_Start.size())),
						std::string(searchBuf.substr(msgBegin + type.m_Message.m_Start.size(), msgEnd - msgBegin - type.m_Message.m_Start.size())),
						IsDead(category), IsTeam(category));
				}
				else
				{
					if (nameBegin == searchBuf.npos)
						LogError("Failed to find name begin sequence in chat message of type "s << category);
					if (nameEnd == searchBuf.npos)
						LogError("Failed to find name end sequence in chat message of type "s << category);
					if (msgBegin == searchBuf.npos)
						LogError("Failed to find message begin sequence in chat message of type "s << category);
					if (msgEnd == searchBuf.npos)
						LogError("Failed to find message end sequence in chat message of type "s << category);
				}

				parseEnd += type.m_Full.m_Start.size() + found + type.m_Full.m_End.size();
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
					for (auto listener : m_WorldState->m_ConsoleLineListeners)
						listener->OnConsoleLineParsed(*m_WorldState, *parsed);

					consoleLinesUpdated = true;
				}
			}
			else
			{
				for (auto listener : m_WorldState->m_ConsoleLineListeners)
					listener->OnConsoleLineUnparsed(*m_WorldState, lineStr);
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
