#pragma once

#include "Clock.h"
#include "GameData/TFParty.h"
#include "LobbyMember.h"
#include "PlayerStatus.h"
#include "IConsoleLine.h"

#include <mh/reflection/enum.hpp>

#include <array>
#include <memory>
#include <string_view>

namespace tf2_bot_detector
{
	enum class TeamShareResult;
	enum class TFClassType;
	enum class TFMatchGroup;
	enum class TFQueueStateChange;
	enum class UserMessageType;

	class GenericConsoleLine final : public ConsoleLineBase<GenericConsoleLine, false>
	{
		using BaseClass = ConsoleLineBase;

	public:
		GenericConsoleLine(time_point_t timestamp, std::string text);
		static std::shared_ptr<IConsoleLine> TryParse(const ConsoleLineTryParseArgs& args);

		ConsoleLineType GetType() const override { return ConsoleLineType::Generic; }
		bool ShouldPrint() const override { return false; }
		void Print(const PrintArgs& args) const override;

	private:
		std::string m_Text;
	};

	class ChatConsoleLine final : public ConsoleLineBase<ChatConsoleLine, false>
	{
		using BaseClass = ConsoleLineBase;

	public:
		ChatConsoleLine(time_point_t timestamp, std::string playerName, std::string message, bool isDead,
			bool isTeam, bool isSelf, TeamShareResult teamShare, SteamID id);
		static std::shared_ptr<IConsoleLine> TryParse(const ConsoleLineTryParseArgs& args);
		//static std::shared_ptr<ChatConsoleLine> TryParseFlexible(const std::string_view& text, time_point_t timestamp);

		ConsoleLineType GetType() const override { return ConsoleLineType::Chat; }
		void Print(const PrintArgs& args) const override;

		const std::string& GetPlayerName() const { return m_PlayerName; }
		const std::string& GetMessage() const { return m_Message; }
		bool IsDead() const { return m_IsDead; }
		bool IsTeam() const { return m_IsTeam; }
		bool IsSelf() const { return m_IsSelf; }
		TeamShareResult GetTeamShareResult() const { return m_TeamShareResult; }

	private:
		//static std::shared_ptr<ChatConsoleLine> TryParse(const std::string_view& text, time_point_t timestamp, bool flexible);

		std::string m_PlayerName;
		std::string m_Message;
		SteamID m_PlayerSteamID;
		TeamShareResult m_TeamShareResult;
		bool m_IsDead : 1;
		bool m_IsTeam : 1;
		bool m_IsSelf : 1;
	};

	class LobbyStatusFailedLine final : public ConsoleLineBase<LobbyStatusFailedLine>
	{
		using BaseClass = ConsoleLineBase;

	public:
		using ConsoleLineBase::ConsoleLineBase;
		static std::shared_ptr<IConsoleLine> TryParse(const ConsoleLineTryParseArgs& args);

		ConsoleLineType GetType() const override { return ConsoleLineType::LobbyStatusFailed; }
		bool ShouldPrint() const override { return false; }
		void Print(const PrintArgs& args) const override;
	};

	class PartyHeaderLine final : public ConsoleLineBase<PartyHeaderLine>
	{
		using BaseClass = ConsoleLineBase;

	public:
		PartyHeaderLine(time_point_t timestamp, TFParty party);
		static std::shared_ptr<IConsoleLine> TryParse(const ConsoleLineTryParseArgs& args);

		const TFParty& GetParty() const { return m_Party; }

		ConsoleLineType GetType() const override { return ConsoleLineType::PartyHeader; }
		bool ShouldPrint() const override { return false; }
		void Print(const PrintArgs& args) const override;

	private:
		TFParty m_Party{};
	};

	class LobbyHeaderLine final : public ConsoleLineBase<LobbyHeaderLine>
	{
		using BaseClass = ConsoleLineBase;

	public:
		LobbyHeaderLine(time_point_t timestamp, unsigned memberCount, unsigned pendingCount);
		static std::shared_ptr<IConsoleLine> TryParse(const ConsoleLineTryParseArgs& args);

		auto GetMemberCount() const { return m_MemberCount; }
		auto GetPendingCount() const { return m_PendingCount; }

