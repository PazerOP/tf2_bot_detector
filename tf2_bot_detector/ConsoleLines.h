#pragma once

#include "Clock.h"
#include "LobbyMember.h"
#include "PlayerStatus.h"

#include <memory>
#include <string_view>

namespace tf2_bot_detector
{
	enum class ConsoleLineType
	{
		Generic,
		Chat,
		LobbyDestroyed,
		LobbyHeader,
		LobbyMember,
		PlayerStatus,
		PlayerStatusShort,
		ClientReachedServerSpawn,
		KillNotification,
		CvarlistConvar,
	};

	class IConsoleLine
	{
	public:
		IConsoleLine(time_point_t timestamp);
		virtual ~IConsoleLine() = default;

		virtual ConsoleLineType GetType() const = 0;
		virtual bool ShouldPrint() const { return true; }
		virtual void Print() const = 0;

		static std::unique_ptr<IConsoleLine> ParseConsoleLine(const std::string_view& text, time_point_t timestamp);

		time_point_t GetTimestamp() const { return m_Timestamp; }

	private:
		time_point_t m_Timestamp;
	};

	class GenericConsoleLine final : public IConsoleLine
	{
	public:
		GenericConsoleLine(time_point_t timestamp, std::string&& text);

		ConsoleLineType GetType() const override { return ConsoleLineType::Generic; }
		bool ShouldPrint() const override { return false; }
		void Print() const override;

	private:
		std::string m_Text;
	};

	class ChatConsoleLine final : public IConsoleLine
	{
	public:
		ChatConsoleLine(time_point_t timestamp, std::string&& playerName, std::string&& message, bool isDead, bool isTeam);

		ConsoleLineType GetType() const override { return ConsoleLineType::Chat; }
		void Print() const override;

		const std::string& GetPlayerName() const { return m_PlayerName; }
		const std::string& GetMessage() const { return m_Message; }
		bool IsDead() const { return m_IsDead; }
		bool IsTeam() const { return m_IsTeam; }

	private:
		std::string m_PlayerName;
		std::string m_Message;
		bool m_IsDead : 1;
		bool m_IsTeam : 1;
	};

	class LobbyHeaderLine final : public IConsoleLine
	{
	public:
		LobbyHeaderLine(time_point_t timestamp, unsigned memberCount, unsigned pendingCount);

		auto GetMemberCount() const { return m_MemberCount; }
		auto GetPendingCount() const { return m_PendingCount; }

		ConsoleLineType GetType() const override { return ConsoleLineType::LobbyHeader; }
		bool ShouldPrint() const override { return false; }
		void Print() const override;

	private:
		unsigned m_MemberCount;
		unsigned m_PendingCount;
	};

	class LobbyMemberLine final : public IConsoleLine
	{
	public:
		LobbyMemberLine(time_point_t timestamp, const LobbyMember& lobbyMember);

		const LobbyMember& GetLobbyMember() const { return m_LobbyMember; }

		ConsoleLineType GetType() const override { return ConsoleLineType::LobbyMember; }
		bool ShouldPrint() const override { return false; }
		void Print() const override;

	private:
		LobbyMember m_LobbyMember;
	};

	class LobbyDestroyedLine final : public IConsoleLine
	{
	public:
		using IConsoleLine::IConsoleLine;

		ConsoleLineType GetType() const override { return ConsoleLineType::LobbyDestroyed; }
		bool ShouldPrint() const override { return true; }
		void Print() const override;
	};

	class ServerStatusPlayerLine final : public IConsoleLine
	{
	public:
		ServerStatusPlayerLine(time_point_t timestamp, PlayerStatus playerStatus);

		const PlayerStatus& GetPlayerStatus() const { return m_PlayerStatus; }

		ConsoleLineType GetType() const override { return ConsoleLineType::PlayerStatus; }
		bool ShouldPrint() const override { return false; }
		void Print() const override;

	private:
		PlayerStatus m_PlayerStatus;
	};

	class ServerStatusShortPlayerLine final : public IConsoleLine
	{
	public:
		ServerStatusShortPlayerLine(time_point_t timestamp, PlayerStatusShort playerStatus);

		const PlayerStatusShort& GetPlayerStatus() const { return m_PlayerStatus; }

		ConsoleLineType GetType() const override { return ConsoleLineType::PlayerStatusShort; }
		bool ShouldPrint() const override { return false; }
		void Print() const override;

	private:
		PlayerStatusShort m_PlayerStatus;
	};

	class ClientReachedServerSpawnLine final : public IConsoleLine
	{
	public:
		using IConsoleLine::IConsoleLine;

		ConsoleLineType GetType() const override { return ConsoleLineType::ClientReachedServerSpawn; }
		bool ShouldPrint() const override { return false; }
		void Print() const override;
	};

	class KillNotificationLine final : public IConsoleLine
	{
	public:
		KillNotificationLine(time_point_t timestamp, std::string attackerName,
			std::string victimName, std::string weaponName, bool wasCrit);

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

	class CvarlistConvarLine final : public IConsoleLine
	{
	public:
		CvarlistConvarLine(time_point_t timestamp, std::string name, float value, std::string flagsList, std::string helpText);

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
}
