#include "ConsoleLines.h"
#include "GameData/UserMessageType.h"
#include "Log.h"
#include "RegexHelpers.h"

#include <mh/text/charconv_helper.hpp>
#include <mh/text/string_insertion.hpp>
#include <imgui_desktop/ScopeGuards.h>
#include "ImGui_TF2BotDetector.h"

#include <regex>
#include <stdexcept>

using namespace tf2_bot_detector;
using namespace std::chrono_literals;
using namespace std::string_literals;
using namespace std::string_view_literals;

GenericConsoleLine::GenericConsoleLine(time_point_t timestamp, std::string text) :
	BaseClass(timestamp), m_Text(std::move(text))
{
	m_Text.shrink_to_fit();
}

std::shared_ptr<IConsoleLine> GenericConsoleLine::TryParse(const std::string_view& text, time_point_t timestamp)
{
	return std::make_shared<GenericConsoleLine>(timestamp, std::string(text));
}

void GenericConsoleLine::Print() const
{
	ImGui::TextUnformatted(m_Text.data(), m_Text.data() + m_Text.size());
}

ChatConsoleLine::ChatConsoleLine(time_point_t timestamp, std::string playerName, std::string message, bool isDead, bool isTeam) :
	ConsoleLineBase(timestamp), m_PlayerName(std::move(playerName)), m_Message(std::move(message)), m_IsDead(isDead), m_IsTeam(isTeam)
{
	m_PlayerName.shrink_to_fit();
	m_Message.shrink_to_fit();
}

std::shared_ptr<IConsoleLine> ChatConsoleLine::TryParse(const std::string_view& text, time_point_t timestamp)
{
	return TryParse(text, timestamp, false);
}

std::shared_ptr<ChatConsoleLine> ChatConsoleLine::TryParseFlexible(const std::string_view& text, time_point_t timestamp)
{
	return TryParse(text, timestamp, true);
}

std::shared_ptr<ChatConsoleLine> ChatConsoleLine::TryParse(const std::string_view& text, time_point_t timestamp, bool flexible)
{
	if (!flexible)
		LogWarning("Found ourselves searching chat message "s << std::quoted(text) << " in non-flexible mode");

	static const std::regex s_Regex(R"regex((\*DEAD\*)?\s*(\(TEAM\))?\s*(.{1,33}) :  ((?:.|[\r\n])*))regex", std::regex::optimize);
	static const std::regex s_RegexFlexible(R"regex((\*DEAD\*)?\s*(\(TEAM\))?\s*((?:.|\s){1,33}?) :  ((?:.|[\r\n])*))regex", std::regex::optimize);

	if (svmatch result; std::regex_match(text.begin(), text.end(), result, flexible ? s_RegexFlexible : s_Regex))
	{
		return std::make_shared<ChatConsoleLine>(timestamp, result[3].str(), result[4].str(),
			result[1].matched, result[2].matched);
	}

	return nullptr;
}

