#include "MainWindow.h"
#include "ConsoleLines.h"
#include "RegexCharConv.h"

#include <imgui.h>

#include <cassert>
#include <chrono>
#include <regex>

using namespace tf2_bot_detector;
using namespace std::chrono_literals;

MainWindow::MainWindow() :
	ImGuiDesktop::Window(800, 600, "TF2 Bot Detector")
{
	m_OpenTime = std::chrono::steady_clock::now();
}

MainWindow::~MainWindow()
{
}

void MainWindow::OnDraw()
{
	ImGui::Text("Here's some window bruh");

	ImGui::NewLine();
	ImGui::Text("Current application time: %1.2f", std::chrono::duration<float>(std::chrono::steady_clock::now() - m_OpenTime).count());
	ImGui::NewLine();
	if (ImGui::Button("Quit"))
		SetShouldClose(true);

	//ImGui::InputTextMultiline("##fileContents", const_cast<char*>(m_FileText.data()), m_FileText.size(), { -1, -1 }, ImGuiInputTextFlags_ReadOnly);
	if (ImGui::BeginChild("##fileContents", { ImGui::GetWindowContentRegionWidth() * 0.5f, -1 }, true, ImGuiWindowFlags_AlwaysVerticalScrollbar))
	{
		ImGui::PushTextWrapPos();

		for (auto it = &m_PrintingLines[m_PrintingLineCount - 1]; it != &m_PrintingLines[-1]; --it)
		{
			assert(*it);
			(*it)->Print();
		}

		ImGui::PopTextWrapPos();
		ImGui::SetScrollHereY(1.0f);
		ImGui::EndChild();
	}

	ImGui::SameLine();

	if (ImGui::BeginChild("Test", { -1, -1 }, true))
	{
		if (ImGui::BeginChildFrame(ImGui::GetID("TestSDFSDF"), { -1, -1 }))
		{
			ImGui::Columns(3, "PlayersColumns");
			//ImGui::SetColumnWidth(0, 32);
			ImGui::TextUnformatted("#"); ImGui::NextColumn();
			ImGui::TextUnformatted("Name"); ImGui::NextColumn();
			ImGui::TextUnformatted("Steam ID"); ImGui::NextColumn();
			ImGui::Separator();
			for (const auto& player : m_CurrentLobbyMembers)
			{
				char buf[32];
				if (auto result = mh::to_chars(buf, player.m_Index))
					*result.ptr = '\0';
				else
					continue;

				ImGui::Selectable(buf, false, ImGuiSelectableFlags_SpanAllColumns); ImGui::NextColumn();

				ImGui::TextUnformatted(player.m_Name.c_str()); ImGui::NextColumn(); // TODO: player names

				ImGui::TextUnformatted(player.m_SteamID.str().c_str()); ImGui::NextColumn();
			}

			ImGui::EndChildFrame();
		}

		ImGui::EndChild();
	}
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
								auto member = memberLine->GetLobbyMember();
								if (member.m_Index < m_CurrentLobbyMembers.size())
								{
									if (std::is_eq(m_CurrentLobbyMembers[member.m_Index].m_SteamID <=> member.m_SteamID))
										member.m_Name = m_CurrentLobbyMembers[member.m_Index].m_Name;

									m_CurrentLobbyMembers[member.m_Index] = member;
								}

								break;
							}
							case ConsoleLineType::PlayerStatus:
							{
								auto statusLine = static_cast<const ServerStatusPlayerLine*>(parsed.get());
								const auto& status = statusLine->GetPlayerStatus();

								for (auto& lobbyMember : m_CurrentLobbyMembers)
								{
									// std::is_eq workaround for incomplete MSVC implementation
									if (std::is_eq(lobbyMember.m_SteamID <=> status.m_SteamID))
										lobbyMember.m_Name = status.m_Name;
								}
							}
							}

							m_ConsoleLines.push_back(std::move(parsed));
							consoleLinesUpdated = true;
						}
					}

					std::tm time{};
					from_chars_throw(match[1], time.tm_mon);
					time.tm_mon -= 1;

					from_chars_throw(match[2], time.tm_mday);
					from_chars_throw(match[3], time.tm_year);
					time.tm_year -= 1900;

					from_chars_throw(match[4], time.tm_hour);
					from_chars_throw(match[5], time.tm_min);
					from_chars_throw(match[6], time.tm_sec);

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

void MainWindow::UpdatePrintingLines()
{
	m_PrintingLineCount = 0;
	for (auto it = m_ConsoleLines.rbegin(); it != m_ConsoleLines.rend(); ++it)
	{
		if (it->get()->ShouldPrint())
		{
			m_PrintingLines[m_PrintingLineCount++] = it->get();
			if (m_PrintingLineCount >= 512)
				break;
		}
	}
}

void MainWindow::CustomDeleters::operator()(FILE* f) const
{
	fclose(f);
}
