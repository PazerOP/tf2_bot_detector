#pragma once

#include "Clock.h"
#include "SteamID.h"
#include "TFConstants.h"

#include <cppcoro/generator.hpp>

#include <optional>

namespace tf2_bot_detector
{
	class ChatConsoleLine;
	class ConfigExecLine;
	class ConsoleLogParser;
	class IConsoleLineListener;
	class IPlayer;
	class IWorldEventListener;
	enum class LobbyMemberTeam : uint8_t;
	class Settings;
	enum class TFClassType;

	enum class TeamShareResult
	{
		SameTeams,
		OppositeTeams,
		Neither,
	};

	class IWorldState;

	class IWorldStateConLog
	{
	public:
		virtual ~IWorldStateConLog() = default;

	private:
		friend class ConsoleLogParser;

		virtual IConsoleLineListener& GetConsoleLineListenerBroadcaster() = 0;

		virtual void UpdateTimestamp(const ConsoleLogParser& parser) = 0;
	};

	class IWorldState : public IWorldStateConLog
	{
	public:
		virtual ~IWorldState() = default;

		static std::unique_ptr<IWorldState> Create(const Settings& settings);

		virtual void Update() = 0;

		virtual time_point_t GetCurrentTime() const = 0;
		virtual time_point_t GetLastStatusUpdateTime() const = 0;

		virtual void AddWorldEventListener(IWorldEventListener* listener) = 0;
		virtual void RemoveWorldEventListener(IWorldEventListener* listener) = 0;
		virtual void AddConsoleLineListener(IConsoleLineListener* listener) = 0;
		virtual void RemoveConsoleLineListener(IConsoleLineListener* listener) = 0;

		virtual void AddConsoleOutputChunk(const std::string_view& chunk) = 0;
		virtual void AddConsoleOutputLine(const std::string_view& line) = 0;

		virtual std::optional<SteamID> FindSteamIDForName(const std::string_view& playerName) const = 0;
		virtual std::optional<LobbyMemberTeam> FindLobbyMemberTeam(const SteamID& id) const = 0;
		virtual std::optional<UserID_t> FindUserID(const SteamID& id) const = 0;

		virtual TeamShareResult GetTeamShareResult(const SteamID& id) const = 0;
		virtual TeamShareResult GetTeamShareResult(const SteamID& id0, const SteamID& id1) const = 0;
		virtual TeamShareResult GetTeamShareResult(const std::optional<LobbyMemberTeam>& team0, const SteamID& id1) const = 0;
		static TeamShareResult GetTeamShareResult(
			const std::optional<LobbyMemberTeam>& team0, const std::optional<LobbyMemberTeam>& team1);

		virtual const IPlayer* FindPlayer(const SteamID& id) const = 0;
		IPlayer* FindPlayer(const SteamID& id) { return const_cast<IPlayer*>(std::as_const(*this).FindPlayer(id)); }

		virtual size_t GetApproxLobbyMemberCount() const = 0;
		virtual cppcoro::generator<const IPlayer&> GetLobbyMembers() const = 0;
		cppcoro::generator<IPlayer&> GetLobbyMembers();
		virtual cppcoro::generator<const IPlayer&> GetPlayers() const = 0;
		cppcoro::generator<IPlayer&> GetPlayers();

		// Have we joined a team and picked a class?
		virtual bool IsLocalPlayerInitialized() const = 0;
		virtual bool IsVoteInProgress() const = 0;
	};

	inline cppcoro::generator<IPlayer&> IWorldState::GetLobbyMembers()
	{
		for (const IPlayer& p : std::as_const(*this).GetLobbyMembers())
			co_yield const_cast<IPlayer&>(p);
	}
	inline cppcoro::generator<IPlayer&> IWorldState::GetPlayers()
	{
		for (const IPlayer& p : std::as_const(*this).GetPlayers())
			co_yield const_cast<IPlayer&>(p);
	}
}
