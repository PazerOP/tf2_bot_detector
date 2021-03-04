#include "Config/Rules.h"
#include "IPlayer.h"

#include <mh/error/not_implemented_error.hpp>
#include <mh/text/codecvt.hpp>

#include <catch2/catch.hpp>

using namespace std::string_view_literals;
using namespace tf2_bot_detector;

namespace
{
	struct MockPlayer : IPlayer
	{
		std::string m_Name;

		const IWorldState& GetWorld() const override { throw mh::not_implemented_error(); }

		// Inherited via IPlayer
		const LobbyMember* GetLobbyMember() const override
		{
			throw mh::not_implemented_error();
		}
		std::string GetNameUnsafe() const override { return m_Name; }
		SteamID GetSteamID() const override
		{
			throw mh::not_implemented_error();
		}
		const mh::expected<SteamAPI::PlayerSummary, std::error_condition>& GetPlayerSummary() const override
		{
			throw mh::not_implemented_error();
		}
		const mh::expected<SteamAPI::PlayerBans, std::error_condition>& GetPlayerBans() const override
		{
			throw mh::not_implemented_error();
		}
		mh::expected<duration_t, std::error_condition> GetTF2Playtime() const override
		{
			throw mh::not_implemented_error();
		}
		bool IsFriend() const override
		{
			throw mh::not_implemented_error();
		}
		std::optional<UserID_t> GetUserID() const override
		{
			throw mh::not_implemented_error();
		}
		PlayerStatusState GetConnectionState() const override
		{
			throw mh::not_implemented_error();
		}
		time_point_t GetConnectionTime() const override
		{
			throw mh::not_implemented_error();
		}
		duration_t GetConnectedTime() const override
		{
			throw mh::not_implemented_error();
		}
		TFTeam GetTeam() const override
		{
			throw mh::not_implemented_error();
		}
		const PlayerScores& GetScores() const override
		{
			throw mh::not_implemented_error();
		}
		uint16_t GetPing() const override
		{
			throw mh::not_implemented_error();
		}
		time_point_t GetLastStatusUpdateTime() const override
		{
			throw mh::not_implemented_error();
		}
		duration_t GetActiveTime() const override
		{
			throw mh::not_implemented_error();
		}
		std::any& GetOrCreateDataStorage(const std::type_index& type) override
		{
			throw mh::not_implemented_error();
		}
		const std::any* FindDataStorage(const std::type_index& type) const override
		{
			throw mh::not_implemented_error();
		}
		std::optional<time_point_t> GetEstimatedAccountCreationTime() const override
		{
			throw mh::not_implemented_error();
		}
		const mh::expected<LogsTFAPI::PlayerLogsInfo>& GetLogsInfo() const override
		{
			throw mh::not_implemented_error();
		}
		const mh::expected<SteamAPI::PlayerInventoryInfo>& GetInventoryInfo() const override
		{
			throw mh::not_implemented_error();
		}
	};
}

TEST_CASE("Player Rules - ends_with", "[PlayerRuleTests]")
{
	MockPlayer player;
	player.m_Name = "Special Gamer";

	ModerationRule rule;
	rule.m_Description = "test rule - ends_with";

	auto& usernameTextMatch = rule.m_Triggers.m_UsernameTextMatch.emplace();
	usernameTextMatch.m_Mode = TextMatchMode::EndsWith;

	usernameTextMatch.m_Patterns = { "Special Gamer" };
	REQUIRE(rule.Match(player));

	usernameTextMatch.m_Patterns = { "Super Special Gamer" };
	REQUIRE(!rule.Match(player));

	usernameTextMatch.m_Patterns = { "Gamer" };
	REQUIRE(rule.Match(player));

	usernameTextMatch.m_Patterns = { "Gamers" };
	REQUIRE(!rule.Match(player));

	usernameTextMatch.m_Patterns = { "Gamer", "Gamers" };
	REQUIRE(rule.Match(player));

	usernameTextMatch.m_Patterns = { "r" };
	REQUIRE(rule.Match(player));
}

TEST_CASE("Player Rules - chatmsg contains", "[PlayerRuleTests]")
{
	MockPlayer player;
	player.m_Name = "Special Gamer";

	const auto chatMsg = mh::change_encoding<char>(u8"Mean words!!!!!!!!!!!!!!! 😡");

	ModerationRule rule;

	SECTION("match any")
	{
		rule.m_Triggers.m_Mode = TriggerMatchMode::MatchAny;
	}
	SECTION("match all")
	{
		rule.m_Triggers.m_Mode = TriggerMatchMode::MatchAll;
	}

	auto& textMatch = rule.m_Triggers.m_ChatMsgTextMatch.emplace();
	textMatch.m_Mode = TextMatchMode::Contains;

	textMatch.m_Patterns = { "text" };
	REQUIRE(!rule.Match(player, chatMsg));

	textMatch.m_Patterns = { "ean" };
	REQUIRE(rule.Match(player, chatMsg));
}

TEST_CASE("Player Rules - chatmsg word", "[PlayerRuleTests]")
{
	MockPlayer player;
	player.m_Name = "Special Gamer";

	const auto chatMsg = mh::change_encoding<char>(u8"you are stinky");

	ModerationRule rule;

	SECTION("match any")
	{
		rule.m_Triggers.m_Mode = TriggerMatchMode::MatchAny;
	}
	SECTION("match all")
	{
		rule.m_Triggers.m_Mode = TriggerMatchMode::MatchAll;
	}

	auto& textMatch = rule.m_Triggers.m_ChatMsgTextMatch.emplace();
	textMatch.m_Mode = TextMatchMode::Word;

	textMatch.m_Patterns = { "you" };
	REQUIRE(rule.Match(player, chatMsg));

	textMatch.m_Patterns = { "are" };
	REQUIRE(rule.Match(player, chatMsg));

	textMatch.m_Patterns = { "stinky" };
	REQUIRE(rule.Match(player, chatMsg));

	textMatch.m_Patterns = { "smelly" };
	REQUIRE(!rule.Match(player, chatMsg));
}
