/*
  Created on 26.07.18.
*/

#pragma once

#include <menu/object/container/Table.hpp>
#include <functional>
#include <menu/object/container/TRow.hpp>

namespace zerokernel::special
{

class PlayerListData
{
public:
    std::string name;
    unsigned steam;
    // TF2-specific
    int classId;
    int teamId;
    bool dead;
    std::string state;
};

class PlayerListController : public IMessageHandler
{
public:
    typedef std::function<void(int)> kick_callback_type;
    typedef std::function<void(unsigned)> open_steam_callback_type;
    typedef std::function<void(unsigned /*steamid*/, int /*userid*/)> change_state_callback_type;

    explicit PlayerListController(Table &table);

    void addPlayer(int id, PlayerListData data);

    /*
     * If player changes name
     * If player changes team or class
     * If CO data changes
     */
    void updatePlayerName(int id, const char *name);

    void updatePlayerTeam(int id, int team);

    void updatePlayerClass(int id, int classId);

    void updatePlayerLifeState(int id, bool dead);

    void updatePlayerState(int id, std::string state);

    void removePlayer(int id);

    /* On leaving the server */
    void removeAll();

    void setKickButtonCallback(kick_callback_type callback);

    void setOpenSteamCallback(open_steam_callback_type callback);

    void setChangeStateCallback(change_state_callback_type callback);

    void updateRow(TRow *row);

    void changeRowColor(TRow *row, const rgba_t &color);

    void handleMessage(Message &msg, bool is_relayed) override;

    Table &table;

    kick_callback_type cb_kick;
    open_steam_callback_type cb_open_steam;
    change_state_callback_type cb_change_state;
};
} // namespace zerokernel::special
