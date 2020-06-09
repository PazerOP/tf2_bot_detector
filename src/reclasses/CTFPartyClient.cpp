/*
 * CTFPartyClient.cpp
 *
 *  Created on: Dec 7, 2017
 *      Author: nullifiedcat
 */

#include "common.hpp"
#include "core/e8call.hpp"

re::CTFPartyClient *re::CTFPartyClient::GTFPartyClient()
{
    typedef re::CTFPartyClient *(*GTFPartyClient_t)(void);
    static uintptr_t addr                     = gSignatures.GetClientSignature("55 A1 ? ? ? ? 89 E5 5D C3 8D B6 00 00 00 00 A1 ? ? ? ? 85 C0");
    static GTFPartyClient_t GTFPartyClient_fn = GTFPartyClient_t(addr);

    return GTFPartyClient_fn();
}

bool re::CTFPartyClient::BInQueue(re::CTFPartyClient *this_)
{
    return *(uint8_t *) ((uint8_t *) this_ + 69);
}
int re::CTFPartyClient::GetNumOnlineMembers()
{
    typedef int (*GetNumOnlineMembers_t)(re::CTFPartyClient *);
    static uintptr_t addr                               = gSignatures.GetClientSignature("55 89 E5 57 56 53 83 EC ? 8B 7D ? 8B 77 ? 85 F6 0F 84");
    static GetNumOnlineMembers_t GetNumOnlineMembers_fn = GetNumOnlineMembers_t(addr);

    return GetNumOnlineMembers_fn(this);
}
int re::CTFPartyClient::GetNumMembers()
{
    typedef int (*GetNumMembers_t)(re::CTFPartyClient *);
    static uintptr_t addr                   = gSignatures.GetClientSignature("55 89 E5 8B 45 ? 8B 50 ? C6 80") + 0x30;
    static GetNumMembers_t GetNumMembers_fn = GetNumMembers_t(addr);

    return GetNumMembers_fn(this);
}
void re::CTFPartyClient::SendPartyChat(const char *message)
{
    typedef void (*SendPartyChat_t)(re::CTFPartyClient *, const char *);
    static uintptr_t addr                   = gSignatures.GetClientSignature("55 89 E5 57 56 53 81 EC ? ? ? ? 8B 5D 0C 89 1C 24");
    static SendPartyChat_t SendPartyChat_fn = SendPartyChat_t(addr);

    SendPartyChat_fn(this, message);
}

bool re::CTFPartyClient::BCanQueueForStandby(re::CTFPartyClient *this_)
{
    typedef bool (*BCanQueueForStandby_t)(re::CTFPartyClient *);
    static uintptr_t addr                               = gSignatures.GetClientSignature("55 89 E5 53 83 EC 24 8B 5D 08 80 7B 46 00 75 40 8B 4B 38 85 C9 74 39 "
                                                           "E8 ? ? ? ? 89 04 24 E8 ? ? ? ? 84 C0 75 28");
    static BCanQueueForStandby_t BCanQueueForStandby_fn = BCanQueueForStandby_t(addr);

    return BCanQueueForStandby_fn(this_);
}

re::ITFGroupMatchCriteria *re::CTFPartyClient::MutLocalGroupCriteria(re::CTFPartyClient *client)
{
    typedef re::ITFGroupMatchCriteria *(*MutLocalGroupCriteria_t)(re::CTFPartyClient *);
    static uintptr_t addr                                   = gSignatures.GetClientSignature("55 89 E5 8B 45 ? 8B 50 ? C6 80");
    static MutLocalGroupCriteria_t MutLocalGroupCriteria_fn = MutLocalGroupCriteria_t(addr);

    return MutLocalGroupCriteria_fn(client);
}

