#include "ConsoleLog/ConsoleLines.h"
#include "SteamID.h"

#include <catch2/catch.hpp>

using namespace std::chrono_literals;
using namespace tf2_bot_detector;

TEST_CASE("tf2bd_cl_status")
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
		auto parsedLine = ServerStatusPlayerLine::TryParse(test.m_StatusLine, tfbd_clock_t::now());
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
