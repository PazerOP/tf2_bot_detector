#include "ConsoleLines.h"
#include "Application.h"
#include "Config/Settings.h"
#include "GameData/MatchmakingQueue.h"
#include "GameData/UserMessageType.h"
#include "UI/MainWindow.h"
#include "UI/ImGui_TF2BotDetector.h"
#include "Util/RegexUtils.h"
#include "Log.h"
#include "WorldState.h"

#include <mh/algorithm/multi_compare.hpp>
#include <mh/text/charconv_helper.hpp>
#include <mh/text/fmtstr.hpp>
#include <mh/text/format.hpp>
#include <mh/text/string_insertion.hpp>
#include <imgui_desktop/ScopeGuards.h>

#include <regex>
#include <sstream>
#include <stdexcept>

#undef GetMessage

using namespace tf2_bot_detector;
using namespace std::chrono_literals;
using namespace std::string_literals;
using namespace std::string_view_literals;

GenericConsoleLine::GenericConsoleLine(time_point_t timestamp, std::string text) :
	BaseClass(timestamp), m_Text(std::move(text))
{
	m_Text.shrink_to_fit();
}

std::shared_ptr<IConsoleLine> GenericConsoleLine::TryParse(const ConsoleLineTryParseArgs& args)
{
	return std::make_shared<GenericConsoleLine>(args.m_Timestamp, std::string(args.m_Text));
}

void GenericConsoleLine::Print(const PrintArgs& args) const
{
	ImGui::TextFmt(m_Text);
}

ChatConsoleLine::ChatConsoleLine(time_point_t timestamp, std::string playerName, std::string message,
	bool isDead, bool isTeam, bool isSelf, TeamShareResult teamShareResult, SteamID id) :
	ConsoleLineBase(timestamp), m_PlayerName(std::move(playerName)), m_Message(std::move(message)),
	m_IsDead(isDead), m_IsTeam(isTeam), m_IsSelf(isSelf), m_TeamShareResult(teamShareResult), m_PlayerSteamID(id)
{
	m_PlayerName.shrink_to_fit();
	m_Message.shrink_to_fit();
}

std::shared_ptr<IConsoleLine> ChatConsoleLine::TryParse(const ConsoleLineTryParseArgs& args)
{
	LogError(MH_SOURCE_LOCATION_CURRENT(), "This should never happen!");
	return nullptr;
}

#if 0
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
#endif

template<typename TTextFunc, typename TSameLineFunc>
static void ProcessChatMessage(const ChatConsoleLine& msgLine, const Settings::Theme& theme,
	TTextFunc&& textFunc, TSameLineFunc&& sameLineFunc)
{
	auto& colorSettings = theme.m_Colors;
	std::array<float, 4> colors{ 0.8f, 0.8f, 1.0f, 1.0f };

	if (msgLine.IsSelf())
		colors = colorSettings.m_ChatLogYouFG;
	else if (msgLine.GetTeamShareResult() == TeamShareResult::SameTeams)
		colors = colorSettings.m_ChatLogFriendlyTeamFG;
	else if (msgLine.GetTeamShareResult() == TeamShareResult::OppositeTeams)
		colors = colorSettings.m_ChatLogEnemyTeamFG;

	const auto PrintLHS = [&](float alphaScale = 1.0f)
	{
		if (msgLine.IsDead())
		{
			textFunc(ImVec4(0.5f, 0.5f, 0.5f, 1.0f * alphaScale), "*DEAD*");
			sameLineFunc();
		}

		if (msgLine.IsTeam())
		{
			textFunc(ImVec4(colors[0], colors[1], colors[2], colors[3] * 0.75f * alphaScale), "(TEAM)");
			sameLineFunc();
		}

		textFunc(ImVec4(colors[0], colors[1], colors[2], colors[3] * alphaScale), msgLine.GetPlayerName());
		sameLineFunc();

		textFunc(ImVec4(1, 1, 1, alphaScale), ": ");
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
				PrintLHS(0.5f);
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
				sameLineFunc();
				textFunc(ImVec4(1, 0.5f, 0.5f, 1.0f),
					mh::fmtstr<64>("(\\n x {})", (newlineEnd - nonNewlineEnd)).c_str());
			}

			i = newlineEnd;
			firstLine = false;
		}
	}
}