int re::CTFPartyClient::LoadSavedCasualCriteria()
{
    typedef int (*LoadSavedCasualCriteria_t)(re::CTFPartyClient *);
    static uintptr_t addr = gSignatures.GetClientSignature("55 89 E5 8B 45 ? 5D 8B 80 ? ? ? ? C3 66 90 55 89 E5 "
                                                           "8B 45 ? 5D 0F B6 80 ? ? ? ? C3 90 55 89 E5 56") -
                            0x40;
    static LoadSavedCasualCriteria_t LoadSavedCasualCriteria_fn = LoadSavedCasualCriteria_t(addr);

    return LoadSavedCasualCriteria_fn(this);
}
void re::CTFPartyClient::RequestQueueForStandby()
{
    typedef void (*RequestStandby_t)(re::CTFPartyClient *);
    static uintptr_t addr                     = gSignatures.GetClientSignature("55 89 E5 57 56 53 83 EC ? 8B 7D ? 8B 4F ? 85 C9 74");
    static RequestStandby_t RequestStandby_fn = RequestStandby_t(addr);
    RequestStandby_fn(this);
    return;
}
char re::CTFPartyClient::RequestQueueForMatch(int type)
{
    typedef char (*RequestQueueForMatch_t)(re::CTFPartyClient *, int);
    static uintptr_t addr                                 = gSignatures.GetClientSignature("55 89 E5 57 56 53 81 EC ? ? ? ? 8B 45 ? E8");
    static RequestQueueForMatch_t RequestQueueForMatch_fn = RequestQueueForMatch_t(addr);

    return RequestQueueForMatch_fn(this, type);
}
bool re::CTFPartyClient::BInQueueForMatchGroup(int type)
{
    typedef bool (*BInQueueForMatchGroup_t)(re::CTFPartyClient *, int);
    static uintptr_t addr                                   = gSignatures.GetClientSignature("55 89 E5 56 53 8B 5D ? 8B 75 ? 89 D8 E8 ? ? ? ? 84 C0 74 ? 8B 4E");
    static BInQueueForMatchGroup_t BInQueueForMatchGroup_fn = BInQueueForMatchGroup_t(addr);

    return BInQueueForMatchGroup_fn(this, type);
}
bool re::CTFPartyClient::BInQueueForStandby()
{
    return *((unsigned char *) this + 84);
}
char re::CTFPartyClient::RequestLeaveForMatch(int type)
{
    typedef char (*RequestLeaveForMatch_t)(re::CTFPartyClient *, int);
    static uintptr_t addr                                 = e8call_direct(gSignatures.GetClientSignature("E8 ? ? ? ? 90 8B 45 E4"));
    static RequestLeaveForMatch_t RequestLeaveForMatch_fn = RequestLeaveForMatch_t(addr);

    return RequestLeaveForMatch_fn(this, type);
}
int re::CTFPartyClient::BInvitePlayerToParty(CSteamID steamid)
{
    typedef int (*BInvitePlayerToParty_t)(re::CTFPartyClient *, CSteamID, bool);
    static uintptr_t addr                                 = gSignatures.GetClientSignature("55 89 E5 57 56 53 81 EC ? ? ? ? 8B 45 ? 8B 5D ? 8B 55 ? 89 85"
                                                           "65 A1 ? ? ? ? 89 45 ? 31 C0 8B 45");
    static BInvitePlayerToParty_t BInvitePlayerToParty_fn = BInvitePlayerToParty_t(addr);
    return BInvitePlayerToParty_fn(this, steamid, false);
}
int re::CTFPartyClient::BRequestJoinPlayer(CSteamID steamid)
{
    typedef int (*BRequestJoinPlayer_t)(re::CTFPartyClient *, CSteamID, bool);
    static uintptr_t addr                             = gSignatures.GetClientSignature("55 89 E5 57 56 53 81 EC ? ? ? ? 8B 45 14 8B 55 ? 89 45 ? 8B");
    static BRequestJoinPlayer_t BRequestJoinPlayer_fn = BRequestJoinPlayer_t(addr);
    return BRequestJoinPlayer_fn(this, steamid, false);
}
int re::CTFPartyClient::PromotePlayerToLeader(CSteamID steamid)
{
    typedef int (*PromotePlayerToLeader_t)(re::CTFPartyClient *, CSteamID);
    static uintptr_t addr                                   = gSignatures.GetClientSignature("8B 55 08 8B 8A A0 01 00 00 8B 92 9C 01 00 00 89 04 24 89 4C 24 08 89 54 24 04 E8") + 27;
    static PromotePlayerToLeader_t PromotePlayerToLeader_fn = PromotePlayerToLeader_t(e8call(reinterpret_cast<void *>(addr)));

    return PromotePlayerToLeader_fn(this, steamid);
}
std::vector<unsigned> re::CTFPartyClient::GetPartySteamIDs()
{
    typedef bool (*SteamIDOfSlot_t)(int slot, CSteamID *our);
    static uintptr_t addr                   = gSignatures.GetClientSignature("55 89 E5 56 53 31 DB 83 EC ? 8B 75 ? E8");
    static SteamIDOfSlot_t SteamIDOfSlot_fn = SteamIDOfSlot_t(addr);
    std::vector<unsigned> party_members;
    for (int i = 0; i < GetNumMembers(); i++)
    {
        CSteamID out;
        SteamIDOfSlot_fn(i, &out);
        if (out.GetAccountID())
            party_members.push_back(out.GetAccountID());
    }
    return party_members;
}

int re::CTFPartyClient::KickPlayer(CSteamID steamid)
{
    typedef int (*KickPlayer_t)(re::CTFPartyClient *, CSteamID);
    static uintptr_t addr             = gSignatures.GetClientSignature("8B 93 9C 01 00 00 8B 8B A0 01 00 00 89 04 24 89 54 24 04 89 4C 24 08 E8") + 24;
    static KickPlayer_t KickPlayer_fn = KickPlayer_t(e8call(reinterpret_cast<void *>(addr)));

    return KickPlayer_fn(this, steamid);
}
bool re::CTFPartyClient::GetCurrentPartyLeader(CSteamID &id)
{
    uintptr_t party = *reinterpret_cast<uintptr_t *>(reinterpret_cast<uintptr_t>(this) + 0x30);
    if (!party)
        return false;

    id = *reinterpret_cast<CSteamID *>(party + 0x1C);
    return true;
}
re::ITFMatchGroupDescription *re::GetMatchGroupDescription(int &idx)
{
    typedef re::ITFMatchGroupDescription *(*GetMatchGroupDescription_t)(int &);
    static uintptr_t addr                                         = gSignatures.GetClientSignature("55 89 E5 8B 45 ? 8B 00 83 F8 ? 77");
    static GetMatchGroupDescription_t GetMatchGroupDescription_fn = GetMatchGroupDescription_t(addr);

    return GetMatchGroupDescription_fn(idx);
}
