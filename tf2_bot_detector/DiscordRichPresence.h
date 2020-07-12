#pragma once
#ifdef TF2BD_ENABLE_DISCORD_INTEGRATION

#include "GameData/TFClassType.h"

#include <string_view>

namespace tf2_bot_detector::Discord
{
	void Update();
	void SetClass(TFClassType classType);
	void SetMap(const std::string_view& mapName);
}

#endif