void ChatConsoleLine::Print(const PrintArgs& args) const
{
	ImGuiDesktop::ScopeGuards::ID id(this);

	ImGui::BeginGroup();
	ProcessChatMessage(*this, args.m_Settings.m_Theme,
		[](const ImVec4& color, const std::string_view& msg) { ImGui::TextFmt(color, msg); },
		[] { ImGui::SameLine(); });
	ImGui::EndGroup();

	const bool isHovered = ImGui::IsItemHovered();

	if (auto scope = ImGui::BeginPopupContextItemScope("ChatConsoleLineContextMenu"))
	{
		if (ImGui::MenuItem("Copy"))
		{
			std::string fullText;

			ProcessChatMessage(*this, args.m_Settings.m_Theme,
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
	else if (isHovered)
	{
		if (auto player = args.m_WorldState.FindPlayer(m_PlayerSteamID))
			args.m_MainWindow.DrawPlayerTooltip(*player);
	}
}

LobbyHeaderLine::LobbyHeaderLine(time_point_t timestamp, unsigned memberCount, unsigned pendingCount) :
	BaseClass(timestamp), m_MemberCount(memberCount), m_PendingCount(pendingCount)
{
}

std::shared_ptr<IConsoleLine> LobbyHeaderLine::TryParse(const ConsoleLineTryParseArgs& args)
{
	static const std::regex s_Regex(R"regex(CTFLobbyShared: ID:([0-9a-f]*)\s+(\d+) member\(s\), (\d+) pending)regex", std::regex::optimize);

	if (svmatch result; std::regex_match(args.m_Text.begin(), args.m_Text.end(), result, s_Regex))
	{
		unsigned memberCount, pendingCount;
		if (!mh::from_chars(std::string_view(&*result[2].first, result[2].length()), memberCount))
			throw std::runtime_error("Failed to parse lobby member count");
		if (!mh::from_chars(std::string_view(&*result[3].first, result[3].length()), pendingCount))
			throw std::runtime_error("Failed to parse lobby pending member count");

		return std::make_shared<LobbyHeaderLine>(args.m_Timestamp, memberCount, pendingCount);
	}

	return nullptr;
}

void LobbyHeaderLine::Print(const PrintArgs& args) const
{
	ImGui::TextFmt({ 1.0f, 1.0f, 0.8f, 1.0f }, "CTFLobbyShared: {} member(s), {} pending",
		m_MemberCount, m_PendingCount);
}

LobbyMemberLine::LobbyMemberLine(time_point_t timestamp, const LobbyMember& lobbyMember) :
	BaseClass(timestamp), m_LobbyMember(lobbyMember)
{
}

std::shared_ptr<IConsoleLine> LobbyMemberLine::TryParse(const ConsoleLineTryParseArgs& args)
{
	static const std::regex s_Regex(R"regex(\s+(?:(?:Member)|(Pending))\[(\d+)\] (\[.*\])\s+team = (\w+)\s+type = (\w+))regex", std::regex::optimize);

	if (svmatch result; std::regex_match(args.m_Text.begin(), args.m_Text.end(), result, s_Regex))
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

		return std::make_shared<LobbyMemberLine>(args.m_Timestamp, member);
	}

	return nullptr;
}

void LobbyMemberLine::Print(const PrintArgs& args) const
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

	ImGui::TextFmt({ 0.8f, 0.0f, 0.8f, 1 }, "  Member[{}] {}  team = {}  type = {}",
		m_LobbyMember.m_Index, m_LobbyMember.m_SteamID, team, type);
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

std::shared_ptr<IConsoleLine> IConsoleLine::ParseConsoleLine(const std::string_view& text, time_point_t timestamp, IWorldState& world)
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

	const ConsoleLineTryParseArgs args{ text, timestamp, world };
	for (auto& data : list)
	{
		if (!data.m_AutoParse)
			continue;

		auto parsed = data.m_TryParseFunc(args);
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

std::shared_ptr<IConsoleLine> ServerStatusPlayerLine::TryParse(const ConsoleLineTryParseArgs& args)
{
	static const std::regex s_Regex(R"regex(#\s+(\d+)\s+"((?:.|[\r\n])+)"\s+(\[.*\])\s+(?:(\d+):)?(\d+):(\d+)\s+(\d+)\s+(\d+)\s+(\w+)(?:\s+(\S+))?)regex", std::regex::optimize);

	if (svmatch result; std::regex_match(args.m_Text.begin(), args.m_Text.end(), result, s_Regex))
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

			status.m_ConnectionTime = args.m_Timestamp - ((connectedHours * 1h) + (connectedMins * 1min) + connectedSecs * 1s);
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

		return std::make_shared<ServerStatusPlayerLine>(args.m_Timestamp, std::move(status));
	}

	return nullptr;
}

void ServerStatusPlayerLine::Print(const PrintArgs& args) const
{
	const PlayerStatus& s = m_PlayerStatus;
	ImGui::Text("# %6u \"%-19s\" %-19s %4u %4u",
		s.m_UserID,
		s.m_Name.c_str(),
		s.m_SteamID.str().c_str(),
		s.m_Ping,
		s.m_Loss);
}

std::shared_ptr<IConsoleLine> ClientReachedServerSpawnLine::TryParse(const ConsoleLineTryParseArgs& args)
{
	if (args.m_Text == "Client reached server_spawn."sv)
		return std::make_shared<ClientReachedServerSpawnLine>(args.m_Timestamp);

	return nullptr;
}

void ClientReachedServerSpawnLine::Print(const PrintArgs& args) const
{
	ImGui::TextFmt("Client reached server_spawn.");
}

KillNotificationLine::KillNotificationLine(time_point_t timestamp, std::string attackerName,
	std::string victimName, std::string weaponName, bool wasCrit) :
	BaseClass(timestamp), m_AttackerName(std::move(attackerName)), m_VictimName(std::move(victimName)),
	m_WeaponName(std::move(weaponName)), m_WasCrit(wasCrit)
{
}

std::shared_ptr<IConsoleLine> KillNotificationLine::TryParse(const ConsoleLineTryParseArgs& args)
{
	static const std::regex s_Regex(R"regex((.*) killed (.*) with (.*)\.( \(crit\))?)regex", std::regex::optimize);

	if (svmatch result; std::regex_match(args.m_Text.begin(), args.m_Text.end(), result, s_Regex))
	{
		return std::make_shared<KillNotificationLine>(args.m_Timestamp, result[1].str(),
			result[2].str(), result[3].str(), result[4].matched);
	}

	return nullptr;
}

void KillNotificationLine::Print(const PrintArgs& args) const
{
	ImGui::TextFmt("{} killed {} with {}.{}", m_AttackerName.c_str(),
		m_VictimName.c_str(), m_WeaponName.c_str(), m_WasCrit ? " (crit)" : "");
}

LobbyChangedLine::LobbyChangedLine(time_point_t timestamp, LobbyChangeType type) :
	BaseClass(timestamp), m_ChangeType(type)
{
}

std::shared_ptr<IConsoleLine> LobbyChangedLine::TryParse(const ConsoleLineTryParseArgs& args)
{
	if (args.m_Text == "Lobby created"sv)
		return std::make_shared<LobbyChangedLine>(args.m_Timestamp, LobbyChangeType::Created);
	else if (args.m_Text == "Lobby updated"sv)
		return std::make_shared<LobbyChangedLine>(args.m_Timestamp, LobbyChangeType::Updated);
	else if (args.m_Text == "Lobby destroyed"sv)
		return std::make_shared<LobbyChangedLine>(args.m_Timestamp, LobbyChangeType::Destroyed);

	return nullptr;
}

bool LobbyChangedLine::ShouldPrint() const
{
	return GetChangeType() == LobbyChangeType::Created;
}

void LobbyChangedLine::Print(const PrintArgs& args) const
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

std::shared_ptr<IConsoleLine> CvarlistConvarLine::TryParse(const ConsoleLineTryParseArgs& args)
{
	static const std::regex s_Regex(R"regex((\S+)\s+:\s+([-\d.]+)\s+:\s+(.+)?\s+:[\t ]+(.+)?)regex", std::regex::optimize);
	if (svmatch result; std::regex_match(args.m_Text.begin(), args.m_Text.end(), result, s_Regex))
	{
		float value;
		from_chars_throw(result[2], value);
		return std::make_shared<CvarlistConvarLine>(args.m_Timestamp, result[1].str(), value, result[3].str(), result[4].str());
	}

	return nullptr;
}

void CvarlistConvarLine::Print(const PrintArgs& args) const
{
	// TODO
	//ImGui::Text("\"%s\" = \"%s\"", m_Name.c_str(), m_ConvarValue.c_str());
}

ServerStatusShortPlayerLine::ServerStatusShortPlayerLine(time_point_t timestamp, PlayerStatusShort playerStatus) :
	BaseClass(timestamp), m_PlayerStatus(std::move(playerStatus))
{
}

std::shared_ptr<IConsoleLine> ServerStatusShortPlayerLine::TryParse(const ConsoleLineTryParseArgs& args)
{
	static const std::regex s_Regex(R"regex(#(\d+) - (.+))regex", std::regex::optimize);

	if (svmatch result; std::regex_match(args.m_Text.begin(), args.m_Text.end(), result, s_Regex))
	{
		PlayerStatusShort status{};

		from_chars_throw(result[1], status.m_ClientIndex);
		assert(status.m_ClientIndex >= 1);
		status.m_Name = result[2].str();

		return std::make_shared<ServerStatusShortPlayerLine>(args.m_Timestamp, std::move(status));
	}

	return nullptr;
}

void ServerStatusShortPlayerLine::Print(const PrintArgs& args) const
{
	ImGui::Text("#%u - %s", m_PlayerStatus.m_ClientIndex, m_PlayerStatus.m_Name.c_str());
}

VoiceReceiveLine::VoiceReceiveLine(time_point_t timestamp, uint8_t channel,
	uint8_t entindex, uint16_t bufSize) :
	BaseClass(timestamp), m_Channel(channel), m_Entindex(entindex), m_BufSize(bufSize)
{
}

std::shared_ptr<IConsoleLine> VoiceReceiveLine::TryParse(const ConsoleLineTryParseArgs& args)
{
	static const std::regex s_Regex(R"regex(Voice - chan (\d+), ent (\d+), bufsize: (\d+))regex", std::regex::optimize);

	if (svmatch result; std::regex_match(args.m_Text.begin(), args.m_Text.end(), result, s_Regex))
	{
		uint8_t channel;
		from_chars_throw(result[1], channel);

		uint8_t entindex;
		from_chars_throw(result[2], entindex);

		uint16_t bufSize;
		from_chars_throw(result[3], bufSize);

		return std::make_shared<VoiceReceiveLine>(args.m_Timestamp, channel, entindex, bufSize);
	}

	return nullptr;
}

void VoiceReceiveLine::Print(const PrintArgs& args) const
{
	ImGui::TextFmt("Voice - chan {}, ent {}, bufsize: {}", +m_Channel, m_Entindex, m_BufSize);
}

ServerStatusPlayerCountLine::ServerStatusPlayerCountLine(time_point_t timestamp, uint8_t playerCount,
	uint8_t botCount, uint8_t maxplayers) :
	BaseClass(timestamp), m_PlayerCount(playerCount), m_BotCount(botCount), m_MaxPlayers(maxplayers)
{
}

std::shared_ptr<IConsoleLine> ServerStatusPlayerCountLine::TryParse(const ConsoleLineTryParseArgs& args)
{
	static const std::regex s_Regex(R"regex(players : (\d+) humans, (\d+) bots \((\d+) max\))regex", std::regex::optimize);

	if (svmatch result; std::regex_match(args.m_Text.begin(), args.m_Text.end(), result, s_Regex))
	{
		uint8_t playerCount, botCount, maxPlayers;
		from_chars_throw(result[1], playerCount);
		from_chars_throw(result[2], botCount);
		from_chars_throw(result[3], maxPlayers);
		return std::make_shared<ServerStatusPlayerCountLine>(args.m_Timestamp, playerCount, botCount, maxPlayers);
	}

	return nullptr;
}

void ServerStatusPlayerCountLine::Print(const PrintArgs& args) const
{
	ImGui::TextFmt("players : {} humans, {} bots ({} max)", m_PlayerCount, m_BotCount, m_MaxPlayers);
}

EdictUsageLine::EdictUsageLine(time_point_t timestamp, uint16_t usedEdicts, uint16_t totalEdicts) :
	BaseClass(timestamp), m_UsedEdicts(usedEdicts), m_TotalEdicts(totalEdicts)
{
}

std::shared_ptr<IConsoleLine> EdictUsageLine::TryParse(const ConsoleLineTryParseArgs& args)
{
	static const std::regex s_Regex(R"regex(edicts  : (\d+) used of (\d+) max)regex", std::regex::optimize);

	if (svmatch result; std::regex_match(args.m_Text.begin(), args.m_Text.end(), result, s_Regex))
	{
		uint16_t usedEdicts, totalEdicts;
		from_chars_throw(result[1], usedEdicts);
		from_chars_throw(result[2], totalEdicts);
		return std::make_shared<EdictUsageLine>(args.m_Timestamp, usedEdicts, totalEdicts);
	}

	return nullptr;
}

void EdictUsageLine::Print(const PrintArgs& args) const
{
	ImGui::TextFmt("edicts  : {} used of {} max", m_UsedEdicts, m_TotalEdicts);
}

PingLine::PingLine(time_point_t timestamp, uint16_t ping, std::string playerName) :
	BaseClass(timestamp), m_Ping(ping), m_PlayerName(std::move(playerName))
{
}

std::shared_ptr<IConsoleLine> PingLine::TryParse(const ConsoleLineTryParseArgs& args)
{
	static const std::regex s_Regex(R"regex( *(\d+) ms : (.{1,32}))regex", std::regex::optimize);

	if (svmatch result; std::regex_match(args.m_Text.begin(), args.m_Text.end(), result, s_Regex))
	{
		uint16_t ping;
		from_chars_throw(result[1], ping);
		return std::make_shared<PingLine>(args.m_Timestamp, ping, result[2].str());
	}

	return nullptr;
}

void PingLine::Print(const PrintArgs& args) const
{
	ImGui::Text("%4u : %s", m_Ping, m_PlayerName.c_str());
}

SVCUserMessageLine::SVCUserMessageLine(time_point_t timestamp, std::string address, UserMessageType type, uint16_t bytes) :
	BaseClass(timestamp), m_Address(std::move(address)), m_MsgType(type), m_MsgBytes(bytes)
{
}

std::shared_ptr<IConsoleLine> SVCUserMessageLine::TryParse(const ConsoleLineTryParseArgs& args)
{
	static const std::regex s_Regex(R"regex(Msg from ((?:\d+\.\d+\.\d+\.\d+:\d+)|loopback): svc_UserMessage: type (\d+), bytes (\d+))regex", std::regex::optimize);

	if (svmatch result; std::regex_match(args.m_Text.begin(), args.m_Text.end(), result, s_Regex))
	{
		uint16_t type, bytes;
		from_chars_throw(result[2], type);

		from_chars_throw(result[3], bytes);

		return std::make_shared<SVCUserMessageLine>(args.m_Timestamp, result[1].str(), UserMessageType(type), bytes);
	}

	return nullptr;
}

bool SVCUserMessageLine::IsSpecial(UserMessageType type)
{
#ifdef _DEBUG
	return mh::any_eq(type,
		UserMessageType::CallVoteFailed,
		UserMessageType::VoteFailed,
		UserMessageType::VotePass,
		UserMessageType::VoteSetup,
		UserMessageType::VoteStart);
#else
	return false;
#endif
}

bool SVCUserMessageLine::ShouldPrint() const
{
	return IsSpecial(m_MsgType);
}

void SVCUserMessageLine::Print(const PrintArgs& args) const
{
	if (IsSpecial(m_MsgType))
		ImGui::TextFmt({ 0, 1, 1, 1 }, "{}", mh::enum_fmt(m_MsgType));
	else
		ImGui::TextFmt("Msg from {}: svc_UserMessage: type {}, bytes {}", m_Address, int(m_MsgType), m_MsgBytes);
}

std::shared_ptr<IConsoleLine> LobbyStatusFailedLine::TryParse(const ConsoleLineTryParseArgs& args)
{
	if (args.m_Text == "Failed to find lobby shared object"sv)
		return std::make_shared<LobbyStatusFailedLine>(args.m_Timestamp);

	return nullptr;
}

void LobbyStatusFailedLine::Print(const PrintArgs& args) const
{
	ImGui::Text("Failed to find lobby shared object");
}

ConfigExecLine::ConfigExecLine(time_point_t timestamp, std::string configFileName, bool success) :
	BaseClass(timestamp), m_ConfigFileName(std::move(configFileName)), m_Success(success)
{
}

std::shared_ptr<IConsoleLine> ConfigExecLine::TryParse(const ConsoleLineTryParseArgs& args)
{
	// Success
	constexpr auto prefix = "execing "sv;
	if (args.m_Text.starts_with(prefix))
		return std::make_shared<ConfigExecLine>(args.m_Timestamp, std::string(args.m_Text.substr(prefix.size())), true);

	// Failure
	static const std::regex s_Regex(R"regex('(.*)' not present; not executing\.)regex", std::regex::optimize);
	if (svmatch result; std::regex_match(args.m_Text.begin(), args.m_Text.end(), result, s_Regex))
		return std::make_shared<ConfigExecLine>(args.m_Timestamp, result[1].str(), false);

	return nullptr;
}

void ConfigExecLine::Print(const PrintArgs& args) const
{
	if (m_Success)
		ImGui::Text("execing %s", m_ConfigFileName.c_str());
	else
		ImGui::Text("'%s' not present; not executing.", m_ConfigFileName.c_str());
}

ServerStatusMapLine::ServerStatusMapLine(time_point_t timestamp, std::string mapName,
	const std::array<float, 3>& position) :
	BaseClass(timestamp), m_MapName(std::move(mapName)), m_Position(position)
{
}

std::shared_ptr<IConsoleLine> ServerStatusMapLine::TryParse(const ConsoleLineTryParseArgs& args)
{
	static const std::regex s_Regex(R"regex(map     : (.*) at: ((?:-|\d)+) x, ((?:-|\d)+) y, ((?:-|\d)+) z)regex", std::regex::optimize);

	if (svmatch result; std::regex_match(args.m_Text.begin(), args.m_Text.end(), result, s_Regex))
	{
		std::array<float, 3> pos{};
		from_chars_throw(result[2], pos[0]);
		from_chars_throw(result[3], pos[1]);
		from_chars_throw(result[4], pos[2]);

		return std::make_shared<ServerStatusMapLine>(args.m_Timestamp, result[1].str(), pos);
	}

	return nullptr;
}

void ServerStatusMapLine::Print(const PrintArgs& args) const
{
	ImGui::Text("map     : %s at: %1.0f x, %1.0f y, %1.0f z", m_MapName.c_str(),
		m_Position[0], m_Position[1], m_Position[2]);
}

std::shared_ptr<IConsoleLine> TeamsSwitchedLine::TryParse(const ConsoleLineTryParseArgs& args)
{
	if (args.m_Text == "Teams have been switched."sv)
		return std::make_unique<TeamsSwitchedLine>(args.m_Timestamp);

	return nullptr;
}

bool TeamsSwitchedLine::ShouldPrint() const
{
#ifdef _DEBUG
	return true;
#else
	return false;
#endif
}

void TeamsSwitchedLine::Print(const PrintArgs& args) const
{
	ImGui::TextFmt({ 0.98f, 0.73f, 0.01f, 1 }, "Teams have been switched.");
}

ConnectingLine::ConnectingLine(time_point_t timestamp, std::string address, bool isMatchmaking, bool isRetrying) :
	BaseClass(timestamp), m_Address(std::move(address)), m_IsMatchmaking(isMatchmaking), m_IsRetrying(isRetrying)
{
}

std::shared_ptr<IConsoleLine> ConnectingLine::TryParse(const ConsoleLineTryParseArgs& args)
{
	{
		static const std::regex s_ConnectingRegex(R"regex(Connecting to( matchmaking server)? (.*?)(\.\.\.)?)regex", std::regex::optimize);
		if (svmatch result; std::regex_match(args.m_Text.begin(), args.m_Text.end(), result, s_ConnectingRegex))
			return std::make_shared<ConnectingLine>(args.m_Timestamp, result[2].str(), result[1].matched, false);
	}

	{
		static const std::regex s_RetryingRegex(R"regex(Retrying (.*)\.\.\.)regex", std::regex::optimize);
		if (svmatch result; std::regex_match(args.m_Text.begin(), args.m_Text.end(), result, s_RetryingRegex))
			return std::make_shared<ConnectingLine>(args.m_Timestamp, result[1].str(), false, true);
	}

	return nullptr;
}

void ConnectingLine::Print(const PrintArgs& args) const
{
	if (m_IsMatchmaking)
		ImGui::TextFmt("Connecting to matchmaking server {}...", m_Address);
	else if (m_IsRetrying)
		ImGui::TextFmt("Retrying {}...", m_Address);
	else
		ImGui::TextFmt("Connecting to {}...", m_Address);
}

std::shared_ptr<IConsoleLine> HostNewGameLine::TryParse(const ConsoleLineTryParseArgs& args)
{
	if (args.m_Text == "---- Host_NewGame ----"sv)
		return std::make_shared<HostNewGameLine>(args.m_Timestamp);

	return nullptr;
}

void HostNewGameLine::Print(const PrintArgs& args) const
{
	ImGui::TextFmt("---- Host_NewGame ----"sv);
}

PartyHeaderLine::PartyHeaderLine(time_point_t timestamp, TFParty party) :
	BaseClass(timestamp), m_Party(std::move(party))
{
}

std::shared_ptr<IConsoleLine> PartyHeaderLine::TryParse(const ConsoleLineTryParseArgs& args)
{
	static const std::regex s_Regex(R"regex(TFParty:\s+ID:([0-9a-f]+)\s+(\d+) member\(s\)\s+LeaderID: (\[.*\]))regex", std::regex::optimize);
	if (svmatch result; std::regex_match(args.m_Text.begin(), args.m_Text.end(), result, s_Regex))
	{
		TFParty party{};

		{
			uint64_t partyID;
			from_chars_throw(result[1], partyID, 16);
			party.m_PartyID = TFPartyID(partyID);
		}

		from_chars_throw(result[2], party.m_MemberCount);

		party.m_LeaderID = SteamID(result[3].str());

		return std::make_shared<PartyHeaderLine>(args.m_Timestamp, std::move(party));
	}

	return nullptr;
}

void PartyHeaderLine::Print(const PrintArgs& args) const
{
	ImGui::Text("TFParty: ID:%xll  %u member(s)  LeaderID: %s", m_Party.m_PartyID,
		m_Party.m_MemberCount, m_Party.m_LeaderID.str().c_str());
}

std::shared_ptr<IConsoleLine> GameQuitLine::TryParse(const ConsoleLineTryParseArgs& args)
{
	if (args.m_Text == "CTFGCClientSystem::ShutdownGC"sv)
		return std::make_shared<GameQuitLine>(args.m_Timestamp);

	return nullptr;
}

void GameQuitLine::Print(const PrintArgs& args) const
{
	ImGui::TextFmt("CTFGCClientSystem::ShutdownGC"sv);
}

QueueStateChangeLine::QueueStateChangeLine(time_point_t timestamp, TFMatchGroup queueType,
	TFQueueStateChange stateChange) :
	BaseClass(timestamp), m_QueueType(queueType), m_StateChange(stateChange)
{
}

namespace
{
	struct QueueStateChangeType
	{
		std::string_view m_String;
		TFMatchGroup m_QueueType;
		TFQueueStateChange m_StateChange;
	};

	static constexpr QueueStateChangeType QUEUE_STATE_CHANGE_TYPES[] =
	{
		{ "[PartyClient] Requesting queue for 12v12 Casual Match", TFMatchGroup::Casual12s, TFQueueStateChange::RequestedEnter },
		{ "[PartyClient] Entering queue for match group 12v12 Casual Match", TFMatchGroup::Casual12s, TFQueueStateChange::Entered },
		{ "[PartyClient] Requesting exit queue for 12v12 Casual Match", TFMatchGroup::Casual12s, TFQueueStateChange::RequestedExit },
		{ "[PartyClient] Leaving queue for match group 12v12 Casual Match", TFMatchGroup::Casual12s, TFQueueStateChange::Exited },

		{ "[PartyClient] Requesting queue for 6v6 Ladder Match", TFMatchGroup::Competitive6s, TFQueueStateChange::RequestedEnter },
		{ "[PartyClient] Entering queue for match group 6v6 Ladder Match", TFMatchGroup::Competitive6s, TFQueueStateChange::Entered },
		{ "[PartyClient] Requesting exit queue for 6v6 Ladder Match", TFMatchGroup::Competitive6s, TFQueueStateChange::RequestedExit },
		{ "[PartyClient] Leaving queue for match group 6v6 Ladder Match", TFMatchGroup::Competitive6s, TFQueueStateChange::Exited },
	};
}

std::shared_ptr<IConsoleLine> QueueStateChangeLine::TryParse(const ConsoleLineTryParseArgs& args)
{
	for (const auto& match : QUEUE_STATE_CHANGE_TYPES)
	{
		if (args.m_Text == match.m_String)
			return std::make_shared<QueueStateChangeLine>(args.m_Timestamp, match.m_QueueType, match.m_StateChange);
	}

	return nullptr;
}

void QueueStateChangeLine::Print(const PrintArgs& args) const
{
	for (const auto& match : QUEUE_STATE_CHANGE_TYPES)
	{
		if (match.m_QueueType == m_QueueType &&
			match.m_StateChange == m_StateChange)
		{
			ImGui::TextFmt(match.m_String);
			return;
		}
	}
}

InQueueLine::InQueueLine(time_point_t timestamp, TFMatchGroup queueType, time_point_t queueStartTime) :
	BaseClass(timestamp), m_QueueType(queueType), m_QueueStartTime(queueStartTime)
{
}

std::shared_ptr<IConsoleLine> InQueueLine::TryParse(const ConsoleLineTryParseArgs& args)
{
	static const std::regex s_Regex(
		R"regex(    MatchGroup: (\d+)\s+Started matchmaking:\s+(.*)\s+\(\d+ seconds ago, now is (.*)\))regex",
		std::regex::optimize);

	if (svmatch result; std::regex_match(args.m_Text.begin(), args.m_Text.end(), result, s_Regex))
	{
		TFMatchGroup matchGroup = TFMatchGroup::Invalid;
		{
			uint8_t matchGroupRaw{};
			from_chars_throw(result[1], matchGroupRaw);
			matchGroup = TFMatchGroup(matchGroupRaw);
		}

		time_point_t startTime{};
		{
			std::tm startTimeFull{};
			std::istringstream ss;
			ss.str(result[2].str());
			//ss >> std::get_time(&startTimeFull, "%c");
			ss >> std::get_time(&startTimeFull, "%a %b %d %H:%M:%S %Y");

			startTimeFull.tm_isdst = -1; // auto-detect DST
			startTime = clock_t::from_time_t(std::mktime(&startTimeFull));
			if (startTime.time_since_epoch().count() < 0)
			{
				LogError(MH_SOURCE_LOCATION_CURRENT(), "Failed to parse "s << std::quoted(result[2].str()) << " as a timestamp");
				return nullptr;
			}
		}

		return std::make_shared<InQueueLine>(args.m_Timestamp, matchGroup, startTime);
	}

	return nullptr;
}

void InQueueLine::Print(const PrintArgs& args) const
{
	char timeBufNow[128];
	const auto localTM = GetLocalTM();
	strftime(timeBufNow, sizeof(timeBufNow), "%c", &localTM);

	char timeBufThen[128];
	const auto queueStartTimeTM = ToTM(m_QueueStartTime);
	strftime(timeBufThen, sizeof(timeBufThen), "%c", &queueStartTimeTM);

	const uint64_t seconds = to_seconds<uint64_t>(clock_t::now() - m_QueueStartTime);

	ImGui::TextFmt("    MatchGroup: {}  Started matchmaking: {} ({} seconds ago, now is {})",
		uint32_t(m_QueueType), timeBufThen, seconds, timeBufNow);
}

ServerJoinLine::ServerJoinLine(time_point_t timestamp, std::string hostName, std::string mapName,
	uint8_t playerCount, uint8_t playerMaxCount, uint32_t buildNumber, uint32_t serverNumber) :
	BaseClass(timestamp), m_HostName(std::move(hostName)), m_MapName(std::move(mapName)), m_PlayerCount(playerCount),
	m_PlayerMaxCount(playerMaxCount), m_BuildNumber(buildNumber), m_ServerNumber(serverNumber)
{
}

std::shared_ptr<IConsoleLine> ServerJoinLine::TryParse(const ConsoleLineTryParseArgs& args)
{
	static const std::regex s_Regex(
		R"regex(\n(.*)\nMap: (.*)\nPlayers: (\d+) \/ (\d+)\nBuild: (\d+)\nServer Number: (\d+)\s+)regex",
		std::regex::optimize);

	if (svmatch result; std::regex_match(args.m_Text.begin(), args.m_Text.end(), result, s_Regex))
	{
		uint32_t buildNumber, serverNumber;
		from_chars_throw(result[5], buildNumber);
		from_chars_throw(result[6], serverNumber);

		uint8_t playerCount, playerMaxCount;
		from_chars_throw(result[3], playerCount);
		from_chars_throw(result[4], playerMaxCount);

		return std::make_shared<ServerJoinLine>(args.m_Timestamp, result[1].str(), result[2].str(),
			playerCount, playerMaxCount, buildNumber, serverNumber);
	}

	return nullptr;
}

void ServerJoinLine::Print(const PrintArgs& args) const
{
	ImGui::TextFmt("\n{}\nMap: {}\nPlayers: {} / {}\nBuild: {}\nServer Number: {}\n",
		m_HostName, m_MapName, m_PlayerCount, m_PlayerMaxCount, m_BuildNumber, m_ServerNumber);
}

ServerDroppedPlayerLine::ServerDroppedPlayerLine(time_point_t timestamp, std::string playerName, std::string reason) :
	BaseClass(timestamp), m_PlayerName(std::move(playerName)), m_Reason(std::move(reason))
{
}

std::shared_ptr<IConsoleLine> ServerDroppedPlayerLine::TryParse(const ConsoleLineTryParseArgs& args)
{
	static const std::regex s_Regex(R"regex(Dropped (.*) from server \((.*)\))regex", std::regex::optimize);

	if (svmatch result; std::regex_match(args.m_Text.begin(), args.m_Text.end(), result, s_Regex))
	{
		return std::make_shared<ServerDroppedPlayerLine>(args.m_Timestamp, result[1].str(), result[2].str());
	}

	return nullptr;
}

void ServerDroppedPlayerLine::Print(const PrintArgs& args) const
{
	ImGui::TextFmt("Dropped {} from server ({})", m_PlayerName, m_Reason);
}

ServerStatusPlayerIPLine::ServerStatusPlayerIPLine(time_point_t timestamp, std::string localIP, std::string publicIP) :
	BaseClass(timestamp), m_LocalIP(std::move(localIP)), m_PublicIP(std::move(publicIP))
{
}

std::shared_ptr<IConsoleLine> ServerStatusPlayerIPLine::TryParse(const ConsoleLineTryParseArgs& args)
{
	static const std::regex s_Regex(R"regex(udp\/ip  : (.*)  \(public ip: (.*)\))regex", std::regex::optimize);

	if (svmatch result; std::regex_match(args.m_Text.begin(), args.m_Text.end(), result, s_Regex))
		return std::make_shared<ServerStatusPlayerIPLine>(args.m_Timestamp, result[1].str(), result[2].str());

	return nullptr;
}

void ServerStatusPlayerIPLine::Print(const PrintArgs& args) const
{
	ImGui::TextFmt("udp/ip  : {}  (public ip: {})", m_LocalIP, m_PublicIP);
}

DifferingLobbyReceivedLine::DifferingLobbyReceivedLine(time_point_t timestamp, const Lobby& newLobby,
	const Lobby& currentLobby, bool connectedToMatchServer, bool hasLobby, bool assignedMatchEnded) :
	BaseClass(timestamp), m_NewLobby(newLobby), m_CurrentLobby(currentLobby),
	m_ConnectedToMatchServer(connectedToMatchServer), m_HasLobby(hasLobby), m_AssignedMatchEnded(assignedMatchEnded)
{
}

std::shared_ptr<IConsoleLine> DifferingLobbyReceivedLine::TryParse(const ConsoleLineTryParseArgs& args)
{
	static const std::regex s_Regex(
		R"regex(Differing lobby received\. Lobby: (.*)\/Match(\d+)\/Lobby(\d+) CurrentlyAssigned: (.*)\/Match(\d+)\/Lobby(\d+) ConnectedToMatchServer: (\d+) HasLobby: (\d+) AssignedMatchEnded: (\d+))regex",
		std::regex::optimize);

	if (svmatch result; std::regex_match(args.m_Text.begin(), args.m_Text.end(), result, s_Regex))
	{
		Lobby newLobby;
		newLobby.m_LobbyID = SteamID(result[1].str());
		from_chars_throw(result[2], newLobby.m_MatchID);
		from_chars_throw(result[3], newLobby.m_LobbyNumber);

		Lobby currentLobby;
		currentLobby.m_LobbyID = SteamID(result[4].str());
		from_chars_throw(result[5], currentLobby.m_MatchID);
		from_chars_throw(result[6], currentLobby.m_LobbyNumber);

		bool connectedToMatchServer, hasLobby, assignedMatchEnded;
		from_chars_throw(result[7], connectedToMatchServer);
		from_chars_throw(result[8], hasLobby);
		from_chars_throw(result[9], assignedMatchEnded);

		return std::make_shared<DifferingLobbyReceivedLine>(args.m_Timestamp, newLobby, currentLobby,
			connectedToMatchServer, hasLobby, assignedMatchEnded);
	}

	return nullptr;
}

namespace tf2_bot_detector
{
	template<typename CharT, typename Traits>
	std::basic_ostream<CharT, Traits>& operator<<(std::basic_ostream<CharT, Traits>& os,
		const DifferingLobbyReceivedLine::Lobby& lobby)
	{
		return os << lobby.m_LobbyID << "/Match" << lobby.m_MatchID << "/Lobby" << lobby.m_LobbyNumber;
	}
}

void DifferingLobbyReceivedLine::Print(const PrintArgs& args) const
{
	ImGui::TextFmt(
		"Differing lobby received. Lobby: {} CurrentlyAssigned: {} ConnectedToMatchServer: {} HasLobby: {} AssignedMatchEnded: {}",
		m_NewLobby, m_CurrentLobby, int(m_ConnectedToMatchServer), int(m_HasLobby), int(m_AssignedMatchEnded));
}

#ifdef TF2BD_ENABLE_TESTS
#include <catch2/catch.hpp>

TEST_CASE("ProcessChatMessage - Newlines", "[tf2bd]")
{
	const auto TestProcessChatMessage = [](std::string message)
	{
		ChatConsoleLine line(tfbd_clock_t::now(), "<playername>", std::move(message), false, false, false, TeamShareResult::SameTeams, SteamID{});

		Settings::Theme dummyTheme;

		std::string output;

		ProcessChatMessage(line, dummyTheme,
			[&](const ImVec4& color, const std::string_view& msg)
			{
				output << msg << '\n';
			},
			[&]()
			{
				output << "<sameline>";
			});

		return output;
	};

	REQUIRE(TestProcessChatMessage("this is a normal message") ==
		"<playername>\n<sameline>: \n<sameline>this is a normal message\n");
	REQUIRE(TestProcessChatMessage("this is\n\na message with newlines in it") ==
		"<playername>\n<sameline>: \n<sameline>this is\n<sameline>(\\n x 2)\n<playername>\n<sameline>: \n<sameline>a message with newlines in it\n");
}

#endif

MatchmakingBannedTimeLine::MatchmakingBannedTimeLine(time_point_t timestamp, LadderType ladderType, uint64_t bannedTime) :
	BaseClass(timestamp), m_LadderType(ladderType), m_BannedTime(bannedTime)
{
}

std::shared_ptr<IConsoleLine> MatchmakingBannedTimeLine::TryParse(const ConsoleLineTryParseArgs& args)
{
	static const std::regex s_Regex(R"regex((?:(casual)|(?:ranked))_banned_time: (\d+))regex", std::regex::optimize);

	if (svmatch result; std::regex_match(args.m_Text.begin(), args.m_Text.end(), result, s_Regex))
	{
		const LadderType ladderType = result[1].matched ? LadderType::Casual : LadderType::Competitive;

		uint64_t bannedTime;
		from_chars_throw(result[2], bannedTime);

		return std::make_shared<MatchmakingBannedTimeLine>(args.m_Timestamp, ladderType, bannedTime);
	}

	return nullptr;
}

void MatchmakingBannedTimeLine::Print(const PrintArgs& args) const
{
	ImGui::TextFmt("{}_banned_time: {}",
		m_LadderType == LadderType::Casual ? "casual" : "ranked",
		m_BannedTime);
}
