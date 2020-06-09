/*
 * UberSpam.cpp
 *
 *  Created on: May 3, 2017
 *      Author: nullifiedcat
 */

#include "common.hpp"
#include <hacks/UberSpam.hpp>
#include <settings/Int.hpp>

namespace hacks::tf::uberspam
{
static settings::Int source{ "uberspam.source", "0" };
static settings::Boolean team_chat{ "uberspam.team-chat", "true" };
static settings::String custom_file{ "uberspam.file", "uberspam.txt" };

static settings::Boolean on_ready{ "uberspam.triggers.ready", "true" };
static settings::Boolean on_used{ "uberspam.triggers.used", "true" };
static settings::Boolean on_ended{ "uberspam.triggers.ended", "true" };
static settings::Int on_build{ "uberspam.triggers.every-n-percent", "25" };

TextFile custom_lines;

static CatCommand custom_file_reload("uberspam_file_reload", "Reload Ubercharge Spam File", []() { custom_lines.Load(*custom_file); });

const std::vector<std::string> *GetSource()
{
    switch ((int) source)
    {
    case 1:
        return &builtin_cathook;
    case 2:
        return &builtin_nonecore;
    case 3:
        return &custom_lines.lines;
    }
    return nullptr;
}

int ChargePercentLineIndex(float chargef)
{
    if ((int) on_build <= 0)
        return 0;
    int charge = chargef * 100.0f;
    if (charge == 100)
        return 0;
    auto src = GetSource();
    if (!src)
        return 0;
    int lines = src->size() - 3;
    if (lines <= 0)
        return 0;
    int cpl = 100 / lines;
    return 3 + (charge / cpl);
}

static void CreateMove()
{
    if (CE_BAD(LOCAL_E) || !LOCAL_E->m_bAlivePlayer() || CE_BAD(LOCAL_W))
        return;
    if (!GetSource())
        return;
    if (LOCAL_W->m_iClassID() != CL_CLASS(CWeaponMedigun))
        return;
    if (GetSource()->size() < 4)
        return;
    float charge                   = CE_FLOAT(LOCAL_W, netvar.m_flChargeLevel);
    static bool release_last_frame = false;
    static int last_charge         = 0;
    bool release                   = CE_BYTE(LOCAL_W, netvar.bChargeRelease);
    if (release_last_frame != release)
    {
        if (release)
        {
            if (on_used)
                chat_stack::Say(GetSource()->at(1), !!team_chat);
        }
        else
        {
            if (on_ended)
                chat_stack::Say(GetSource()->at(2), !!team_chat);
        }
    }
    if (!release && ((int) (100.0f * charge) != last_charge))
    {
        if (charge == 1.0f)
        {
            if (on_ready)
                chat_stack::Say(GetSource()->at(0), !!team_chat);
        }
        else
        {
            if ((int) (charge * 100.0f) != 0 && on_build)
            {
                int chargeperline = ((int) on_build >= 100) ? (100 / (GetSource()->size() - 2)) : (int) on_build;
                if (chargeperline < 1)
                    chargeperline = 1;
                if ((int) (charge * 100.0f) % chargeperline == 0)
                {
                    std::string res = GetSource()->at(ChargePercentLineIndex(charge));
                    ReplaceString(res, "%i%", std::to_string((int) (charge * 100.0f)));
                    chat_stack::Say(res, !!team_chat);
                }
            }
        }
    }
    release_last_frame = release;
    last_charge        = (int) (100.0f * charge);
}

static InitRoutine register_EC([]() { EC::Register(EC::CreateMove, CreateMove, "Uberspam", EC::average); });
// Ready, Used, Ended, %...

const std::vector<std::string> builtin_cathook  = { "-> I am charged!", "-> Not a step back! UBERCHARGE USED!", "-> My Ubercharge comes to an end!", "-> I have %i%% of ubercharge!", "-> I have half of the ubercharge!", "-> Ubercharge almost ready! (%i%%)" };
const std::vector<std::string> builtin_nonecore = { ">>> GET READY TO RUMBLE! <<<", ">>> CHEATS ACTIVATED! <<<", ">>> RUMBLE COMPLETE! <<<", ">>> RUMBLE IS %i%% CHARGED! <<<" };
} // namespace hacks::tf::uberspam
