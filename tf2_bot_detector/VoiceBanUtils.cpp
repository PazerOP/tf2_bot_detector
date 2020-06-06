#include "VoiceBanUtils.h"

using namespace std;
using namespace tf2_bot_detector;

VoiceBanUtils::VoiceBanUtils(const Settings& settings)
    : m_Settings(&settings)
    , m_BanList()
	, m_PlayerList(settings)
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

    m_BanList = std::list<VoiceBanPlayerID>();

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
    char searchString[SIGNED_GUID_LEN] = {0};
    strcpy_s(searchString, SIGNED_GUID_LEN, id.str().c_str());

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

    memset(playerId.guid, 0, SIGNED_GUID_LEN);
    strcpy_s(playerId.guid, SIGNED_GUID_LEN, id.str().c_str());

    m_BanList.push_back(playerId);

    return true;
}

void VoiceBanUtils::BanCheaters()
{
    m_PlayerList.LoadFiles();

    Log(std::string("Muting cheaters from saved list ..."));

    #define BAN_FROM_LIST(LIST)                                                 \
    for (const auto& [steamID, player]: LIST)                                   \
    {                                                                           \
        if (player.m_Attributes.HasAttribute(PlayerAttributes::Cheater)) {      \
            if (MarkBan(steamID)) {                                             \
                Log(std::string("Muted ") << steamID);                          \
            } else {                                                            \
                LogWarning(std::string("Already muted ") << steamID);           \
            }                                                                   \
        }                                                                       \
    }                                                                           

    BAN_FROM_LIST(m_PlayerList.GetOfficialPlayerList())
    BAN_FROM_LIST(m_PlayerList.GetOtherPlayerLists())
}
