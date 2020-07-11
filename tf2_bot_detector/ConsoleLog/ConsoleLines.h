#pragma once

#include "Clock.h"
#include "LobbyMember.h"
#include "PlayerStatus.h"
#include "IConsoleLine.h"

#include <memory>
#include <string_view>

namespace tf2_bot_detector
{
	enum class TFClassType;
	enum class UserMessageType;

	class GenericConsoleLine final : public ConsoleLineBase<GenericConsoleLine, false>
	{
		using BaseClass = ConsoleLineBase;

	public:
		GenericConsoleLine(time_point_t timestamp, std::string text);
		static std::shared_ptr<IConsoleLine> TryParse(const std::string_view& text, time_point_t timestamp);

		ConsoleLineType GetType() const override { return ConsoleLineType::Generic; }
		bool ShouldPrint() const override { return false; }
		void Print() const override;

	private:
		std::string m_Text;
	};

	class ChatConsoleLine final : public ConsoleLineBase<ChatConsoleLine, false>
	{
		using BaseClass = ConsoleLineBase;

	public:
		ChatConsoleLine(time_point_t timestamp, std::string playerName, std::string message, bool isDead, bool isTeam);
		static std::shared_ptr<IConsoleLine> TryParse(const std::string_view& text, time_point_t timestamp);
		static std::shared_ptr<ChatConsoleLine> TryParseFlexible(const std::string_view& text, time_point_t timestamp);

		ConsoleLineType GetType() const override { return ConsoleLineType::Chat; }
		void Print() const override;

		const std::string& GetPlayerName() const { return m_PlayerName; }
		const std::string& GetMessage() const { return m_Message; }
		bool IsDead() const { return m_IsDead; }
		bool IsTeam() const { return m_IsTeam; }

	private:
		static std::shared_ptr<ChatConsoleLine> TryParse(const std::string_view& text, time_point_t timestamp, bool flexible);

		std::string m_PlayerName;
		std::string m_Message;
		bool m_IsDead : 1;
		bool m_IsTeam : 1;
	};

	class LobbyStatusFailedLine final : public ConsoleLineBase<LobbyStatusFailedLine>
	{
		using BaseClass = ConsoleLineBase;

	public:
		using ConsoleLineBase::ConsoleLineBase;
		static std::shared_ptr<IConsoleLine> TryParse(const std::string_view& text, time_point_t timestamp);

		ConsoleLineType GetType() const override { return ConsoleLineType::LobbyStatusFailed; }
		bool ShouldPrint() const override { return false; }
		void Print() const override;
	};

	class LobbyHeaderLine final : public ConsoleLineBase<LobbyHeaderLine>
	{
		using BaseClass = ConsoleLineBase;

	public:
		LobbyHeaderLine(time_point_t timestamp, unsigned memberCount, unsigned pendingCount);
		static std::shared_ptr<IConsoleLine> TryParse(const std::string_view& text, time_point_t timestamp);

		auto GetMemberCount() const { return m_MemberCount; }
		auto GetPendingCount() const { return m_PendingCount; }

		ConsoleLineType GetType() const override { return ConsoleLineType::LobbyHeader; }
		bool ShouldPrint() const override { return false; }
		void Print() const override;

	private:
		unsigned m_MemberCount;
		unsigned m_PendingCount;
	};

	class LobbyMemberLine final : public ConsoleLineBase<LobbyMemberLine>
	{
		using BaseClass = ConsoleLineBase;

	public:
		LobbyMemberLine(time_point_t timestamp, const LobbyMember& lobbyMember);
		static std::shared_ptr<IConsoleLine> TryParse(const std::string_view& text, time_point_t timestamp);

		const LobbyMember& GetLobbyMember() const { return m_LobbyMember; }

		ConsoleLineType GetType() const override { return ConsoleLineType::LobbyMember; }
		bool ShouldPrint() const override { return false; }
		void Print() const override;

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
		static std::shared_ptr<IConsoleLine> TryParse(const std::string_view& text, time_point_t timestamp);

		ConsoleLineType GetType() const override { return ConsoleLineType::LobbyChanged; }
		LobbyChangeType GetChangeType() const { return m_ChangeType; }
		bool ShouldPrint() const override;
		void Print() const override;

	private:
		LobbyChangeType m_ChangeType;
	};

	class ServerStatusPlayerLine final : public ConsoleLineBase<ServerStatusPlayerLine>
	{
		using BaseClass = ConsoleLineBase;

	public:
		ServerStatusPlayerLine(time_point_t timestamp, PlayerStatus playerStatus);
		static std::shared_ptr<IConsoleLine> TryParse(const std::string_view& text, time_point_t timestamp);

		const PlayerStatus& GetPlayerStatus() const { return m_PlayerStatus; }

		ConsoleLineType GetType() const override { return ConsoleLineType::PlayerStatus; }
		bool ShouldPrint() const override { return false; }
		void Print() const override;

	private:
		PlayerStatus m_PlayerStatus;
	};

	class ServerStatusShortPlayerLine final : public ConsoleLineBase<ServerStatusShortPlayerLine>
	{
		using BaseClass = ConsoleLineBase;

	public:
		ServerStatusShortPlayerLine(time_point_t timestamp, PlayerStatusShort playerStatus);
		static std::shared_ptr<IConsoleLine> TryParse(const std::string_view& text, time_point_t timestamp);

		const PlayerStatusShort& GetPlayerStatus() const { return m_PlayerStatus; }

		ConsoleLineType GetType() const override { return ConsoleLineType::PlayerStatusShort; }
		bool ShouldPrint() const override { return false; }
		void Print() const override;

