#include "ConsoleLines.h"
#include "RegexCharConv.h"

#include <mh/text/charconv_helper.hpp>
#include <mh/text/string_insertion.hpp>
#include "ImGui_TF2BotDetector.h"

#include <regex>
#include <stdexcept>

using namespace tf2_bot_detector;
using namespace std::string_literals;
using namespace std::string_view_literals;

GenericConsoleLine::GenericConsoleLine(time_point_t timestamp, std::string&& text) :
	IConsoleLine(timestamp), m_Text(std::move(text))
{
	m_Text.shrink_to_fit();
}

void GenericConsoleLine::Print() const
{
	ImGui::TextUnformatted(m_Text.data(), m_Text.data() + m_Text.size());
}

ChatConsoleLine::ChatConsoleLine(time_point_t timestamp, std::string&& playerName, std::string&& message, bool isDead, bool isTeam) :
	IConsoleLine(timestamp), m_PlayerName(std::move(playerName)), m_Message(std::move(message)), m_IsDead(isDead), m_IsTeam(isTeam)
{
	m_PlayerName.shrink_to_fit();
	m_Message.shrink_to_fit();
}

void ChatConsoleLine::Print() const
{
	if (m_IsDead)
	{
		ImGui::TextColoredUnformatted(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "*DEAD*");
		ImGui::SameLine();
	}

	if (m_IsTeam)
	{
		ImGui::TextColoredUnformatted(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "(TEAM)");
		ImGui::SameLine();
	}

	ImGui::TextColoredUnformatted(ImVec4(0.8f, 0.8f, 1.0f, 1.0f), m_PlayerName);
	ImGui::SameLine();

	ImGui::TextColoredUnformatted(ImVec4(1, 1, 1, 1), ": ");
	ImGui::SameLine();

	const ImVec4 msgColor(0.8f, 0.8f, 0.8f, 1.0f);
	if (m_Message.find('\n') == m_Message.npos)
	{
		ImGui::TextColoredUnformatted(msgColor, m_Message);
	}
	else
	{
		// collapse groups of newlines in the message into red "(\n x <count>)" text
		ImGui::NewLine();
		for (size_t i = 0; i < m_Message.size(); )
		{
			size_t nonNewlineEnd = std::min(m_Message.find('\n', i), m_Message.size());
			ImGui::SameLine();
			ImGui::TextUnformatted(m_Message.data() + i, m_Message.data() + nonNewlineEnd);

			size_t newlineEnd = std::min(m_Message.find_first_not_of('\n', nonNewlineEnd), m_Message.size());
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(1, 0.5f, 0.5f, 1.0f), "(\\n x %i)", (newlineEnd - i));

			i = newlineEnd;
		}
	}
}

LobbyHeaderLine::LobbyHeaderLine(time_point_t timestamp, unsigned memberCount, unsigned pendingCount) :
	IConsoleLine(timestamp), m_MemberCount(memberCount), m_PendingCount(pendingCount)
{
}

void LobbyHeaderLine::Print() const
{
	ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.8f, 1.0f), "CTFLobbyShared: %u member(s), %u pending",
		m_MemberCount, m_PendingCount);
}

LobbyMemberLine::LobbyMemberLine(time_point_t timestamp, const LobbyMember& lobbyMember) :
	IConsoleLine(timestamp), m_LobbyMember(lobbyMember)
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

IConsoleLine::IConsoleLine(time_point_t timestamp) :
	m_Timestamp(timestamp)
{
}

