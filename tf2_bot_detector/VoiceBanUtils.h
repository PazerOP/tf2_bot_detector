#pragma once

#include "Config/Settings.h"
#include "Log.h"
#include "SteamID.h"

#include <filesystem>
#include <fstream>
#include <vector>
#include <cstring>

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
	private:
		const Settings* m_Settings = nullptr;
		std::filesystem::path m_VoiceDataPath;

		std::vector<VoiceBanPlayerID> m_BanList;
	};
}
