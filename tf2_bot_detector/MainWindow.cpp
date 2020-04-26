#include "MainWindow.h"
#include "ConsoleLines.h"

#include <cassert>
#include <chrono>
#include <regex>

using namespace tf2_bot_detector;

MainWindow::MainWindow() :
	ImGuiDesktop::Window(800, 600, "TF2 Bot Detector")
{
}

void MainWindow::OnDraw()
{
}

static std::regex s_TimestampRegex(R"regex(\n?(\d\d)\/(\d\d)\/(\d\d\d\d) - (\d\d):(\d\d):(\d\d): )regex", std::regex::optimize);
void MainWindow::OnUpdate()
{
	if (!m_File)
	{
		{
			FILE* temp;
			const auto error = fopen_s(&temp, "C:\\Program Files (x86)\\Steam\\steamapps\\common\\Team Fortress 2\\tf\\console.log", "r");
			m_File.reset(temp);
			assert(error == 0);
		}

		if (m_File)
			m_ConsoleLines.clear();
	}

	if (m_File)
	{
		char buf[4096];
		size_t readCount;
		using clock = std::chrono::steady_clock;
		const auto startTime = clock::now();
		bool consoleLinesUpdated = false;
		do
		{
			readCount = fread(buf, sizeof(buf[0]), std::size(buf), m_File.get());
			if (readCount > 0)
			{
				m_FileLineBuf.append(buf, readCount);

				auto regexBegin = m_FileLineBuf.cbegin();
				std::smatch match;
				while (std::regex_search(regexBegin, m_FileLineBuf.cend(), match, s_TimestampRegex))
				{
					if (m_CurrentTimestamp)
					{
						const auto prefix = match.prefix();
						const auto suffix = match.suffix();
						auto parsed = IConsoleLine::ParseConsoleLine(
							std::string_view(&*prefix.first, prefix.length()));

						if (parsed)
						{
							switch (parsed->GetType())
							{
							case ConsoleLineType::LobbyHeader:
							{
								auto headerLine = static_cast<const LobbyHeaderLine*>(parsed.get());
								const auto count = headerLine->GetMemberCount() + headerLine->GetPendingCount();
								m_CurrentLobbyMembers.resize(count);
								break;
							}

							case ConsoleLineType::LobbyMember:
							{
								auto memberLine = static_cast<const LobbyMemberLine*>(parsed.get());
								const auto& member = memberLine->GetLobbyMember();
								if (member.m_Index < m_CurrentLobbyMembers.size())
									m_CurrentLobbyMembers[member.m_Index] = member;

								break;
							}
							}

							m_ConsoleLines.push_back(std::move(parsed));
							consoleLinesUpdated = true;
						}
					}

					std::tm time{};
					from_chars_helper(match[1], time.tm_mon);
					time.tm_mon -= 1;

					from_chars_helper(match[2], time.tm_mday);
					from_chars_helper(match[3], time.tm_year);
					time.tm_year -= 1900;

					from_chars_helper(match[4], time.tm_hour);
					from_chars_helper(match[5], time.tm_min);
					from_chars_helper(match[6], time.tm_sec);

					m_CurrentTimestamp = time;
					regexBegin = match[0].second;
				}

				m_FileLineBuf.erase(m_FileLineBuf.begin(), regexBegin);
#if 0
				size_t startPos = 0;
				size_t newlinePos = 0;
				while ((newlinePos = m_FileLineBuf.find('\n', startPos)) != m_FileLineBuf.npos)
				{
					auto parsed = IConsoleLine::ParseConsoleLine(
						std::string_view(m_FileLineBuf).substr(startPos, newlinePos - startPos));



					startPos = newlinePos + 1;
				}

				m_FileLineBuf.erase(0, startPos);
#endif
			}

			//if (m_ConsoleLines.size() > 512)
			//	m_ConsoleLines.erase(m_ConsoleLines.begin(), m_ConsoleLines.begin() + (m_ConsoleLines.size() - 512));

			if (auto elapsed = clock::now() - startTime; elapsed > 10ms)
				break;

		} while (readCount > 0);

		if (readCount > 0)
			QueueUpdate();

		if (consoleLinesUpdated)
			UpdatePrintingLines();
	}
}
