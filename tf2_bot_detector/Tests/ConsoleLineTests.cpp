#include "ConsoleLog/ConsoleLines.h"
#include "SteamID.h"
#include "WorldState.h"

#include <catch2/catch.hpp>
#include <mh/error/not_implemented_error.hpp>

using namespace std::chrono_literals;
using namespace tf2_bot_detector;

namespace
{
	class DummyWorldState : public IWorldState
	{
		// Inherited via IWorldState
		virtual IConsoleLineListener& GetConsoleLineListenerBroadcaster() override
		{
			throw mh::not_implemented_error();
		}
		virtual void UpdateTimestamp(const ConsoleLogParser& parser) override
		{
			throw mh::not_implemented_error();
		}
		virtual void Update() override
		{
			throw mh::not_implemented_error();
		}
		virtual time_point_t GetCurrentTime() const override
		{
			throw mh::not_implemented_error();
		}
		virtual time_point_t GetLastStatusUpdateTime() const override
		{
			throw mh::not_implemented_error();
		}
		virtual void AddWorldEventListener(IWorldEventListener* listener) override
		{
			throw mh::not_implemented_error();
		}
		virtual void RemoveWorldEventListener(IWorldEventListener* listener) override
		{
			throw mh::not_implemented_error();
		}
		virtual void AddConsoleLineListener(IConsoleLineListener* listener) override
		{
			throw mh::not_implemented_error();
		}
		virtual void RemoveConsoleLineListener(IConsoleLineListener* listener) override
		{
			throw mh::not_implemented_error();
		}
		virtual void AddConsoleOutputChunk(const std::string_view& chunk) override
		{
			throw mh::not_implemented_error();
		}
		virtual mh::task<> AddConsoleOutputLine(std::string line) override
		{
			throw mh::not_implemented_error();
		}
		virtual std::optional<SteamID> FindSteamIDForName(const std::string_view& playerName) const override
		{
			throw mh::not_implemented_error();
		}
		virtual std::optional<LobbyMemberTeam> FindLobbyMemberTeam(const SteamID& id) const override
		{
			throw mh::not_implemented_error();
		}
		virtual std::optional<UserID_t> FindUserID(const SteamID& id) const override
		{
			throw mh::not_implemented_error();
		}
		virtual TeamShareResult GetTeamShareResult(const SteamID& id) const override
		{
			throw mh::not_implemented_error();
		}
		virtual TeamShareResult GetTeamShareResult(const SteamID& id0, const SteamID& id1) const override
		{
			throw mh::not_implemented_error();
		}
		virtual TeamShareResult GetTeamShareResult(const std::optional<LobbyMemberTeam>& team0, const SteamID& id1) const override
		{
			throw mh::not_implemented_error();
		}
		virtual const IPlayer* FindPlayer(const SteamID& id) const override
		{
			throw mh::not_implemented_error();
		}
		virtual size_t GetApproxLobbyMemberCount() const override
		{
			throw mh::not_implemented_error();
		}
		virtual mh::generator<const IPlayer&> GetLobbyMembers() const override
		{
			throw mh::not_implemented_error();
		}
		virtual mh::generator<const IPlayer&> GetPlayers() const override
		{
			throw mh::not_implemented_error();
		}
		virtual bool IsLocalPlayerInitialized() const override
		{
			throw mh::not_implemented_error();
		}
		virtual bool IsVoteInProgress() const override
		{
			throw mh::not_implemented_error();
		}
		virtual const IAccountAges& GetAccountAges() const override
		{
			throw mh::not_implemented_error();
		}

	} static s_DummyWorldState;
}

TEST_CASE("tf2bd_cl_status", "[ConsoleLines]")
{
	struct StatusLineTest
	{
		std::string_view m_StatusLine;

		uint32_t m_ExpectedUserID;
		std::string_view m_ExpectedName;
		SteamID m_ExpectedSteamID;
		duration_t m_ExpectedConnectionTime;
		uint32_t m_ExpectedPing;
		uint32_t m_ExpectedLoss;
		PlayerStatusState m_ExpectedState;
	};

	constexpr StatusLineTest s_StatusLineTests[] =
	{
		{
			.m_StatusLine = "#    348 \"2fort\x0A closed due to COVID\x0A\" [U:1:1118537734] 00:51  157    0 active",
			.m_ExpectedUserID = 348,
			.m_ExpectedName = "2fort\x0A closed due to COVID\x0A",
			.m_ExpectedSteamID = SteamID(1118537734, SteamAccountType::Individual, SteamAccountUniverse::Public),
			.m_ExpectedConnectionTime = 51s,
			.m_ExpectedPing = 157,
			.m_ExpectedLoss = 0,
			.m_ExpectedState = PlayerStatusState::Active,
		},
		{
			.m_StatusLine = "#    348 \"2fort\x0D closed due to COVID\x0D\" [U:1:1118537734] 00:51  157    0 active",
			.m_ExpectedUserID = 348,
			.m_ExpectedName = "2fort\x0D closed due to COVID\x0D",
			.m_ExpectedSteamID = SteamID(1118537734, SteamAccountType::Individual, SteamAccountUniverse::Public),
			.m_ExpectedConnectionTime = 51s,
			.m_ExpectedPing = 157,
			.m_ExpectedLoss = 0,
			.m_ExpectedState = PlayerStatusState::Active,
		}
	};

	for (const auto& test : s_StatusLineTests)
	{
		ConsoleLineTryParseArgs args{ test.m_StatusLine, tfbd_clock_t::now(), s_DummyWorldState };

		auto parsedLine = ServerStatusPlayerLine::TryParse(args);
		REQUIRE(parsedLine);

		auto parsedStatusLine = dynamic_cast<ServerStatusPlayerLine*>(parsedLine.get());
		REQUIRE(parsedStatusLine);

		const PlayerStatus& playerStatus = parsedStatusLine->GetPlayerStatus();
		REQUIRE(playerStatus.m_UserID == test.m_ExpectedUserID);
		REQUIRE(playerStatus.m_Name == test.m_ExpectedName);
		REQUIRE(playerStatus.m_SteamID == test.m_ExpectedSteamID);
		//REQUIRE(playerStatus.m_ConnectionTime == test.m_ExpectedConnectionTime); // why did i do this...
		REQUIRE(playerStatus.m_Ping == test.m_ExpectedPing);
		REQUIRE(playerStatus.m_Loss == test.m_ExpectedLoss);
		REQUIRE(playerStatus.m_State == test.m_ExpectedState);
	}
}