using svmatch = std::match_results<std::string_view::const_iterator>;
static const std::regex s_ChatRegex(R"regex((\*DEAD\*)?\s*(\(TEAM\))?\s*(.{1,32}) :  ((?:.|[\r\n])*))regex", std::regex::optimize);
static const std::regex s_LobbyHeaderRegex(R"regex(CTFLobbyShared: ID:([0-9a-f]*)\s+(\d+) member\(s\), (\d+) pending)regex", std::regex::optimize);
static const std::regex s_LobbyMemberRegex(R"regex(\s+(?:(?:Member)|(Pending))\[(\d+)\] (\[.*\])\s+team = (\w+)\s+type = (\w+))regex", std::regex::optimize);
static const std::regex s_KillNotificationRegex(R"regex((.*) killed (.*) with (.*)\.( \(crit\))?)regex", std::regex::optimize);
static const std::regex s_StatusMessageRegex(R"regex(#\s+(\d+)\s+"(.*)"\s+(\[.*\])\s+(?:(\d+):)?(\d+):(\d+)\s+(\d+)\s+(\d+)\s+(\w+)(?:\s+(\S+))?)regex", std::regex::optimize);
static const std::regex s_StatusShortRegex(R"regex(#(\d+) - (.+))regex", std::regex::optimize);
static const std::regex s_CvarlistValueRegex(R"regex((\S+)\s+:\s+([-\d.]+)\s+:\s+(.+)?\s+:[\t ]+(.+)?)regex", std::regex::optimize);
static const std::regex s_VoiceReceiveRegex(R"regex(Voice - chan (\d+), ent (\d+), bufsize: (\d+))regex", std::regex::optimize);
std::unique_ptr<IConsoleLine> IConsoleLine::ParseConsoleLine(const std::string_view& text, time_point_t timestamp)
{
	svmatch result;
	if (std::regex_match(text.begin(), text.end(), result, s_VoiceReceiveRegex))
	{
		uint8_t channel;
		from_chars_throw(result[1], channel);

		uint8_t entindex;
		from_chars_throw(result[2], entindex);

		uint16_t bufSize;
		from_chars_throw(result[3], bufSize);

		return std::make_unique<VoiceReceiveLine>(timestamp, channel, entindex, bufSize);
	}
	else if (std::regex_match(text.begin(), text.end(), result, s_LobbyMemberRegex))
	{
		LobbyMember member{};
		member.m_Pending = result[1].matched;

		if (!mh::from_chars(std::string_view(&*result[2].first, result[2].length()), member.m_Index))
			throw std::runtime_error("Failed to parse lobby member regex");

		member.m_SteamID = SteamID(std::string_view(&*result[3].first, result[3].length()));

		std::string_view teamStr(&*result[4].first, result[4].length());

		if (teamStr == "TF_GC_TEAM_DEFENDERS"sv)
			member.m_Team = LobbyMemberTeam::Defenders;
		else if (teamStr == "TF_GC_TEAM_INVADERS"sv)
			member.m_Team = LobbyMemberTeam::Invaders;
		else
			throw std::runtime_error("Unknown lobby member team");

		std::string_view typeStr(&*result[5].first, result[5].length());
		if (typeStr == "MATCH_PLAYER"sv)
			member.m_Type = LobbyMemberType::Player;
		else if (typeStr == "INVALID_PLAYER"sv)
			member.m_Type = LobbyMemberType::InvalidPlayer;
		else
			throw std::runtime_error("Unknown lobby member type");

		return std::make_unique<LobbyMemberLine>(timestamp, member);
	}
	else if (std::regex_match(text.begin(), text.end(), result, s_StatusMessageRegex))
	{
		PlayerStatus status{};

		from_chars_throw(result[1], status.m_UserID);
		status.m_Name = result[2].str();
		status.m_SteamID = SteamID(std::string_view(&*result[3].first, result[3].length()));

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

		// State
		{
			const auto state = std::string_view(&*result[9].first, result[9].length());
			if (state == "active"sv)
				status.m_State = PlayerStatusState::Active;
			else if (state == "spawning"sv)
				status.m_State = PlayerStatusState::Spawning;
			else if (state == "connecting"sv)
				status.m_State = PlayerStatusState::Connecting;
			else if (state == "challenging"sv)
				status.m_State = PlayerStatusState::Challenging;
			else
				throw std::runtime_error("Unknown player status state "s << std::quoted(state));
		}

		status.m_Address = result[10].str();

		return std::make_unique<ServerStatusPlayerLine>(timestamp, std::move(status));
	}
	else if (std::regex_match(text.begin(), text.end(), result, s_StatusShortRegex))
	{
		PlayerStatusShort status{};

		from_chars_throw(result[1], status.m_ClientIndex);
		assert(status.m_ClientIndex >= 1);
		status.m_Name = result[2].str();

		return std::make_unique<ServerStatusShortPlayerLine>(timestamp, std::move(status));
	}
	else if (std::regex_match(text.begin(), text.end(), result, s_LobbyHeaderRegex))
	{
		unsigned memberCount, pendingCount;
		if (!mh::from_chars(std::string_view(&*result[2].first, result[2].length()), memberCount))
			throw std::runtime_error("Failed to parse lobby member count");
		if (!mh::from_chars(std::string_view(&*result[3].first, result[3].length()), pendingCount))
			throw std::runtime_error("Failed to parse lobby pending member count");

		return std::make_unique<LobbyHeaderLine>(timestamp, memberCount, pendingCount);
	}
	else if (std::regex_match(text.begin(), text.end(), result, s_KillNotificationRegex))
	{
		return std::make_unique<KillNotificationLine>(timestamp, result[1].str(),
			result[2].str(), result[3].str(), result[4].matched);
	}
	else if (std::regex_match(text.begin(), text.end(), result, s_ChatRegex))
	{
		return std::make_unique<ChatConsoleLine>(timestamp, result[3].str(), result[4].str(),
			result[1].matched, result[2].matched);
	}
	else if (text == "Client reached server_spawn."sv)
	{
		return std::make_unique<ClientReachedServerSpawnLine>(timestamp);
	}
	else if (text == "Lobby created"sv)
	{
		return std::make_unique<LobbyChangedLine>(timestamp, LobbyChangeType::Created);
	}
	else if (text == "Lobby updated"sv)
	{
		return std::make_unique<LobbyChangedLine>(timestamp, LobbyChangeType::Updated);
	}
	else if (text == "Lobby destroyed"sv)
	{
		return std::make_unique<LobbyChangedLine>(timestamp, LobbyChangeType::Destroyed);
	}
	else if (std::regex_match(text.begin(), text.end(), result, s_CvarlistValueRegex))
	{
		float value;
		from_chars_throw(result[2], value);
		return std::make_unique<CvarlistConvarLine>(timestamp, result[1].str(), value, result[3].str(), result[4].str());
	}

	return nullptr;
	//return std::make_unique<GenericConsoleLine>(timestamp, std::string(text));
}

