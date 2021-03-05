#pragma once

#include <mh/coroutine/generator.hpp>

#include <filesystem>

namespace tf2_bot_detector
{
	class Settings;

	class IAddonManager
	{
	public:
		virtual ~IAddonManager() = default;

		static IAddonManager& Get();

		virtual mh::generator<std::filesystem::path> GetAllAvailableAddons() const = 0;
		virtual mh::generator<std::filesystem::path> GetAllEnabledAddons(const Settings& settings) const = 0;
		virtual bool IsAddonEnabled(const Settings& settings, const std::filesystem::path& addon) const = 0;
		virtual void SetAddonEnabled(Settings& settings, const std::filesystem::path& addon, bool enabled) = 0;
	};
}