		ConsoleLineType GetType() const override { return ConsoleLineType::LobbyHeader; }
		bool ShouldPrint() const override { return false; }
		void Print(const PrintArgs& args) const override;

	private:
		unsigned m_MemberCount;
		unsigned m_PendingCount;
	};

	class LobbyMemberLine final : public ConsoleLineBase<LobbyMemberLine>
	{
		using BaseClass = ConsoleLineBase;

	public:
		LobbyMemberLine(time_point_t timestamp, const LobbyMember& lobbyMember);
		static std::shared_ptr<IConsoleLine> TryParse(const ConsoleLineTryParseArgs& args);

		const LobbyMember& GetLobbyMember() const { return m_LobbyMember; }

		ConsoleLineType GetType() const override { return ConsoleLineType::LobbyMember; }
		bool ShouldPrint() const override { return false; }
		void Print(const PrintArgs& args) const override;

	private:
		LobbyMember m_LobbyMember;
	};

	enum class LobbyChangeType
	{
		Created,
		Updated,
		Destroyed,
	};

	class LobbyChangedLine final : public ConsoleLineBase<LobbyChangedLine>
	{
		using BaseClass = ConsoleLineBase;

	public:
		LobbyChangedLine(time_point_t timestamp, LobbyChangeType type);
		static std::shared_ptr<IConsoleLine> TryParse(const ConsoleLineTryParseArgs& args);

		ConsoleLineType GetType() const override { return ConsoleLineType::LobbyChanged; }
		LobbyChangeType GetChangeType() const { return m_ChangeType; }
		bool ShouldPrint() const override;
		void Print(const PrintArgs& args) const override;

	private:
		LobbyChangeType m_ChangeType;
	};

	class DifferingLobbyReceivedLine final : public ConsoleLineBase<DifferingLobbyReceivedLine>
	{
		using BaseClass = ConsoleLineBase;

	public:
		struct Lobby
		{
			SteamID m_LobbyID;
			uint64_t m_LobbyNumber{};
			uint32_t m_MatchID;
		};

		DifferingLobbyReceivedLine(time_point_t timestamp, const Lobby& newLobby, const Lobby& currentLobby,
			bool connectedToMatchServer, bool hasLobby, bool assignedMatchEnded);
		static std::shared_ptr<IConsoleLine> TryParse(const ConsoleLineTryParseArgs& args);

		ConsoleLineType GetType() const override { return ConsoleLineType::DifferingLobbyReceived; }
		bool ShouldPrint() const override { return false; }
		void Print(const PrintArgs& args) const override;

		const Lobby& GetNewLobby() const { return m_NewLobby; }
		const Lobby& GetCurrentLobby() const { return m_CurrentLobby; }
		bool IsConnectedToMatchServer() const { return m_ConnectedToMatchServer; }
		bool HasLobby() const { return m_HasLobby; }
		bool HasAssignedMatchEnded() const { return m_AssignedMatchEnded; }

	private:
		Lobby m_NewLobby{};
		Lobby m_CurrentLobby{};
		bool m_ConnectedToMatchServer : 1 = false;
		bool m_HasLobby : 1 = false;
		bool m_AssignedMatchEnded : 1 = false;
	};

	class ServerStatusPlayerLine final : public ConsoleLineBase<ServerStatusPlayerLine>
	{
		using BaseClass = ConsoleLineBase;

	public:
		ServerStatusPlayerLine(time_point_t timestamp, PlayerStatus playerStatus);
		static std::shared_ptr<IConsoleLine> TryParse(const ConsoleLineTryParseArgs& args);

		const PlayerStatus& GetPlayerStatus() const { return m_PlayerStatus; }

		ConsoleLineType GetType() const override { return ConsoleLineType::PlayerStatus; }
		bool ShouldPrint() const override { return false; }
		void Print(const PrintArgs& args) const override;

	private:
		PlayerStatus m_PlayerStatus;
	};

	class ServerStatusPlayerIPLine final : public ConsoleLineBase<ServerStatusPlayerIPLine>
	{
		using BaseClass = ConsoleLineBase;

	public:
		ServerStatusPlayerIPLine(time_point_t timestamp, std::string localIP, std::string publicIP);
		static std::shared_ptr<IConsoleLine> TryParse(const ConsoleLineTryParseArgs& args);

