#include <menu/object/container/Table.hpp>
#include <menu/special/PlayerListController.hpp>
#include <menu/object/container/TRow.hpp>
#include <menu/ObjectFactory.hpp>
#include <menu/object/Text.hpp>
#include <settings/Registered.hpp>
#include <menu/object/container/TData.hpp>
#include <menu/Message.hpp>
#include <menu/menu/special/PlayerListController.hpp>
#include "drawing.hpp"

/*
  Created on 26.07.18.
*/

static settings::RVariable<rgba_t> color_team_red{ "zk.style.player-list.team.red", "d10e25" };
static settings::RVariable<rgba_t> color_team_red_dead{ "zk.style.player-list.team.red-dead", "660000" };
static settings::RVariable<rgba_t> color_team_blue{ "zk.style.player-list.team.blue", "0000ff" };
static settings::RVariable<rgba_t> color_team_blue_dead{ "zk.style.player-list.team.blue-dead", "6f6fce" };
static settings::RVariable<rgba_t> color_team_other{ "zk.style.player-list.team.other", "E6E6E6" };
static settings::RVariable<rgba_t> color_team_other_dead{ "zk.style.player-list.team.other-dead", "E6E6E6" };

static const char *class_names[] = { "Unknown", "Scout", "Sniper", "Soldier", "Demoman", "Medic", "Heavy", "Pyro", "Spy", "Engineer" };

zerokernel::special::PlayerListController::PlayerListController(Table &table) : table(table)
{
}

void zerokernel::special::PlayerListController::setKickButtonCallback(zerokernel::special::PlayerListController::kick_callback_type callback)
{
    cb_kick = std::move(callback);
}

void zerokernel::special::PlayerListController::setOpenSteamCallback(zerokernel::special::PlayerListController::open_steam_callback_type callback)
{
    cb_open_steam = std::move(callback);
}

void zerokernel::special::PlayerListController::setChangeStateCallback(zerokernel::special::PlayerListController::change_state_callback_type callback)
{
    cb_change_state = std::move(callback);
}

void zerokernel::special::PlayerListController::handleMessage(zerokernel::Message &msg, bool is_relayed)
{
    if (is_relayed)
        return;

    if (msg.name != "LeftClick")
        return;

    auto action = (std::string) msg.sender->kv["action"];

    if (action == "open_steam")
    {
        auto steamId = (std::string) msg.sender->kv["steam_id"];
        errno        = 0;
        auto id      = std::strtoul(steamId.c_str(), nullptr, 10);
        if (!errno)
        {
            if (cb_open_steam)
                cb_open_steam(id);
        }
    }
    if (action == "kick")
    {
        if (cb_kick)
        {
            cb_kick((int) msg.sender->kv["user_id"]);
        }
    }
    if (action == "change_state")
    {
        auto steamId = (std::string) msg.sender->kv["steam_id"];
        errno        = 0;
        auto id      = std::strtoul(steamId.c_str(), nullptr, 10);
        if (!errno)
        {
            if (cb_change_state)
                cb_change_state(id, (int) msg.sender->kv["user_id"]);
        }
    }
}

void zerokernel::special::PlayerListController::removeAll()
{
    table.removeAllRowsExceptFirst();
}

void zerokernel::special::PlayerListController::removePlayer(int id)
{
    table.iterateObjects([id](BaseMenuObject *a) {
        auto row = dynamic_cast<TRow *>(a);
        // Shouldn't happen
        if (row == nullptr)
            return;
        if ((int) row->kv["player_id"] == id)
            row->markForDelete();
    });
}

void zerokernel::special::PlayerListController::updatePlayerName(int id, const char *name)
{
    table.iterateObjects([id, name](BaseMenuObject *a) {
        auto row = dynamic_cast<TRow *>(a);
        // Shouldn't happen
        if (row == nullptr)
            return;
        if ((int) row->kv["player_id"] == id)
        {
            auto el = dynamic_cast<Text *>(row->getElementById("username"));
            if (el)
                el->set(name);
        }
    });
}

void zerokernel::special::PlayerListController::updatePlayerLifeState(int id, bool dead)
{
    table.iterateObjects([this, id, dead](BaseMenuObject *a) {
        auto row = dynamic_cast<TRow *>(a);
        // Shouldn't happen
        if (row == nullptr)
            return;
        if ((int) (row->kv["player_id"]) == id)
        {
            row->kv["player_dead"] = int(dead);
            updateRow(row);
        }
    });
}

void zerokernel::special::PlayerListController::updatePlayerTeam(int id, int team)
{
    table.iterateObjects([this, id, team](BaseMenuObject *a) {
        auto row = dynamic_cast<TRow *>(a);
        // Shouldn't happen
        if (row == nullptr)
            return;
        if ((int) row->kv["player_id"] == id)
        {
            row->kv["player_team"] = team;
            updateRow(row);
        }
    });
}

