#pragma once

#include "LobbyMember.h"
#include "PlayerStatus.h"

#include <ctime>
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
		ClientReachedServerSpawn,
		KillNotification,
	};

	class IConsoleLine
	{
	public:
		IConsoleLine(std::time_t timestamp);
		virtual ~IConsoleLine() = default;

		virtual ConsoleLineType GetType() const = 0;
		virtual bool ShouldPrint() const { return true; }
		virtual void Print() const = 0;

		static std::unique_ptr<IConsoleLine> ParseConsoleLine(const std::string_view& text, std::time_t timestamp);

		std::time_t GetTimestamp() const { return m_Timestamp; }

	private:
		std::time_t m_Timestamp;
	};

	class GenericConsoleLine final : public IConsoleLine
	{
	public:
		GenericConsoleLine(std::time_t timestamp, std::string&& text);

		ConsoleLineType GetType() const override { return ConsoleLineType::Generic; }
		bool ShouldPrint() const override { return false; }
		void Print() const override;

	private:
		std::string m_Text;
	};

	class ChatConsoleLine final : public IConsoleLine
	{
	public:
		ChatConsoleLine(std::time_t timestamp, std::string&& playerName, std::string&& message, bool isDead, bool isTeam);

		ConsoleLineType GetType() const override { return ConsoleLineType::Chat; }
		void Print() const override;

	private:
		std::string m_PlayerName;
		std::string m_Message;
		bool m_IsDead : 1;
		bool m_IsTeam : 1;
	};

	class LobbyHeaderLine final : public IConsoleLine
	{
	public:
		LobbyHeaderLine(std::time_t timestamp, unsigned memberCount, unsigned pendingCount);

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
		LobbyMemberLine(std::time_t timestamp, const LobbyMember& lobbyMember);

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
		ServerStatusPlayerLine(std::time_t timestamp, const PlayerStatus& playerStatus);

		const PlayerStatus& GetPlayerStatus() const { return m_PlayerStatus; }

		ConsoleLineType GetType() const override { return ConsoleLineType::PlayerStatus; }
		bool ShouldPrint() const override { return false; }
		void Print() const override;

	private:
		PlayerStatus m_PlayerStatus;
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
		KillNotificationLine(std::time_t timestamp, std::string attackerName,
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
}
