#include "VoiceBanUtils.h"

using namespace std;
using namespace tf2_bot_detector;

VoiceBanUtils::VoiceBanUtils(const Settings& settings)
    : m_Settings(&settings)
    , m_BanList()
{
    m_VoiceDataPath = m_Settings->m_TFDir / "voice_ban.dt";
}

bool VoiceBanUtils::ReadVoiceBans()
{
    ifstream voiceDataFile(m_VoiceDataPath, ios::binary);
    if (!voiceDataFile.good()) {
        LogWarning(std::string(__FUNCTION__ ": Failed to open voice_ban.dt"));
        return false;
    }

    int version;
    voiceDataFile.read((char*)&version, sizeof(version));

    if (version != BANMGR_FILEVERSION) {
        LogWarning(std::string(__FUNCTION__ ": Version mismatch"));
        return false;
    }

    int contentSize = std::filesystem::file_size(m_VoiceDataPath) - sizeof(version);
    int idsToRead = contentSize / SIGNED_GUID_LEN;

    m_BanList = std::vector<VoiceBanPlayerID>();

    while (idsToRead > 0) {
        VoiceBanPlayerID playerId;
        voiceDataFile.read(playerId.guid, SIGNED_GUID_LEN);
		//Log(std::string(playerId.guid));
        m_BanList.push_back(playerId);
        idsToRead--; 
    }

    voiceDataFile.close();

    return true;
}

bool VoiceBanUtils::WriteVoiceBans()
{
    ofstream voiceDataFile(m_VoiceDataPath, ios::binary);
    if (!voiceDataFile.good()) {
        LogWarning(std::string(__FUNCTION__ ": Failed to open voice_ban.dt"));    
        return false;
    }

    int version = BANMGR_FILEVERSION;
    voiceDataFile.write((char*)&version, sizeof(version));

    for (VoiceBanPlayerID playerId : m_BanList) {
        voiceDataFile.write(playerId.guid, SIGNED_GUID_LEN);
    }

    voiceDataFile.close();

    return true;
}

bool VoiceBanUtils::IsBanned(const tf2_bot_detector::SteamID& id)
{
    char searchString[32] = {0};
    strcpy_s(searchString, id.str().c_str());

    for (VoiceBanPlayerID playerId : m_BanList) {
        if (strcmp(searchString, playerId.guid) == 0)
            return true;
    }

    return false;     
}

bool VoiceBanUtils::MarkBan(const tf2_bot_detector::SteamID& id)
{
    if (IsBanned(id)) return false;

    VoiceBanPlayerID playerId;
    const char* guid = id.str().c_str();

    memset(playerId.guid, 0, SIGNED_GUID_LEN);
    strcpy_s(playerId.guid, guid);

    m_BanList.push_back(playerId);

    return true;
}