template<typename TTextFunc, typename TSameLineFunc>
static void ProcessChatMessage(const ChatConsoleLine& msgLine, TTextFunc&& textFunc, TSameLineFunc& sameLineFunc)
{
	const auto PrintLHS = [&]
	{
		if (msgLine.IsDead())
		{
			textFunc(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "*DEAD*");
			sameLineFunc();
		}

		if (msgLine.IsTeam())
		{
			textFunc(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "(TEAM)");
			sameLineFunc();
		}

		textFunc(ImVec4(0.8f, 0.8f, 1.0f, 1.0f), msgLine.GetPlayerName());
		sameLineFunc();

		textFunc(ImVec4(1, 1, 1, 1), ": ");
		sameLineFunc();
	};

	PrintLHS();

	const auto& msg = msgLine.GetMessage();
	const ImVec4 msgColor(0.8f, 0.8f, 0.8f, 1.0f);
	if (msg.find('\n') == msg.npos)
	{
		textFunc(msgColor, msg);
	}
	else
	{
		// collapse groups of newlines in the message into red "(\n x <count>)" text
		bool firstLine = true;
		for (size_t i = 0; i < msg.size(); )
		{
			if (!firstLine)
			{
				ImGuiDesktop::ScopeGuards::GlobalAlpha alpha(0.5f);
				PrintLHS();
			}

			size_t nonNewlineEnd = std::min(msg.find('\n', i), msg.size());
			{
				auto start = msg.data() + i;
				auto end = msg.data() + nonNewlineEnd;
				textFunc({ 1, 1, 1, 1 }, std::string_view(start, end - start));
			}

			size_t newlineEnd = std::min(msg.find_first_not_of('\n', nonNewlineEnd), msg.size());

			if (newlineEnd > nonNewlineEnd)
			{
				char buf[64];
				sprintf_s(buf, "(\\n x %zu)", (newlineEnd - nonNewlineEnd));
				textFunc(ImVec4(1, 0.5f, 0.5f, 1.0f), buf);
				sameLineFunc();
			}

			i = newlineEnd;
			firstLine = false;
		}
	}
}

void ChatConsoleLine::Print() const
{
	ImGuiDesktop::ScopeGuards::ID id(this);

	ImGui::BeginGroup();
	ProcessChatMessage(*this,
		[](const ImVec4& color, const std::string_view& msg) { ImGui::TextColoredUnformatted(color, msg); },
		[] { ImGui::SameLine(); });
	ImGui::EndGroup();

	if (auto scope = ImGui::BeginPopupContextItemScope("ChatConsoleLineContextMenu"))
	{
		if (ImGui::MenuItem("Copy"))
		{
			std::string fullText;

			ProcessChatMessage(*this,
				[&](const ImVec4&, const std::string_view& msg)
				{
					if (!fullText.empty())
						fullText += ' ';

					fullText.append(msg);
				},
				[] {});

			ImGui::SetClipboardText(fullText.c_str());
		}
	}
}

LobbyHeaderLine::LobbyHeaderLine(time_point_t timestamp, unsigned memberCount, unsigned pendingCount) :
	BaseClass(timestamp), m_MemberCount(memberCount), m_PendingCount(pendingCount)
{
}

std::shared_ptr<IConsoleLine> LobbyHeaderLine::TryParse(const std::string_view& text, time_point_t timestamp)
{
	static const std::regex s_Regex(R"regex(CTFLobbyShared: ID:([0-9a-f]*)\s+(\d+) member\(s\), (\d+) pending)regex", std::regex::optimize);

	if (svmatch result; std::regex_match(text.begin(), text.end(), result, s_Regex))
	{
		unsigned memberCount, pendingCount;
		if (!mh::from_chars(std::string_view(&*result[2].first, result[2].length()), memberCount))
			throw std::runtime_error("Failed to parse lobby member count");
		if (!mh::from_chars(std::string_view(&*result[3].first, result[3].length()), pendingCount))
			throw std::runtime_error("Failed to parse lobby pending member count");

		return std::make_shared<LobbyHeaderLine>(timestamp, memberCount, pendingCount);
	}

	return nullptr;
}

void LobbyHeaderLine::Print() const
{
	ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.8f, 1.0f), "CTFLobbyShared: %u member(s), %u pending",
		m_MemberCount, m_PendingCount);
}

LobbyMemberLine::LobbyMemberLine(time_point_t timestamp, const LobbyMember& lobbyMember) :
	BaseClass(timestamp), m_LobbyMember(lobbyMember)
{
}

std::shared_ptr<IConsoleLine> LobbyMemberLine::TryParse(const std::string_view& text, time_point_t timestamp)
{
	static const std::regex s_Regex(R"regex(\s+(?:(?:Member)|(Pending))\[(\d+)\] (\[.*\])\s+team = (\w+)\s+type = (\w+))regex", std::regex::optimize);

	if (svmatch result; std::regex_match(text.begin(), text.end(), result, s_Regex))
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

		return std::make_shared<LobbyMemberLine>(timestamp, member);
	}

	return nullptr;
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

