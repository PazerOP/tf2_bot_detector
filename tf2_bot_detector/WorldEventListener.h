#pragma once

#include "Clock.h"

#include <string_view>

namespace tf2_bot_detector
{
	class IPlayer;
	class IWorldState;
	enum class TFClassType;

	class IWorldEventListener
	{
	public:
		virtual ~IWorldEventListener() = default;

		virtual void OnTimestampUpdate(IWorldState& world) = 0;
		virtual void OnPlayerStatusUpdate(IWorldState& world, const IPlayer& player) = 0;
		virtual void OnChatMsg(IWorldState& world, IPlayer& player, const std::string_view& msg) = 0;
		virtual void OnLocalPlayerInitialized(IWorldState& world, bool initialized) = 0;
		virtual void OnLocalPlayerSpawned(IWorldState& world, TFClassType classType) = 0;
		virtual void OnPlayerDroppedFromServer(IWorldState& world, IPlayer& player, const std::string_view& reason) = 0;
	};

	class BaseWorldEventListener : public IWorldEventListener
	{
	public:
		void OnTimestampUpdate(IWorldState& world) override {}
		void OnPlayerStatusUpdate(IWorldState& world, const IPlayer& player) override {}
		void OnChatMsg(IWorldState& world, IPlayer& player, const std::string_view& msg) override {}
		void OnLocalPlayerInitialized(IWorldState& world, bool initialized) override {}
		void OnLocalPlayerSpawned(IWorldState& world, TFClassType classType) override {}
		void OnPlayerDroppedFromServer(IWorldState& world, IPlayer& player, const std::string_view& reason) override {}
	};

	class AutoWorldEventListener : public BaseWorldEventListener
	{
	public:
		AutoWorldEventListener(IWorldState& world);
		AutoWorldEventListener(const AutoWorldEventListener& other);
		AutoWorldEventListener& operator=(const AutoWorldEventListener& other);
		AutoWorldEventListener(AutoWorldEventListener&& other);
		AutoWorldEventListener& operator=(AutoWorldEventListener&& other);
		~AutoWorldEventListener();

	private:
		IWorldState* m_World = nullptr;
	};
}