void zerokernel::special::PlayerListController::updatePlayerClass(int id, int classId)
{
    table.iterateObjects([this, id, classId](BaseMenuObject *a) {
        auto row = dynamic_cast<TRow *>(a);
        // Shouldn't happen
        if (row == nullptr)
            return;
        if ((int) row->kv["player_id"] == id)
        {
            row->kv["player_class"] = classId;
            updateRow(row);
        }
    });
}

void zerokernel::special::PlayerListController::updatePlayerState(int id, std::string state_string)
{
    table.iterateObjects([this, id, state_string](BaseMenuObject *a) {
        auto row = dynamic_cast<TRow *>(a);
        // Shouldn't happen
        if (row == nullptr)
            return;
        if ((int) row->kv["player_id"] == id)
        {
            auto state = dynamic_cast<Text *>(row->getElementById("state"));
            if (state)
                state->kv["state"] = state_string;
            updateRow(row);
            return;
        }
    });
}

void zerokernel::special::PlayerListController::addPlayer(int id, zerokernel::special::PlayerListData data)
{
    auto row  = ObjectFactory::createFromPrefab("player-list-row");
    auto trow = dynamic_cast<TRow *>(row.get());
    if (trow)
    {
        trow->kv["player_id"]    = id;
        trow->kv["player_team"]  = data.teamId;
        trow->kv["player_class"] = data.classId;
        trow->kv["player_dead"]  = int(data.dead);
        trow->kv["player_steam"] = int(data.steam);
        auto uid                 = dynamic_cast<Text *>(trow->getElementById("uid"));
        auto steam               = dynamic_cast<Text *>(trow->getElementById("steam"));
        auto username            = dynamic_cast<Text *>(trow->getElementById("username"));
        auto kick                = dynamic_cast<Text *>(trow->getElementById("kick"));
        auto state               = dynamic_cast<Text *>(trow->getElementById("state"));
        if (uid)
            uid->set(std::to_string(id));
        if (steam)
        {
            steam->kv["steam_id"] = std::to_string(data.steam);
            steam->kv["action"]   = "open_steam";
            steam->addMessageHandler(*this);
            steam->set(std::to_string(data.steam));
        }
        if (username && username->font)
            username->set(ShrinkString(data.name, username->bb.getParentBox().getContentBox().width, *username->font));
        if (kick)
        {
            kick->kv["user_id"] = id;
            kick->kv["action"]  = "kick";
            kick->addMessageHandler(*this);
        }
        if (state)
        {
            state->kv["state"]    = data.state;
            state->kv["user_id"]  = id;
            state->kv["steam_id"] = std::to_string(data.steam);
            state->kv["action"]   = "change_state";
            state->addMessageHandler(*this);
        }
        updateRow(trow);
        table.addObject(std::move(row));
    }
    else
    {
        printf("WARNING: Could not create a player list row from prefab\n");
        return;
    }
}

void zerokernel::special::PlayerListController::changeRowColor(zerokernel::TRow *row, const rgba_t &color)
{
    row->iterateObjects([&color](BaseMenuObject *object) -> void {
        auto tdata = dynamic_cast<TData *>(object);
        if (tdata)
        {
            tdata->iterateObjects([&color](BaseMenuObject *object) -> void {
                auto text = dynamic_cast<Text *>(object);
                if (text && (int) text->kv["team_color"])
                {
                    printf("Setting color to %f %f %f %f\n", color.r, color.g, color.b, color.a);
                    text->setColorText(&color);
                }
            });
        }
    });
}

void zerokernel::special::PlayerListController::updateRow(zerokernel::TRow *row)
{
    int team    = (int) row->kv["player_team"];
    int dead    = (int) row->kv["player_dead"];
    int classId = (int) row->kv["player_class"];

    if (classId < 0 || classId > 9)
        classId = 0;

    switch (team)
    {
    case 1:
        changeRowColor(row, dead ? *color_team_red_dead : *color_team_red);
        break;
    case 2:
        changeRowColor(row, dead ? *color_team_blue_dead : *color_team_blue);
        break;
    default:
        changeRowColor(row, dead ? *color_team_other_dead : *color_team_other);
    }

    auto el         = dynamic_cast<Text *>(row->getElementById("class"));
    auto alivestate = dynamic_cast<Text *>(row->getElementById("alivestate"));
    auto state      = dynamic_cast<Text *>(row->getElementById("state"));
    if (el)
    {
        el->set(class_names[classId]);
    }
    if (alivestate)
        alivestate->set(dead ? "Dead" : "Alive");
    if (state)
        state->set(static_cast<std::string>(state->kv["state"]));
}
