#include "Config/PlayerListJSON.h"

#include <catch2/catch.hpp>

using namespace std::chrono_literals;
using namespace tf2_bot_detector;

TEST_CASE("mh::formatter<PlayerAttributesList>", "[tf2bd][formatting]")
{
	PlayerAttributesList list;

	list.SetAttribute(PlayerAttribute::Cheater);
	REQUIRE(mh::format("Attribs: {}", list) == "Attribs: PlayerAttribute::Cheater");
}

TEST_CASE("mh::formatter<PlayerMarks>", "[tf2bd][formatting]")
{
	PlayerMarks marks;
	marks.m_Marks.push_back(PlayerMarks::Mark({ PlayerAttribute::Suspicious, PlayerAttribute::Racist }, "cfg/playerlist.json"));

	auto fmt = mh::format("marks: {}", marks);
	REQUIRE(fmt == "marks: \n\t - \"cfg/playerlist.json\" (PlayerAttribute::Suspicious, PlayerAttribute::Racist)");
}