		ConsoleLineType GetType() const override { return ConsoleLineType::PlayerStatusIP; }
		bool ShouldPrint() const override { return false; }
		void Print(const PrintArgs& args) const override;

		const std::string& GetLocalIP() const { return m_LocalIP; }
		const std::string& GetPublicIP() const { return m_PublicIP; }

	private:
		std::string m_LocalIP;
		std::string m_PublicIP;
	};

	class ServerStatusShortPlayerLine final : public ConsoleLineBase<ServerStatusShortPlayerLine>
	{
		using BaseClass = ConsoleLineBase;

	public:
		ServerStatusShortPlayerLine(time_point_t timestamp, PlayerStatusShort playerStatus);
		static std::shared_ptr<IConsoleLine> TryParse(const ConsoleLineTryParseArgs& args);

		const PlayerStatusShort& GetPlayerStatus() const { return m_PlayerStatus; }

		ConsoleLineType GetType() const override { return ConsoleLineType::PlayerStatusShort; }
		bool ShouldPrint() const override { return false; }
		void Print(const PrintArgs& args) const override;

	private:
		PlayerStatusShort m_PlayerStatus;
	};

	class ServerStatusPlayerCountLine final : public ConsoleLineBase<ServerStatusPlayerCountLine>
	{
		using BaseClass = ConsoleLineBase;

	public:
		ServerStatusPlayerCountLine(time_point_t timestamp, uint8_t playerCount,
			uint8_t botCount, uint8_t maxPlayers);
		static std::shared_ptr<IConsoleLine> TryParse(const ConsoleLineTryParseArgs& args);

		uint8_t GetPlayerCount() const { return m_PlayerCount; }
		uint8_t GetBotCount() const { return m_BotCount; }
		uint8_t GetMaxPlayerCount() const { return m_MaxPlayers; }

		ConsoleLineType GetType() const override { return ConsoleLineType::PlayerStatusCount; }
		bool ShouldPrint() const override { return false; }
		void Print(const PrintArgs& args) const override;

	private:
		uint8_t m_PlayerCount;
		uint8_t m_BotCount;
		uint8_t m_MaxPlayers;
	};

	class ServerStatusMapLine final : public ConsoleLineBase<ServerStatusMapLine>
	{
		using BaseClass = ConsoleLineBase;

	public:
		ServerStatusMapLine(time_point_t timestamp, std::string mapName, const std::array<float, 3>& position);
		static std::shared_ptr<IConsoleLine> TryParse(const ConsoleLineTryParseArgs& args);

		const std::string& GetMapName() const { return m_MapName; }
		const std::array<float, 3>& GetPosition() const { return m_Position; }

		ConsoleLineType GetType() const override { return ConsoleLineType::PlayerStatusMapPosition; }
		bool ShouldPrint() const override { return false; }
		void Print(const PrintArgs& args) const override;

	private:
		std::string m_MapName;
		std::array<float, 3> m_Position{};
	};

	class EdictUsageLine final : public ConsoleLineBase<EdictUsageLine>
	{
		using BaseClass = ConsoleLineBase;

	public:
		EdictUsageLine(time_point_t timestamp, uint16_t usedEdicts, uint16_t totalEdicts);
		static std::shared_ptr<IConsoleLine> TryParse(const ConsoleLineTryParseArgs& args);

		uint16_t GetUsedEdicts() const { return m_UsedEdicts; }
		uint16_t GetTotalEdicts() const { return m_TotalEdicts; }

		ConsoleLineType GetType() const override { return ConsoleLineType::EdictUsage; }
		bool ShouldPrint() const override { return false; }
		void Print(const PrintArgs& args) const override;

	private:
		uint16_t m_UsedEdicts;
		uint16_t m_TotalEdicts;
	};

	class ClientReachedServerSpawnLine final : public ConsoleLineBase<ClientReachedServerSpawnLine>
	{
		using BaseClass = ConsoleLineBase;

	public:
		using ConsoleLineBase::ConsoleLineBase;
		static std::shared_ptr<IConsoleLine> TryParse(const ConsoleLineTryParseArgs& args);