ServerStatusPlayerLine::ServerStatusPlayerLine(time_point_t timestamp, PlayerStatus playerStatus) :
	IConsoleLine(timestamp), m_PlayerStatus(std::move(playerStatus))
{
}

void ServerStatusPlayerLine::Print() const
{
	// Don't print anything for this particular message
}

void ClientReachedServerSpawnLine::Print() const
{
	ImGui::TextUnformatted("Client reached server_spawn.");
}

KillNotificationLine::KillNotificationLine(time_point_t timestamp, std::string attackerName,
	std::string victimName, std::string weaponName, bool wasCrit) :
	IConsoleLine(timestamp), m_AttackerName(std::move(attackerName)), m_VictimName(std::move(victimName)),
	m_WeaponName(std::move(weaponName)), m_WasCrit(wasCrit)
{
}

void KillNotificationLine::Print() const
{
	ImGui::Text("%s killed %s with %s.%s", m_AttackerName.c_str(),
		m_VictimName.c_str(), m_WeaponName.c_str(), m_WasCrit ? " (crit)" : "");
}

LobbyChangedLine::LobbyChangedLine(time_point_t timestamp, LobbyChangeType type) :
	IConsoleLine(timestamp), m_ChangeType(type)
{
}

bool LobbyChangedLine::ShouldPrint() const
{
	return GetChangeType() == LobbyChangeType::Created;
}

void LobbyChangedLine::Print() const
{
	if (ShouldPrint())
		ImGui::Separator();
}

CvarlistConvarLine::CvarlistConvarLine(time_point_t timestamp, std::string name, float value,
	std::string flagsList, std::string helpText) :
	IConsoleLine(timestamp), m_Name(std::move(name)), m_Value(value),
	m_FlagsList(std::move(flagsList)), m_HelpText(std::move(helpText))
{
}

void CvarlistConvarLine::Print() const
{
	// TODO
	//ImGui::Text("\"%s\" = \"%s\"", m_Name.c_str(), m_ConvarValue.c_str());
}

ServerStatusShortPlayerLine::ServerStatusShortPlayerLine(time_point_t timestamp, PlayerStatusShort playerStatus) :
	IConsoleLine(timestamp), m_PlayerStatus(std::move(playerStatus))
{
}

void ServerStatusShortPlayerLine::Print() const
{
	ImGui::Text("#%u - %s", m_PlayerStatus.m_ClientIndex, m_PlayerStatus.m_Name.c_str());
}

VoiceReceiveLine::VoiceReceiveLine(time_point_t timestamp, uint8_t channel,
	uint8_t entindex, uint16_t bufSize) :
	IConsoleLine(timestamp), m_Channel(channel), m_Entindex(entindex), m_BufSize(bufSize)
{
}

void VoiceReceiveLine::Print() const
{
	ImGui::Text("Voice - chan %u, ent %u, bufsize: %u", m_Channel, m_Entindex, m_BufSize);
}
