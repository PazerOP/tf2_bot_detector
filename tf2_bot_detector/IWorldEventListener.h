#pragma once

#include "Clock.h"

#include <string_view>

namespace tf2_bot_detector
{
	class IPlayer;
	class WorldState;

	class IWorldEventListener
	{
	public:
		virtual ~IWorldEventListener() = default;

		virtual void OnTimestampUpdate(WorldState& world) = 0;
		virtual void OnPlayerStatusUpdate(WorldState& world, const IPlayer& player) = 0;
		virtual void OnChatMsg(WorldState& world, IPlayer& player, const std::string_view& msg) = 0;
	};

	class BaseWorldEventListener : public IWorldEventListener
	{
	public:
		void OnTimestampUpdate(WorldState& world) override {}
		void OnPlayerStatusUpdate(WorldState& world, const IPlayer& player) override {}
		void OnChatMsg(WorldState& world, IPlayer& player, const std::string_view& msg) override {}
	};
}
