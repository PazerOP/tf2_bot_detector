#include "MainWindow.h"
#include "Config/Settings.h"
#include "Platform/Platform.h"
#include "UI/ImGui_TF2BotDetector.h"
#include "BaseTextures.h"
#include "IPlayer.h"
#include "Networking/SteamAPI.h"
#include "Networking/LogsTFAPI.h"
#include "TextureManager.h"

#include <imgui_desktop/ScopeGuards.h>
#include <imgui_desktop/StorageHelper.h>
#include <mh/math/interpolation.hpp>
#include <mh/text/fmtstr.hpp>
#include <mh/text/formatters/error_code.hpp>

#include <string_view>

using namespace std::chrono_literals;
using namespace std::string_view_literals;
using namespace tf2_bot_detector;

static std::unordered_set<SteamID> FindFriendsInGame(const IPlayer& player)
{
	const std::unordered_set<SteamID>& steamFriends = player.GetSteamFriends();
	std::unordered_set<SteamID> retVal;

	for (const IPlayer& otherPlayer : player.GetWorld().GetPlayers())
	{
		if (steamFriends.contains(otherPlayer.GetSteamID()))
			retVal.insert(otherPlayer);
	}

	return retVal;
}
