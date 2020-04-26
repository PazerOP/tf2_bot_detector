#include "ConsoleLines.h"
#include "RegexCharConv.h"

#include <mh/text/charconv_helper.hpp>
#include <mh/text/string_insertion.hpp>
#include <imgui.h>

#include <regex>
#include <stdexcept>

using namespace tf2_bot_detector;
using namespace std::string_literals;
using namespace std::string_view_literals;

GenericConsoleLine::GenericConsoleLine(std::string&& text) :
	m_Text(std::move(text))
{
	m_Text.shrink_to_fit();
}

void GenericConsoleLine::Print() const
{
	ImGui::TextUnformatted(m_Text.data(), m_Text.data() + m_Text.size());
}

ChatConsoleLine::ChatConsoleLine(std::string&& playerName, std::string&& message, bool isDead, bool isTeam) :
	m_PlayerName(std::move(playerName)), m_Message(std::move(message)), m_IsDead(isDead), m_IsTeam(isTeam)
{
	m_PlayerName.shrink_to_fit();
	m_Message.shrink_to_fit();
}

void ChatConsoleLine::Print() const
{
	if (m_IsDead)
	{
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
		ImGui::TextUnformatted("*DEAD*");
		ImGui::PopStyleColor();
		ImGui::SameLine();
	}

	if (m_IsTeam)
	{
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 1.0f, 0.5f, 1.0f));
		ImGui::TextUnformatted("(TEAM)");
		ImGui::PopStyleColor();
		ImGui::SameLine();
	}

	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.8f, 1.0f, 1.0f));
	ImGui::TextUnformatted(m_PlayerName.data(), m_PlayerName.data() + m_PlayerName.size());
	ImGui::SameLine();

	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));
	ImGui::TextUnformatted(": ");
	ImGui::SameLine();

	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
	ImGui::TextUnformatted(m_Message.data(), m_Message.data() + m_Message.size());
	ImGui::PopStyleColor(3);
}

LobbyHeaderLine::LobbyHeaderLine(unsigned memberCount, unsigned pendingCount) :
	m_MemberCount(memberCount), m_PendingCount(pendingCount)
{
}

void LobbyHeaderLine::Print() const
{
	ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.8f, 1.0f), "CTFLobbyShared: %u member(s), %u pending",
		m_MemberCount, m_PendingCount);
}

LobbyMemberLine::LobbyMemberLine(const LobbyMember& lobbyMember) :
	m_LobbyMember(lobbyMember)
{
}

void LobbyMemberLine::Print() const
{
	const char* team;
	switch (m_LobbyMember.m_Team)
	{
	case LobbyMemberTeam::Invaders: team = "TF_GC_TEAM_ATTACKERS"; break;
	case LobbyMemberTeam::Defenders: team = "TF_GC_TEAM_INVADERS"; break;
	default: throw std::runtime_error("Unexpected lobby member team");
	}

	assert(m_LobbyMember.m_Type == LobbyMemberType::Player);
	const char* type = "MATCH_PLAYER";

	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.0f, 0.8f, 1.0f));
	ImGui::TextUnformatted(("  Member["s << m_LobbyMember.m_Index << ' ' << m_LobbyMember.m_SteamID << "  team = " << team << "  type = " << type).c_str());
	ImGui::PopStyleColor();
}