		ConsoleLineType GetType() const override { return ConsoleLineType::ClientReachedServerSpawn; }
		bool ShouldPrint() const override { return false; }
		void Print(const PrintArgs& args) const override;
	};

	class KillNotificationLine final : public ConsoleLineBase<KillNotificationLine>
	{
		using BaseClass = ConsoleLineBase;

	public:
		KillNotificationLine(time_point_t timestamp, std::string attackerName,
			std::string victimName, std::string weaponName, bool wasCrit);
		static std::shared_ptr<IConsoleLine> TryParse(const ConsoleLineTryParseArgs& args);

		const std::string& GetVictimName() const { return m_VictimName; }
		const std::string& GetAttackerName() const { return m_AttackerName; }
		const std::string& GetWeaponName() const { return m_WeaponName; }
		bool WasCrit() const { return m_WasCrit; }

		ConsoleLineType GetType() const override { return ConsoleLineType::KillNotification; }
		bool ShouldPrint() const override { return false; }
		void Print(const PrintArgs& args) const override;

	private:
		std::string m_AttackerName;
		std::string m_VictimName;
		std::string m_WeaponName;
		bool m_WasCrit;
	};

	class CvarlistConvarLine final : public ConsoleLineBase<CvarlistConvarLine>
	{
		using BaseClass = ConsoleLineBase;

	public:
		CvarlistConvarLine(time_point_t timestamp, std::string name, float value, std::string flagsList, std::string helpText);
		static std::shared_ptr<IConsoleLine> TryParse(const ConsoleLineTryParseArgs& args);

		const std::string& GetConvarName() const { return m_Name; }
		float GetConvarValue() const { return m_Value; }
		const std::string& GetFlagsListString() const { return m_FlagsList; }
		const std::string& GetHelpText() const { return m_HelpText; }

		ConsoleLineType GetType() const override { return ConsoleLineType::CvarlistConvar; }
		bool ShouldPrint() const override { return false; }
		void Print(const PrintArgs& args) const override;

	private:
		std::string m_Name;
		float m_Value;
		std::string m_FlagsList;
		std::string m_HelpText;
	};

	class VoiceReceiveLine final : public ConsoleLineBase<VoiceReceiveLine>
	{
		using BaseClass = ConsoleLineBase;

	public:
		VoiceReceiveLine(time_point_t timestamp, uint8_t channel, uint8_t entindex, uint16_t bufSize);
		static std::shared_ptr<IConsoleLine> TryParse(const ConsoleLineTryParseArgs& args);

		uint8_t GetEntIndex() const { return m_Entindex; }

		ConsoleLineType GetType() const override { return ConsoleLineType::VoiceReceive; }
		bool ShouldPrint() const override { return false; }
		void Print(const PrintArgs& args) const override;

	private:
		uint8_t m_Channel;
		uint8_t m_Entindex;
		uint16_t m_BufSize;
	};

	class PingLine final : public ConsoleLineBase<PingLine>
	{
		using BaseClass = ConsoleLineBase;

	public:
		PingLine(time_point_t timestamp, uint16_t ping, std::string playerName);
		static std::shared_ptr<IConsoleLine> TryParse(const ConsoleLineTryParseArgs& args);

		ConsoleLineType GetType() const override { return ConsoleLineType::Ping; }
		bool ShouldPrint() const override { return false; }
		void Print(const PrintArgs& args) const override;

		uint16_t GetPing() const { return m_Ping; }
		const std::string& GetPlayerName() const { return m_PlayerName; }

	private:
		uint16_t m_Ping{};
		std::string m_PlayerName;
	};

	class SVCUserMessageLine final : public ConsoleLineBase<SVCUserMessageLine>
	{
		using BaseClass = ConsoleLineBase;

	public:
		SVCUserMessageLine(time_point_t timestamp, std::string address, UserMessageType type, uint16_t bytes);
		static std::shared_ptr<IConsoleLine> TryParse(const ConsoleLineTryParseArgs& args);

		ConsoleLineType GetType() const override { return ConsoleLineType::SVC_UserMessage; }
		bool ShouldPrint() const override;
		void Print(const PrintArgs& args) const override;

		const std::string& GetAddress() const { return m_Address; }
		UserMessageType GetUserMessageType() const { return m_MsgType; }
		uint16_t GetUserMessageBytes() const { return m_MsgBytes; }

