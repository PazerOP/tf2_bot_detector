#pragma once

#include "ChatWrappers.h"
#include "Clock.h"
#include "Networking/HTTPClient.h"
#include "SteamID.h"

#include <nlohmann/json_fwd.hpp>

#include <filesystem>
#include <optional>
#include <vector>

namespace srcon
{
	class async_client;
}

namespace tf2_bot_detector
{
	enum class ProgramUpdateCheckMode
	{
		Unknown = -1,

		Releases,
		Previews,
		Disabled,
	};

	void to_json(nlohmann::json& j, const ProgramUpdateCheckMode& d);
	void from_json(const nlohmann::json& j, ProgramUpdateCheckMode& d);

	struct AutoDetectedSettings
	{
		SteamID GetLocalSteamID() const;
		SteamID m_LocalSteamIDOverride;

		std::filesystem::path GetSteamDir() const;
		std::filesystem::path m_SteamDirOverride;

		std::filesystem::path GetTFDir() const;
		std::filesystem::path m_TFDirOverride;
	};

	struct GotoProfileSite
	{
		std::string m_Name;
		std::string m_ProfileURL;

		std::string CreateProfileURL(const SteamID& id) const;
	};

	struct GeneralSettings
	{
		bool m_AutoChatWarnings = true;
		bool m_AutoChatWarningsConnecting = false;
		bool m_AutoVotekick = true;
		float m_AutoVotekickDelay = 15;
		bool m_AutoMark = true;

		bool m_SleepWhenUnfocused = true;
		bool m_AutoTempMute = true;
		bool m_AutoLaunchTF2 = false;

		ProgramUpdateCheckMode m_ProgramUpdateCheckMode = ProgramUpdateCheckMode::Unknown;

		constexpr auto GetAutoVotekickDelay() const { return std::chrono::duration<float>(m_AutoVotekickDelay); }
	};

	class Settings final : public AutoDetectedSettings, public GeneralSettings
	{
	public:
		Settings();

		void LoadFile();
		bool SaveFile() const;

		// Settings that are not saved in the config file because we want them to
		// reset to these defaults when the tool is reopened
		struct Unsaved
		{
			~Unsaved();

			bool m_DebugShowCommands = false;

			uint32_t m_ChatMsgWrappersToken{};
			std::optional<ChatWrappers> m_ChatMsgWrappers;
			std::unique_ptr<srcon::async_client> m_RCONClient;

		} m_Unsaved;

		std::string m_SteamAPIKey;

		std::optional<bool> m_AllowInternetUsage;
		const HTTPClient* GetHTTPClient() const;

		std::vector<GotoProfileSite> m_GotoProfileSites;

		struct Logging
		{
			bool m_RCONPackets = false;
#ifdef TF2BD_ENABLE_DISCORD_INTEGRATION
			bool m_DiscordRichPresence = false;
#endif
		} m_Logging;

		struct Discord
		{
			bool m_EnableRichPresence = true;

		} m_Discord;

		struct Theme
		{
			struct Colors
			{
				std::array<float, 4> m_ScoreboardCheaterBG = { 1, 0, 1, 1 };
				std::array<float, 4> m_ScoreboardSuspiciousBG = { 1, 1, 0, 1 };
				std::array<float, 4> m_ScoreboardExploiterBG = { 0, 1, 1, 1 };
				std::array<float, 4> m_ScoreboardRacistBG = { 1, 1, 1, 1 };
				std::array<float, 4> m_ScoreboardYouFG = { 0, 1, 0, 1 };
				std::array<float, 4> m_ScoreboardConnectingFG = { 1, 1, 0, 0.5f };

				std::array<float, 4> m_ScoreboardFriendlyTeamBG = { 0.19704340398311615f, 0.5180000066757202f, 0.25745877623558044f, 0.5f };
				std::array<float, 4> m_ScoreboardEnemyTeamBG = { 0.8270000219345093f, 0.42039787769317627f, 0.38951700925827026f, 0.5f };

				std::array<float, 4> m_ChatLogYouFG = { 0, 1, 0, 1 };
				std::array<float, 4> m_ChatLogFriendlyTeamFG = { 0.7f, 1, 0.7f, 1 };
				std::array<float, 4> m_ChatLogEnemyTeamFG = { 1, 0.535f, 0.5f, 1 };
			} m_Colors;

		} m_Theme;

	private:
		mutable std::shared_ptr<HTTPClient> m_HTTPClient;
	};
}

template<typename CharT, typename Traits>
std::basic_ostream<CharT, Traits>& operator<<(std::basic_ostream<CharT, Traits>& os, tf2_bot_detector::ProgramUpdateCheckMode val)
{
	using ProgramUpdateCheckMode = tf2_bot_detector::ProgramUpdateCheckMode;

#undef OS_CASE
#define OS_CASE(v) case v : return os << #v
	switch (val)
	{
		OS_CASE(ProgramUpdateCheckMode::Disabled);
		OS_CASE(ProgramUpdateCheckMode::Previews);
		OS_CASE(ProgramUpdateCheckMode::Releases);
		OS_CASE(ProgramUpdateCheckMode::Unknown);

	default:
		return os << "ProgramUpdateCheckMode(" << +std::underlying_type_t<ProgramUpdateCheckMode>(val) << ')';
	}
#undef OS_CASE
}
