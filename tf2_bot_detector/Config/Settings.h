#pragma once

#include "ChatWrappers.h"
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

		std::string GetLocalIP() const;
		std::string m_LocalIPOverride;
	};

	struct GotoProfileSite
	{
		std::string m_Name;
		std::string m_ProfileURL;

		std::string CreateProfileURL(const SteamID& id) const;
	};

	class Settings final : public AutoDetectedSettings
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

		bool m_AutoChatWarnings = true;
		bool m_AutoChatWarningsConnecting = false;
		bool m_AutoVotekick = true;
		bool m_AutoMark = true;

		bool m_SleepWhenUnfocused = true;
		bool m_AutoTempMute = true;
		bool m_AutoLaunchTF2 = false;

		std::string m_SteamAPIKey;

		std::optional<bool> m_AllowInternetUsage;
		const HTTPClient* GetHTTPClient() const;
		ProgramUpdateCheckMode m_ProgramUpdateCheckMode = ProgramUpdateCheckMode::Unknown;
		std::string m_LocalIPOverride;

		std::vector<GotoProfileSite> m_GotoProfileSites;

		struct Theme
		{
			struct Colors
			{
				float m_ScoreboardCheater[4] = { 1, 0, 1, 1 };
				float m_ScoreboardSuspicious[4] = { 1, 1, 0, 1 };
				float m_ScoreboardExploiter[4] = { 0, 1, 1, 1 };
				float m_ScoreboardRacist[4] = { 1, 1, 1, 1 };
				float m_ScoreboardYou[4] = { 0, 1, 0, 1 };
				float m_ScoreboardConnecting[4] = { 1, 1, 0, 0.5f };

				float m_FriendlyTeam[4] = { 0.19704340398311615f, 0.5180000066757202f, 0.25745877623558044f, 0.5f };
				float m_EnemyTeam[4] = { 0.8270000219345093f, 0.42039787769317627f, 0.38951700925827026f, 0.5f };
			} m_Colors;

		} m_Theme;

	private:
		mutable std::optional<HTTPClient> m_HTTPClient;
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
