#include "Clock.h"

#include <catch2/catch.hpp>

#include <sstream>

using namespace tf2_bot_detector;
using namespace std::chrono_literals;

TEST_CASE("tf2bd_humanduration", "[tf2bd]")
{
	{
		std::ostringstream ss;
		constexpr duration_t d = 5h + 16min;
		ss << HumanDuration(d);

		REQUIRE(ss.str() == "5 hours, 16 minutes");
	}

	{
		//constexpr duration_t mins = minute_t(1834409518);
		//constexpr duration_t secs = second_t(813882256308);
		//constexpr duration_t totalDuration = mins + secs;
		//ss << HumanDuration(totalDuration);

		//REQUIRE(ss.str() == "1 minute");
	}
}