auto IConsoleLine::GetTypeData() -> std::list<ConsoleLineTypeData>&
{
	static std::list<ConsoleLineTypeData> s_List;
	return s_List;
}

std::shared_ptr<IConsoleLine> IConsoleLine::ParseConsoleLine(const std::string_view& text, time_point_t timestamp)
{
	auto& list = GetTypeData();

	if ((s_TotalParseCount % 1024) == 0)
	{
		// Periodically re-sort the line types for best performance
		list.sort([](ConsoleLineTypeData& lhs, ConsoleLineTypeData& rhs)
			{
				if (rhs.m_AutoParse != lhs.m_AutoParse)
					return rhs.m_AutoParse < lhs.m_AutoParse;

				// Intentionally reversed, we want descending order
				return rhs.m_AutoParseSuccessCount < lhs.m_AutoParseSuccessCount;
			});
	}

	s_TotalParseCount++;

	for (auto& data : list)
	{
		if (!data.m_AutoParse)
			continue;

		auto parsed = data.m_TryParseFunc(text, timestamp);
		if (!parsed)
			continue;

		data.m_AutoParseSuccessCount++;
		return parsed;
	}

	//if (auto chatLine = ChatConsoleLine::TryParse(text, timestamp))
	//	return chatLine;

	return nullptr;
	//return std::make_shared<GenericConsoleLine>(timestamp, std::string(text));
}

void IConsoleLine::AddTypeData(ConsoleLineTypeData data)
{
	GetTypeData().push_back(std::move(data));
}

ServerStatusPlayerLine::ServerStatusPlayerLine(time_point_t timestamp, PlayerStatus playerStatus) :
	BaseClass(timestamp), m_PlayerStatus(std::move(playerStatus))
{
}

std::shared_ptr<IConsoleLine> ServerStatusPlayerLine::TryParse(const std::string_view& text, time_point_t timestamp)
{
	static const std::regex s_Regex(R"regex(#\s+(\d+)\s+"((?:.|\n)+)"\s+(\[.*\])\s+(?:(\d+):)?(\d+):(\d+)\s+(\d+)\s+(\d+)\s+(\w+)(?:\s+(\S+))?)regex", std::regex::optimize);

	if (svmatch result; std::regex_match(text.begin(), text.end(), result, s_Regex))
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

			status.m_ConnectionTime = timestamp - ((connectedHours * 1h) + (connectedMins * 1min) + connectedSecs * 1s);
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

		return std::make_shared<ServerStatusPlayerLine>(timestamp, std::move(status));
	}

	return nullptr;
}

void ServerStatusPlayerLine::Print() const
{
	const PlayerStatus& s = m_PlayerStatus;
	ImGui::Text("# %6u \"%-19s\" %-19s %4u %4u",
		s.m_UserID,
		s.m_Name.c_str(),
		s.m_SteamID.str().c_str(),
		s.m_Ping,
		s.m_Loss);
}

std::shared_ptr<IConsoleLine> ClientReachedServerSpawnLine::TryParse(const std::string_view& text, time_point_t timestamp)
{
	if (text == "Client reached server_spawn."sv)
		return std::make_shared<ClientReachedServerSpawnLine>(timestamp);

	return nullptr;
}

void ClientReachedServerSpawnLine::Print() const
{
	ImGui::TextUnformatted("Client reached server_spawn.");
}

KillNotificationLine::KillNotificationLine(time_point_t timestamp, std::string attackerName,
	std::string victimName, std::string weaponName, bool wasCrit) :
	BaseClass(timestamp), m_AttackerName(std::move(attackerName)), m_VictimName(std::move(victimName)),
	m_WeaponName(std::move(weaponName)), m_WasCrit(wasCrit)
{
}

