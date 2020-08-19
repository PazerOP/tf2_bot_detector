#pragma once
#ifdef TF2BD_ENABLE_DISCORD_INTEGRATION

#include <memory>

namespace tf2_bot_detector
{
	class Settings;
	class IWorldState;

	class IDRPManager
	{
	public:
		virtual ~IDRPManager() = default;
		virtual void Update() = 0;

		static std::unique_ptr<IDRPManager> Create(const Settings& settings, IWorldState& world);
	};
}

#endif
