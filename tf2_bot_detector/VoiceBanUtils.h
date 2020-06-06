#pragma once

#include "Config/Settings.h"
#include "Config/PlayerListJSON.h"
#include "Log.h"
#include "SteamID.h"

#include <filesystem>
#include <fstream>
#include <list>
#include <cstring>

#include <mh/text/case_insensitive_string.hpp>
#include <mh/text/string_insertion.hpp>

#define BANMGR_FILEVERSION 1
#define SIGNED_GUID_LEN 32

namespace tf2_bot_detector
{
	typedef struct {
		char guid[SIGNED_GUID_LEN];
	} VoiceBanPlayerID;

	class VoiceBanUtils
	{
	public:
		VoiceBanUtils(const Settings& settings);
		bool ReadVoiceBans();
		bool WriteVoiceBans();
		bool IsBanned(const tf2_bot_detector::SteamID& id);
		bool MarkBan(const tf2_bot_detector::SteamID& id);

                void BanCheaters();
	private:
		const Settings* m_Settings = nullptr;
                PlayerListJSON m_PlayerList;
		std::filesystem::path m_VoiceDataPath;

		std::list<VoiceBanPlayerID> m_BanList;
	};
}