	private:
		static bool IsSpecial(UserMessageType type);

		std::string m_Address{};
		UserMessageType m_MsgType{};
		uint16_t m_MsgBytes{};
	};

	class ConfigExecLine final : public ConsoleLineBase<ConfigExecLine>
	{
		using BaseClass = ConsoleLineBase;

	public:
		ConfigExecLine(time_point_t timestamp, std::string configFileName, bool success);
		static std::shared_ptr<IConsoleLine> TryParse(const ConsoleLineTryParseArgs& args);

		ConsoleLineType GetType() const override { return ConsoleLineType::ConfigExec; }
		bool ShouldPrint() const override { return false; }
		void Print(const PrintArgs& args) const override;

		const std::string& GetConfigFileName() const { return m_ConfigFileName; }
		bool IsSuccessful() const { return m_Success; }

	private:
		std::string m_ConfigFileName;
		bool m_Success = false;
	};

	class TeamsSwitchedLine final : public ConsoleLineBase<TeamsSwitchedLine>
	{
		using BaseClass = ConsoleLineBase;

	public:
		TeamsSwitchedLine(time_point_t timestamp) : BaseClass(timestamp) {}
		static std::shared_ptr<IConsoleLine> TryParse(const ConsoleLineTryParseArgs& args);

		ConsoleLineType GetType() const override { return ConsoleLineType::TeamsSwitched; }
		bool ShouldPrint() const override;
		void Print(const PrintArgs& args) const override;

		const std::string& GetConfigFileName() const { return m_ConfigFileName; }
		bool IsSuccessful() const { return m_Success; }

	private:
		std::string m_ConfigFileName;
		bool m_Success = false;
	};

	class ConnectingLine final : public ConsoleLineBase<ConnectingLine>
	{
		using BaseClass = ConsoleLineBase;

	public:
		ConnectingLine(time_point_t timestamp, std::string address, bool isMatchmaking, bool isRetrying);
		static std::shared_ptr<IConsoleLine> TryParse(const ConsoleLineTryParseArgs& args);

		ConsoleLineType GetType() const override { return ConsoleLineType::Connecting; }
		bool ShouldPrint() const override { return false; }
		void Print(const PrintArgs& args) const override;

		const std::string& GetAddress() const { return m_Address; }

	private:
		std::string m_Address;
		bool m_IsMatchmaking : 1;
		bool m_IsRetrying : 1;
	};

	class HostNewGameLine final : public ConsoleLineBase<HostNewGameLine>
	{
		using BaseClass = ConsoleLineBase;

	public:
		HostNewGameLine(time_point_t timestamp) : BaseClass(timestamp) {}
		static std::shared_ptr<IConsoleLine> TryParse(const ConsoleLineTryParseArgs& args);

		ConsoleLineType GetType() const override { return ConsoleLineType::HostNewGame; }
		bool ShouldPrint() const override { return false; }
		void Print(const PrintArgs& args) const override;
	};

	class GameQuitLine final : public ConsoleLineBase<GameQuitLine>
	{
		using BaseClass = ConsoleLineBase;

	public:
		GameQuitLine(time_point_t timestamp) : BaseClass(timestamp) {}
		static std::shared_ptr<IConsoleLine> TryParse(const ConsoleLineTryParseArgs& args);

		ConsoleLineType GetType() const override { return ConsoleLineType::GameQuit; }
		bool ShouldPrint() const override { return false; }
		void Print(const PrintArgs& args) const override;
	};

	class QueueStateChangeLine final : public ConsoleLineBase<QueueStateChangeLine>
	{
		using BaseClass = ConsoleLineBase;

	public:
		QueueStateChangeLine(time_point_t timestamp, TFMatchGroup queueType, TFQueueStateChange stateChange);
		static std::shared_ptr<IConsoleLine> TryParse(const ConsoleLineTryParseArgs& args);

		ConsoleLineType GetType() const override { return ConsoleLineType::QueueStateChange; }
		bool ShouldPrint() const override { return false; }
		void Print(const PrintArgs& args) const override;

		TFMatchGroup GetQueueType() const { return m_QueueType; }
		TFQueueStateChange GetStateChange() const { return m_StateChange; }