using svmatch = std::match_results<std::string_view::const_iterator>;
static std::regex s_ChatRegex(R"regex((\*DEAD\*\s+)?(\(TEAM\)\s+)?(.{1,32}) :  (.*))regex", std::regex::optimize);
static std::regex s_LobbyHeaderRegex(R"regex(CTFLobbyShared: ID:([0-9a-f]*)\s+(\d+) member\(s\), (\d+) pending)regex", std::regex::optimize);
static std::regex s_LobbyMemberRegex(R"regex(\s+Member\[(\d+)\] (\[.*\])\s+team = (\w+)\s+type = (\w+))regex", std::regex::optimize);
static std::regex s_TimestampRegex(R"regex(\n?(\d\d)\/(\d\d)\/(\d\d\d\d) - (\d\d):(\d\d):(\d\d): )regex", std::regex::optimize);
static std::regex s_StatusMessageRegex(R"regex(#\s+(\d+)\s+"(.*)"\s+(\[.*\])\s+(?:(\d+):)?(\d+):(\d+)\s+(\d+)\s+(\d+)\s+(\w+))regex", std::regex::optimize);
std::unique_ptr<IConsoleLine> IConsoleLine::ParseConsoleLine(const std::string_view& text)
{
	svmatch result;
	if (std::regex_match(text.begin(), text.end(), result, s_ChatRegex))
	{
		return std::make_unique<ChatConsoleLine>(result[3].str(), result[4].str(),
			result[1].matched, result[2].matched);
	}
	else if (std::regex_match(text.begin(), text.end(), result, s_LobbyHeaderRegex))
	{
		unsigned memberCount, pendingCount;
		if (!mh::from_chars(std::string_view(&*result[2].first, result[2].length()), memberCount))
			throw std::runtime_error("Failed to parse lobby member count");
		if (!mh::from_chars(std::string_view(&*result[3].first, result[3].length()), pendingCount))
			throw std::runtime_error("Failed to parse lobby pending member count");

		return std::make_unique<LobbyHeaderLine>(memberCount, pendingCount);
	}
	else if (std::regex_match(text.begin(), text.end(), result, s_LobbyMemberRegex))
	{
		LobbyMember member{};
		if (!mh::from_chars(std::string_view(&*result[1].first, result[1].length()), member.m_Index))
			throw std::runtime_error("Failed to parse lobby member regex");

		member.m_SteamID = SteamID(std::string_view(&*result[2].first, result[2].length()));

		std::string_view teamStr(&*result[3].first, result[3].length());

		if (teamStr == "TF_GC_TEAM_DEFENDERS"sv)
			member.m_Team = LobbyMemberTeam::Defenders;
		else if (teamStr == "TF_GC_TEAM_INVADERS"sv)
			member.m_Team = LobbyMemberTeam::Invaders;
		else
			throw std::runtime_error("Unknown lobby member team");

		std::string_view typeStr(&*result[4].first, result[4].length());
		if (typeStr == "MATCH_PLAYER"sv)
			member.m_Type = LobbyMemberType::Player;
		else
			throw std::runtime_error("Unknown lobby member type");

		return std::make_unique<LobbyMemberLine>(member);
	}
	else if (std::regex_match(text.begin(), text.end(), result, s_StatusMessageRegex))
	{
		PlayerStatus status{};

		from_chars_throw(result[1], status.m_UserID);
		status.m_Name = result[2].str();
		status.m_SteamID = std::string_view(&*result[3].first, result[3].length());

		// Connected time
		{
			uint32_t connectedHours = 0;
			uint32_t connectedMins;
			uint32_t connectedSecs;

			if (result[4].matched)
				from_chars_throw(result[4], connectedHours);

			from_chars_throw(result[5], connectedMins);
			from_chars_throw(result[6], connectedSecs);

			status.m_ConnectedTime = (connectedHours * 60 * 60) + (connectedMins * 60) + connectedSecs;
		}

		from_chars_throw(result[7], status.m_Ping);
		from_chars_throw(result[8], status.m_Loss);

		return std::make_unique<ServerStatusPlayerLine>(status);
	}

	//return nullptr;
	return std::make_unique<GenericConsoleLine>(std::string(text));
}

ServerStatusPlayerLine::ServerStatusPlayerLine(const PlayerStatus& playerStatus) :
	m_PlayerStatus(playerStatus)
{
}

void ServerStatusPlayerLine::Print() const
{
	// Don't print anything for this particular message
}
