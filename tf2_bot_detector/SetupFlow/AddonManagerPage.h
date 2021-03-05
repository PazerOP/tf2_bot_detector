#pragma once

#include <mh/coroutine/generator.hpp>

#include <filesystem>

namespace tf2_bot_detector
{
	class Settings;

	namespace Addons
	{
		mh::generator<std::filesystem::path> GetAllAvailableAddons();
		mh::generator<std::filesystem::path> GetAllEnabledAddons(const Settings& settings);
	}
}