	private:
		TFMatchGroup m_QueueType{};
		TFQueueStateChange m_StateChange{};
	};

	class InQueueLine final : public ConsoleLineBase<InQueueLine>
	{
		using BaseClass = ConsoleLineBase;

	public:
		InQueueLine(time_point_t timestamp, TFMatchGroup queueType, time_point_t queueStartTime);
		static std::shared_ptr<IConsoleLine> TryParse(const ConsoleLineTryParseArgs& args);

		ConsoleLineType GetType() const override { return ConsoleLineType::InQueue; }
		bool ShouldPrint() const override { return false; }
		void Print(const PrintArgs& args) const override;

		TFMatchGroup GetQueueType() const { return m_QueueType; }
		time_point_t GetQueueStartTime() const { return m_QueueStartTime; }

	private:
		TFMatchGroup m_QueueType{};
		time_point_t m_QueueStartTime{};
	};

	class ServerJoinLine final : public ConsoleLineBase<ServerJoinLine>
	{
		using BaseClass = ConsoleLineBase;

	public:
		ServerJoinLine(time_point_t timestamp, std::string hostName, std::string mapName,
			uint8_t playerCount, uint8_t playerMaxCount, uint32_t buildNumber, uint32_t serverNumber);
		static std::shared_ptr<IConsoleLine> TryParse(const ConsoleLineTryParseArgs& args);

		ConsoleLineType GetType() const override { return ConsoleLineType::ServerJoin; }
		bool ShouldPrint() const override { return false; }
		void Print(const PrintArgs& args) const override;

		const std::string& GetHostName() const { return m_HostName; }
		const std::string& GetMapName() const { return m_MapName; }
		uint32_t GetBuildNumber() const { return m_BuildNumber; }
		uint32_t GetServerNumber() const { return m_ServerNumber; }
		uint8_t GetPlayerCount() const { return m_PlayerCount; }
		uint8_t GetPlayerMaxCount() const { return m_PlayerMaxCount; }

	private:
		std::string m_HostName;
		std::string m_MapName;
		uint32_t m_BuildNumber{};
		uint32_t m_ServerNumber{};
		uint8_t m_PlayerCount{};
		uint8_t m_PlayerMaxCount{};
	};

	class ServerDroppedPlayerLine final : public ConsoleLineBase<ServerDroppedPlayerLine>
	{
		using BaseClass = ConsoleLineBase;

	public:
		ServerDroppedPlayerLine(time_point_t timestamp, std::string playerName, std::string reason);
		static std::shared_ptr<IConsoleLine> TryParse(const ConsoleLineTryParseArgs& args);

		ConsoleLineType GetType() const override { return ConsoleLineType::ServerDroppedPlayer; }
		bool ShouldPrint() const override { return false; }
		void Print(const PrintArgs& args) const override;

		const std::string& GetPlayerName() const { return m_PlayerName; }
		const std::string& GetReason() const { return m_Reason; }

	private:
		std::string m_PlayerName;
		std::string m_Reason;
	};

	class MatchmakingBannedTimeLine final : public ConsoleLineBase<MatchmakingBannedTimeLine>
	{
		using BaseClass = ConsoleLineBase;

	public:
		enum class LadderType
		{
			Casual,
			Competitive,
		};

		MatchmakingBannedTimeLine(time_point_t timestamp, LadderType ladderType, uint64_t bannedTime);
		static std::shared_ptr<IConsoleLine> TryParse(const ConsoleLineTryParseArgs& args);

		ConsoleLineType GetType() const override { return ConsoleLineType::MatchmakingBannedTime; }
		bool ShouldPrint() const override { return false; }
		void Print(const PrintArgs& args) const override;

		const LadderType GetLadderType() const { return m_LadderType; }
		const uint64_t GetBannedTime() const { return m_BannedTime; }

	private:
		LadderType m_LadderType;
		uint64_t m_BannedTime;
	};
}

MH_ENUM_REFLECT_BEGIN(tf2_bot_detector::MatchmakingBannedTimeLine::LadderType)
	MH_ENUM_REFLECT_VALUE(Casual)
	MH_ENUM_REFLECT_VALUE(Competitive)
MH_ENUM_REFLECT_END()