	private:
		PlayerStatusShort m_PlayerStatus;
	};

	class ServerStatusPlayerCountLine final : public ConsoleLineBase<ServerStatusPlayerCountLine>
	{
		using BaseClass = ConsoleLineBase;

	public:
		ServerStatusPlayerCountLine(time_point_t timestamp, uint8_t playerCount,
			uint8_t botCount, uint8_t maxPlayers);
		static std::shared_ptr<IConsoleLine> TryParse(const std::string_view& text, time_point_t timestamp);

		uint8_t GetPlayerCount() const { return m_PlayerCount; }
		uint8_t GetBotCount() const { return m_BotCount; }
		uint8_t GetMaxPlayerCount() const { return m_MaxPlayers; }

		ConsoleLineType GetType() const override { return ConsoleLineType::PlayerStatusCount; }
		bool ShouldPrint() const override { return false; }
		void Print() const override;

	private:
		uint8_t m_PlayerCount;
		uint8_t m_BotCount;
		uint8_t m_MaxPlayers;
	};

	class EdictUsageLine final : public ConsoleLineBase<EdictUsageLine>
	{
		using BaseClass = ConsoleLineBase;

	public:
		EdictUsageLine(time_point_t timestamp, uint16_t usedEdicts, uint16_t totalEdicts);
		static std::shared_ptr<IConsoleLine> TryParse(const std::string_view& text, time_point_t timestamp);

		uint16_t GetUsedEdicts() const { return m_UsedEdicts; }
		uint16_t GetTotalEdicts() const { return m_TotalEdicts; }

		ConsoleLineType GetType() const override { return ConsoleLineType::EdictUsage; }
		bool ShouldPrint() const override { return false; }
		void Print() const override;

	private:
		uint16_t m_UsedEdicts;
		uint16_t m_TotalEdicts;
	};

	class ClientReachedServerSpawnLine final : public ConsoleLineBase<ClientReachedServerSpawnLine>
	{
		using BaseClass = ConsoleLineBase;

	public:
		using ConsoleLineBase::ConsoleLineBase;
		static std::shared_ptr<IConsoleLine> TryParse(const std::string_view& text, time_point_t timestamp);

		ConsoleLineType GetType() const override { return ConsoleLineType::ClientReachedServerSpawn; }
		bool ShouldPrint() const override { return false; }
		void Print() const override;
	};

	class KillNotificationLine final : public ConsoleLineBase<KillNotificationLine>
	{
		using BaseClass = ConsoleLineBase;

	public:
		KillNotificationLine(time_point_t timestamp, std::string attackerName,
			std::string victimName, std::string weaponName, bool wasCrit);
		static std::shared_ptr<IConsoleLine> TryParse(const std::string_view& text, time_point_t timestamp);

		const std::string& GetVictimName() const { return m_VictimName; }
		const std::string& GetAttackerName() const { return m_AttackerName; }
		const std::string& GetWeaponName() const { return m_WeaponName; }
		bool WasCrit() const { return m_WasCrit; }

		ConsoleLineType GetType() const override { return ConsoleLineType::KillNotification; }
		bool ShouldPrint() const override { return false; }
		void Print() const override;

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
		static std::shared_ptr<IConsoleLine> TryParse(const std::string_view& text, time_point_t timestamp);

		const std::string& GetConvarName() const { return m_Name; }
		float GetConvarValue() const { return m_Value; }
		const std::string& GetFlagsListString() const { return m_FlagsList; }
		const std::string& GetHelpText() const { return m_HelpText; }

		ConsoleLineType GetType() const override { return ConsoleLineType::CvarlistConvar; }
		bool ShouldPrint() const override { return false; }
		void Print() const override;

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
		static std::shared_ptr<IConsoleLine> TryParse(const std::string_view& text, time_point_t timestamp);

		uint8_t GetEntIndex() const { return m_Entindex; }

		ConsoleLineType GetType() const override { return ConsoleLineType::VoiceReceive; }
		bool ShouldPrint() const override { return false; }
		void Print() const override;

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
		static std::shared_ptr<IConsoleLine> TryParse(const std::string_view& text, time_point_t timestamp);

		ConsoleLineType GetType() const override { return ConsoleLineType::Ping; }
		bool ShouldPrint() const override { return false; }
		void Print() const override;

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
		static std::shared_ptr<IConsoleLine> TryParse(const std::string_view& text, time_point_t timestamp);

		ConsoleLineType GetType() const override { return ConsoleLineType::SVC_UserMessage; }
		bool ShouldPrint() const override;
		void Print() const override;

		UserMessageType GetUserMessageType() const { return m_MsgType; }
		uint16_t GetUserMessageBytes() const { return m_MsgBytes; }

	private:
		std::string m_Address{};
		UserMessageType m_MsgType{};
		uint16_t m_MsgBytes{};
	};

	class ConfigExecLine final : public ConsoleLineBase<ConfigExecLine>
	{
		using BaseClass = ConsoleLineBase;

	public:
		ConfigExecLine(time_point_t timestamp, std::string configFileName, bool success);
		static std::shared_ptr<IConsoleLine> TryParse(const std::string_view& text, time_point_t timestamp);

		ConsoleLineType GetType() const override { return ConsoleLineType::ConfigExec; }
		bool ShouldPrint() const override { return false; }
		void Print() const override;

		const std::string& GetConfigFileName() const { return m_ConfigFileName; }
		bool IsSuccessful() const { return m_Success; }

	private:
		std::string m_ConfigFileName;
		bool m_Success = false;
	};
}