std::shared_ptr<IConsoleLine> KillNotificationLine::TryParse(const std::string_view& text, time_point_t timestamp)
{
	static const std::regex s_Regex(R"regex((.*) killed (.*) with (.*)\.( \(crit\))?)regex", std::regex::optimize);

	if (svmatch result; std::regex_match(text.begin(), text.end(), result, s_Regex))
	{
		return std::make_shared<KillNotificationLine>(timestamp, result[1].str(),
			result[2].str(), result[3].str(), result[4].matched);
	}

	return nullptr;
}

void KillNotificationLine::Print() const
{
	ImGui::Text("%s killed %s with %s.%s", m_AttackerName.c_str(),
		m_VictimName.c_str(), m_WeaponName.c_str(), m_WasCrit ? " (crit)" : "");
}

LobbyChangedLine::LobbyChangedLine(time_point_t timestamp, LobbyChangeType type) :
	BaseClass(timestamp), m_ChangeType(type)
{
}

std::shared_ptr<IConsoleLine> LobbyChangedLine::TryParse(const std::string_view& text, time_point_t timestamp)
{
	if (text == "Lobby created"sv)
		return std::make_shared<LobbyChangedLine>(timestamp, LobbyChangeType::Created);
	else if (text == "Lobby updated"sv)
		return std::make_shared<LobbyChangedLine>(timestamp, LobbyChangeType::Updated);
	else if (text == "Lobby destroyed"sv)
		return std::make_shared<LobbyChangedLine>(timestamp, LobbyChangeType::Destroyed);

	return nullptr;
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
	BaseClass(timestamp), m_Name(std::move(name)), m_Value(value),
	m_FlagsList(std::move(flagsList)), m_HelpText(std::move(helpText))
{
}

std::shared_ptr<IConsoleLine> CvarlistConvarLine::TryParse(const std::string_view& text, time_point_t timestamp)
{
	static const std::regex s_Regex(R"regex((\S+)\s+:\s+([-\d.]+)\s+:\s+(.+)?\s+:[\t ]+(.+)?)regex", std::regex::optimize);
	if (svmatch result; std::regex_match(text.begin(), text.end(), result, s_Regex))
	{
		float value;
		from_chars_throw(result[2], value);
		return std::make_shared<CvarlistConvarLine>(timestamp, result[1].str(), value, result[3].str(), result[4].str());
	}

	return nullptr;
}

void CvarlistConvarLine::Print() const
{
	// TODO
	//ImGui::Text("\"%s\" = \"%s\"", m_Name.c_str(), m_ConvarValue.c_str());
}

ServerStatusShortPlayerLine::ServerStatusShortPlayerLine(time_point_t timestamp, PlayerStatusShort playerStatus) :
	BaseClass(timestamp), m_PlayerStatus(std::move(playerStatus))
{
}

std::shared_ptr<IConsoleLine> ServerStatusShortPlayerLine::TryParse(const std::string_view& text, time_point_t timestamp)
{
	static const std::regex s_Regex(R"regex(#(\d+) - (.+))regex", std::regex::optimize);

	if (svmatch result; std::regex_match(text.begin(), text.end(), result, s_Regex))
	{
		PlayerStatusShort status{};

		from_chars_throw(result[1], status.m_ClientIndex);
		assert(status.m_ClientIndex >= 1);
		status.m_Name = result[2].str();

		return std::make_shared<ServerStatusShortPlayerLine>(timestamp, std::move(status));
	}

	return nullptr;
}

void ServerStatusShortPlayerLine::Print() const
{
	ImGui::Text("#%u - %s", m_PlayerStatus.m_ClientIndex, m_PlayerStatus.m_Name.c_str());
}

VoiceReceiveLine::VoiceReceiveLine(time_point_t timestamp, uint8_t channel,
	uint8_t entindex, uint16_t bufSize) :
	BaseClass(timestamp), m_Channel(channel), m_Entindex(entindex), m_BufSize(bufSize)
{
}

std::shared_ptr<IConsoleLine> VoiceReceiveLine::TryParse(const std::string_view& text, time_point_t timestamp)
{
	static const std::regex s_Regex(R"regex(Voice - chan (\d+), ent (\d+), bufsize: (\d+))regex", std::regex::optimize);

	if (svmatch result; std::regex_match(text.begin(), text.end(), result, s_Regex))
	{
		uint8_t channel;
		from_chars_throw(result[1], channel);

		uint8_t entindex;
		from_chars_throw(result[2], entindex);

		uint16_t bufSize;
		from_chars_throw(result[3], bufSize);

		return std::make_shared<VoiceReceiveLine>(timestamp, channel, entindex, bufSize);
	}

	return nullptr;
}

void VoiceReceiveLine::Print() const
{
	ImGui::Text("Voice - chan %u, ent %u, bufsize: %u", m_Channel, m_Entindex, m_BufSize);
}

ServerStatusPlayerCountLine::ServerStatusPlayerCountLine(time_point_t timestamp, uint8_t playerCount,
	uint8_t botCount, uint8_t maxplayers) :
	BaseClass(timestamp), m_PlayerCount(playerCount), m_BotCount(botCount), m_MaxPlayers(maxplayers)
{
}

std::shared_ptr<IConsoleLine> ServerStatusPlayerCountLine::TryParse(const std::string_view& text, time_point_t timestamp)
{
	static const std::regex s_Regex(R"regex(players : (\d+) humans, (\d+) bots \((\d+) max\))regex", std::regex::optimize);

	if (svmatch result; std::regex_match(text.begin(), text.end(), result, s_Regex))
	{
		uint8_t playerCount, botCount, maxPlayers;
		from_chars_throw(result[1], playerCount);
		from_chars_throw(result[2], botCount);
		from_chars_throw(result[3], maxPlayers);
		return std::make_shared<ServerStatusPlayerCountLine>(timestamp, playerCount, botCount, maxPlayers);
	}

	return nullptr;
}

void ServerStatusPlayerCountLine::Print() const
{
	ImGui::Text("players : %u humans, %u bots (%u max)", m_PlayerCount, m_BotCount, m_MaxPlayers);
}

EdictUsageLine::EdictUsageLine(time_point_t timestamp, uint16_t usedEdicts, uint16_t totalEdicts) :
	BaseClass(timestamp), m_UsedEdicts(usedEdicts), m_TotalEdicts(totalEdicts)
{
}

std::shared_ptr<IConsoleLine> EdictUsageLine::TryParse(const std::string_view& text, time_point_t timestamp)
{
	static const std::regex s_Regex(R"regex(edicts  : (\d+) used of (\d+) max)regex", std::regex::optimize);

	if (svmatch result; std::regex_match(text.begin(), text.end(), result, s_Regex))
	{
		uint16_t usedEdicts, totalEdicts;
		from_chars_throw(result[1], usedEdicts);
		from_chars_throw(result[2], totalEdicts);
		return std::make_shared<EdictUsageLine>(timestamp, usedEdicts, totalEdicts);
	}

	return nullptr;
}

void EdictUsageLine::Print() const
{
	ImGui::Text("edicts  : %u used of %u max", m_UsedEdicts, m_TotalEdicts);
}

PingLine::PingLine(time_point_t timestamp, uint16_t ping, std::string playerName) :
	BaseClass(timestamp), m_Ping(ping), m_PlayerName(std::move(playerName))
{
}

std::shared_ptr<IConsoleLine> PingLine::TryParse(const std::string_view& text, time_point_t timestamp)
{
	static const std::regex s_Regex(R"regex( *(\d+) ms : (.{1,32}))regex", std::regex::optimize);

	if (svmatch result; std::regex_match(text.begin(), text.end(), result, s_Regex))
	{
		uint16_t ping;
		from_chars_throw(result[1], ping);
		return std::make_shared<PingLine>(timestamp, ping, result[2].str());
	}

	return nullptr;
}

void PingLine::Print() const
{
	ImGui::Text("%4u : %s", m_Ping, m_PlayerName.c_str());
}

SVCUserMessageLine::SVCUserMessageLine(time_point_t timestamp, std::string address, UserMessageType type, uint16_t bytes) :
	BaseClass(timestamp), m_Address(std::move(address)), m_MsgType(type), m_MsgBytes(bytes)
{
}

std::shared_ptr<IConsoleLine> SVCUserMessageLine::TryParse(const std::string_view& text, time_point_t timestamp)
{
	static const std::regex s_Regex(R"regex(Msg from ((?:\d+\.\d+\.\d+\.\d+:\d+)|loopback): svc_UserMessage: type (\d+), bytes (\d+))regex", std::regex::optimize);

	if (svmatch result; std::regex_match(text.begin(), text.end(), result, s_Regex))
	{
		uint16_t type, bytes;
		from_chars_throw(result[2], type);

		from_chars_throw(result[3], bytes);

		return std::make_shared<SVCUserMessageLine>(timestamp, result[1].str(), UserMessageType(type), bytes);
	}

	return nullptr;
}

bool SVCUserMessageLine::ShouldPrint() const
{
#ifdef _DEBUG
	switch (m_MsgType)
	{
	case UserMessageType::CallVoteFailed:
	case UserMessageType::VoteFailed:
	case UserMessageType::VotePass:
	case UserMessageType::VoteSetup:
	case UserMessageType::VoteStart:
		return true;
	}
#endif

	return false;
}

void SVCUserMessageLine::Print() const
{
	switch (m_MsgType)
	{
	case UserMessageType::CallVoteFailed:
	case UserMessageType::VoteFailed:
	case UserMessageType::VotePass:
	case UserMessageType::VoteSetup:
	case UserMessageType::VoteStart:
		ImGui::TextColoredUnformatted({ 0, 1, 0, 1 }, ""s << m_MsgType);
		break;

	default:
		ImGui::Text("Msg from %s: svc_UserMessage: type %u, bytes %u", m_Address.c_str(), m_MsgType, m_MsgBytes);
		break;
	}
}

std::shared_ptr<IConsoleLine> LobbyStatusFailedLine::TryParse(const std::string_view& text, time_point_t timestamp)
{
	if (text == "Failed to find lobby shared object"sv)
		return std::make_shared<LobbyStatusFailedLine>(timestamp);

	return nullptr;
}

void LobbyStatusFailedLine::Print() const
{
	ImGui::Text("Failed to find lobby shared object");
}

ConfigExecLine::ConfigExecLine(time_point_t timestamp, std::string configFileName, bool success) :
	BaseClass(timestamp), m_ConfigFileName(std::move(configFileName)), m_Success(success)
{
}

std::shared_ptr<IConsoleLine> ConfigExecLine::TryParse(const std::string_view& text, time_point_t timestamp)
{
	// Success
	constexpr auto prefix = "execing "sv;
	if (text.starts_with(prefix))
		return std::make_shared<ConfigExecLine>(timestamp, std::string(text.substr(prefix.size())), true);

	// Failure
	static const std::regex s_Regex(R"regex('(.*)' not present; not executing\.)regex", std::regex::optimize);
	if (svmatch result; std::regex_match(text.begin(), text.end(), result, s_Regex))
		return std::make_shared<ConfigExecLine>(timestamp, result[1].str(), false);

	return nullptr;
}

void ConfigExecLine::Print() const
{
	if (m_Success)
		ImGui::Text("execing %s", m_ConfigFileName.c_str());
	else
		ImGui::Text("'%s' not present; not executing.", m_ConfigFileName.c_str());
}
