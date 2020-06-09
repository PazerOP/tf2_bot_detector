/*
 * CreateMove.cpp
 *
 *  Created on: Jan 8, 2017
 *      Author: nullifiedcat
 */

#include "common.hpp"
#include "hack.hpp"
#include "MiscTemporary.hpp"
#include "SeedPrediction.hpp"
#include <link.h>
#include <hacks/hacklist.hpp>
#include <settings/Bool.hpp>
#include <hacks/AntiAntiAim.hpp>
#include "NavBot.hpp"
#include "HookTools.hpp"
#include "teamroundtimer.hpp"

#include "HookedMethods.hpp"
#include "PreDataUpdate.hpp"

static settings::Boolean minigun_jump{ "misc.minigun-jump-tf2c", "false" };
static settings::Boolean roll_speedhack{ "misc.roll-speedhack", "false" };
static settings::Boolean forward_speedhack{ "misc.roll-speedhack.forward", "false" };
static settings::Boolean engine_pred{ "misc.engine-prediction", "true" };
static settings::Boolean debug_projectiles{ "debug.projectiles", "false" };
static settings::Int fullauto{ "misc.full-auto", "0" };
static settings::Int fakelag_amount{ "misc.fakelag", "0" };
static settings::Boolean fuckmode{ "misc.fuckmode", "false" };

class CMoveData;
namespace engine_prediction
{

void RunEnginePrediction(IClientEntity *ent, CUserCmd *ucmd)
{
    if (!ent)
        return;

    typedef void (*SetupMoveFn)(IPrediction *, IClientEntity *, CUserCmd *, class IMoveHelper *, CMoveData *);
    typedef void (*FinishMoveFn)(IPrediction *, IClientEntity *, CUserCmd *, CMoveData *);

    void **predictionVtable  = *((void ***) g_IPrediction);
    SetupMoveFn oSetupMove   = (SetupMoveFn)(*(unsigned *) (predictionVtable + 19));
    FinishMoveFn oFinishMove = (FinishMoveFn)(*(unsigned *) (predictionVtable + 20));
    // CMoveData *pMoveData = (CMoveData*)(sharedobj::client->lmap->l_addr +
    // 0x1F69C0C);  CMoveData movedata {};
    auto object          = std::make_unique<char[]>(165);
    CMoveData *pMoveData = (CMoveData *) object.get();

    // Backup
    float frameTime = g_GlobalVars->frametime;
    float curTime   = g_GlobalVars->curtime;

    CUserCmd defaultCmd{};
    if (ucmd == nullptr)
    {
        ucmd = &defaultCmd;
    }

    // Set Usercmd for prediction
    NET_VAR(ent, 4188, CUserCmd *) = ucmd;

    // Set correct CURTIME
    g_GlobalVars->curtime   = g_GlobalVars->interval_per_tick * NET_INT(ent, netvar.nTickBase);
    g_GlobalVars->frametime = g_GlobalVars->interval_per_tick;

    *g_PredictionRandomSeed = MD5_PseudoRandom(current_user_cmd->command_number) & 0x7FFFFFFF;

    // Run The Prediction
    g_IGameMovement->StartTrackPredictionErrors(reinterpret_cast<CBasePlayer *>(ent));
    oSetupMove(g_IPrediction, ent, ucmd, NULL, pMoveData);
    g_IGameMovement->ProcessMovement(reinterpret_cast<CBasePlayer *>(ent), pMoveData);
    oFinishMove(g_IPrediction, ent, ucmd, pMoveData);
    g_IGameMovement->FinishTrackPredictionErrors(reinterpret_cast<CBasePlayer *>(ent));

    // Reset User CMD
    NET_VAR(ent, 4188, CUserCmd *) = nullptr;

    g_GlobalVars->frametime = frameTime;
    g_GlobalVars->curtime   = curTime;

    // Adjust tickbase
    NET_INT(ent, netvar.nTickBase)++;
    return;
}
} // namespace engine_prediction

void PrecalculateCanShoot()
{
    auto weapon = g_pLocalPlayer->weapon();
    // Check if player and weapon are good
    if (CE_BAD(g_pLocalPlayer->entity) || CE_BAD(weapon))
    {
        calculated_can_shoot = false;
        return;
    }

    // flNextPrimaryAttack without reload
    static float next_attack = 0.0f;
    // Last shot fired using weapon
    static float last_attack = 0.0f;
    // Last weapon used
    static CachedEntity *last_weapon = nullptr;
    float server_time                = (float) (CE_INT(g_pLocalPlayer->entity, netvar.nTickBase)) * g_GlobalVars->interval_per_tick;
    float new_next_attack            = CE_FLOAT(weapon, netvar.flNextPrimaryAttack);
    float new_last_attack            = CE_FLOAT(weapon, netvar.flLastFireTime);

    // Reset everything if using a new weapon/shot fired
    if (new_last_attack != last_attack || last_weapon != weapon)
    {
        next_attack = new_next_attack;
        last_attack = new_last_attack;
        last_weapon = weapon;
    }
    // Check if can shoot
    calculated_can_shoot = next_attack <= server_time;
}

static int attackticks = 0;
namespace hooked_methods
{
DEFINE_HOOKED_METHOD(CreateMove, bool, void *this_, float input_sample_time, CUserCmd *cmd)
{
    g_Settings.is_create_move = true;
    bool time_replaced, ret, speedapplied;
    float curtime_old, servertime, speed, yaw;
    Vector vsilent, ang;

    current_user_cmd = cmd;
    IF_GAME(IsTF2C())
    {
        if (CE_GOOD(LOCAL_W) && minigun_jump && LOCAL_W->m_iClassID() == CL_CLASS(CTFMinigun))
            CE_INT(LOCAL_W, netvar.iWeaponState) = 0;
    }
    ret = original::CreateMove(this_, input_sample_time, cmd);

    if (!cmd)
    {
        g_Settings.is_create_move = false;
        return ret;
    }

    // Disabled because this causes EXTREME aimbot inaccuracy
    // Actually dont disable it. It causes even more inaccuracy
    if (!cmd->command_number)
    {
        g_Settings.is_create_move = false;
        return ret;
    }

    tickcount++;

    if (!isHackActive())
    {
        g_Settings.is_create_move = false;
        return ret;
    }

    if (!g_IEngine->IsInGame())
    {
        g_Settings.bInvalid       = true;
        g_Settings.is_create_move = false;
        return true;
    }

    PROF_SECTION(CreateMove);
#if ENABLE_VISUALS
    stored_buttons = current_user_cmd->buttons;
    if (freecam_is_toggled)
    {
        current_user_cmd->sidemove    = 0.0f;
        current_user_cmd->forwardmove = 0.0f;
    }
#endif
    if (current_user_cmd && current_user_cmd->command_number)
        last_cmd_number = current_user_cmd->command_number;

    /**bSendPackets = true;
    if (hacks::shared::lagexploit::ExploitActive()) {
        *bSendPackets = ((current_user_cmd->command_number % 4) == 0);
        //logging::Info("%d", *bSendPackets);
    }*/

    // logging::Info("canpacket: %i", ch->CanPacket());
    // if (!cmd) return ret;

    time_replaced = false;
    curtime_old   = g_GlobalVars->curtime;

    INetChannel *ch;
    ch = (INetChannel *) g_IEngine->GetNetChannelInfo();
    if (ch && !hooks::netchannel.IsHooked((void *) ch))
    {
        hooks::netchannel.Set(ch);
        hooks::netchannel.HookMethod(HOOK_ARGS(SendDatagram));
        hooks::netchannel.HookMethod(HOOK_ARGS(CanPacket));
        hooks::netchannel.HookMethod(HOOK_ARGS(SendNetMsg));
        hooks::netchannel.HookMethod(HOOK_ARGS(Shutdown));
        hooks::netchannel.Apply();
#if ENABLE_IPC
        ipc::UpdateServerAddress();
#endif
    }
    if (*fuckmode)
    {
        static int prevbuttons = 0;
        current_user_cmd->buttons |= prevbuttons;
        prevbuttons |= current_user_cmd->buttons;
    }
    hooked_methods::CreateMove();

    if (!g_Settings.bInvalid && CE_GOOD(g_pLocalPlayer->entity))
    {
        servertime            = (float) CE_INT(g_pLocalPlayer->entity, netvar.nTickBase) * g_GlobalVars->interval_per_tick;
        g_GlobalVars->curtime = servertime;
        time_replaced         = true;
    }
    if (g_Settings.bInvalid)
    {
        entity_cache::Invalidate();
    }

    //	PROF_BEGIN();
    {
        PROF_SECTION(EntityCache);
        entity_cache::Update();
    }
    //	PROF_END("Entity Cache updating");
    {
        PROF_SECTION(CM_PlayerResource);
        g_pPlayerResource->Update();
    }
    {
        PROF_SECTION(CM_LocalPlayer);
        g_pLocalPlayer->Update();
    }
    PrecalculateCanShoot();
    if (firstcm)
    {
        DelayTimer.update();
        //        hacks::tf2::NavBot::Init();
        //        hacks::tf2::NavBot::initonce();
        nav::status = nav::off;
        hacks::tf2::NavBot::init(true);
        if (identify)
            sendIdentifyMessage(false);
        EC::run(EC::FirstCM);
        firstcm = false;
    }
    g_Settings.bInvalid = false;

    if (CE_GOOD(g_pLocalPlayer->entity))
    {
        if (!g_pLocalPlayer->life_state && CE_GOOD(g_pLocalPlayer->weapon()))
        {
            // Walkbot can leave game.
            if (!g_IEngine->IsInGame())
            {
                g_Settings.is_create_move = false;
                return ret;
            }
            if (current_user_cmd->buttons & IN_ATTACK)
                ++attackticks;
            else
                attackticks = 0;
            if (fullauto)
                if (current_user_cmd->buttons & IN_ATTACK)
                    if (attackticks % *fullauto + 1 < *fullauto)
                        current_user_cmd->buttons &= ~IN_ATTACK;
            static int fakelag_queue      = 0;
            g_pLocalPlayer->isFakeAngleCM = false;
            if (CE_GOOD(LOCAL_E))
                if (fakelag_amount || (hacks::shared::antiaim::force_fakelag && hacks::shared::antiaim::isEnabled()))
                {
                    int fakelag_amnt = (*fakelag_amount > 1) ? *fakelag_amount : 1;
                    *bSendPackets    = fakelag_amnt == fakelag_queue;
                    if (*bSendPackets)
                        g_pLocalPlayer->isFakeAngleCM = true;
                    fakelag_queue++;
                    if (fakelag_queue > fakelag_amnt)
                        fakelag_queue = 0;
                }
            {
                PROF_SECTION(CM_antiaim);
                hacks::shared::antiaim::ProcessUserCmd(cmd);
            }
            if (debug_projectiles)
                projectile_logging::Update();
        }
    }
    {
        PROF_SECTION(CM_WRAPPER);
        EC::run(EC::CreateMove_NoEnginePred);
        if (engine_pred)
            engine_prediction::RunEnginePrediction(RAW_ENT(LOCAL_E), current_user_cmd);

        EC::run(EC::CreateMove);
    }
    if (time_replaced)
        g_GlobalVars->curtime = curtime_old;
    g_Settings.bInvalid = false;
    {
        PROF_SECTION(CM_chat_stack);
        chat_stack::OnCreateMove();
    }

    // TODO Auto Steam Friend

#if ENABLE_IPC
    {
        PROF_SECTION(CM_playerlist);
        static Timer ipc_update_timer{};
        //	playerlist::DoNotKillMe();
        if (ipc_update_timer.test_and_set(1000 * 10))
        {
            ipc::UpdatePlayerlist();
        }
    }
#endif
    if (CE_GOOD(g_pLocalPlayer->entity))
    {
        speedapplied = false;
        if (roll_speedhack && cmd->buttons & IN_DUCK && (CE_INT(g_pLocalPlayer->entity, netvar.iFlags) & FL_ONGROUND) && !(cmd->buttons & IN_ATTACK) && !HasCondition<TFCond_Charging>(LOCAL_E))
        {
            speed                     = Vector{ cmd->forwardmove, cmd->sidemove, 0.0f }.Length();
            static float prevspeedang = 0.0f;
            if (fabs(speed) > 0.0f)
            {

                if (forward_speedhack)
                {
                    cmd->forwardmove *= -1.0f;
                    cmd->sidemove *= -1.0f;
                    cmd->viewangles.x = 91;
                }
                Vector vecMove(cmd->forwardmove, cmd->sidemove, 0.0f);

                vecMove *= -1;
                float flLength = vecMove.Length();
                Vector angMoveReverse{};
                VectorAngles(vecMove, angMoveReverse);
                cmd->forwardmove = -flLength;
                cmd->sidemove    = 0.0f; // Move only backwards, no sidemove
                float res        = g_pLocalPlayer->v_OrigViewangles.y - angMoveReverse.y;
                while (res > 180)
                    res -= 360;
                while (res < -180)
                    res += 360;
                if (res - prevspeedang > 90.0f)
                    res = (res + prevspeedang) / 2;
                prevspeedang                     = res;
                cmd->viewangles.y                = res;
                cmd->viewangles.z                = 90.0f;
                g_pLocalPlayer->bUseSilentAngles = true;
                speedapplied                     = true;
            }
        }
        if (g_pLocalPlayer->bUseSilentAngles)
        {
            if (!speedapplied)
            {
                vsilent.x = cmd->forwardmove;
                vsilent.y = cmd->sidemove;
                vsilent.z = cmd->upmove;
                speed     = sqrt(vsilent.x * vsilent.x + vsilent.y * vsilent.y);
                VectorAngles(vsilent, ang);
                yaw                 = DEG2RAD(ang.y - g_pLocalPlayer->v_OrigViewangles.y + cmd->viewangles.y);
                cmd->forwardmove    = cos(yaw) * speed;
                cmd->sidemove       = sin(yaw) * speed;
                float clamped_pitch = fabsf(fmodf(cmd->viewangles.x, 360.0f));
                if (clamped_pitch >= 90 && clamped_pitch <= 270)
                    cmd->forwardmove = -cmd->forwardmove;
            }

            ret = false;
        }
        g_pLocalPlayer->UpdateEnd();
    }

    //	PROF_END("CreateMove");
    if (!(cmd->buttons & IN_ATTACK))
    {
        // LoadSavedState();
    }
    g_Settings.is_create_move = false;
    if (nolerp)
    {
        static const ConVar *pUpdateRate = g_pCVar->FindVar("cl_updaterate");
        if (!pUpdateRate)
            pUpdateRate = g_pCVar->FindVar("cl_updaterate");
        else
        {
            float interp = MAX(cl_interp->GetFloat(), cl_interp_ratio->GetFloat() / pUpdateRate->GetFloat());
            cmd->tick_count += TIME_TO_TICKS(interp);
        }
    }
    return ret;
}

void WriteCmd(IInput *input, CUserCmd *cmd, int sequence_nr)
{
    // Write the usercmd
    GetVerifiedCmds(input)[sequence_nr % VERIFIED_CMD_SIZE].m_cmd = *cmd;
    GetVerifiedCmds(input)[sequence_nr % VERIFIED_CMD_SIZE].m_crc = GetChecksum(cmd);
    GetCmds(input)[sequence_nr % VERIFIED_CMD_SIZE]               = *cmd;
}

// This gets called before the other CreateMove, but since we run original first in here all the stuff gets called after normal CreateMove is done
DEFINE_HOOKED_METHOD(CreateMoveEarly, void, IInput *this_, int sequence_nr, float input_sample_time, bool arg3)
{
    bSendPackets = reinterpret_cast<bool *>((uintptr_t) __builtin_frame_address(1) - 8);
    // Call original function, includes Normal CreateMove
    original::CreateMoveEarly(this_, sequence_nr, input_sample_time, arg3);

    CUserCmd *cmd = nullptr;
    if (this_ && GetCmds(this_) && sequence_nr > 0)
        cmd = this_->GetUserCmd(sequence_nr);

    if (!cmd)
        return;

    current_late_user_cmd = cmd;

    if (!isHackActive())
    {
        WriteCmd(this_, current_late_user_cmd, sequence_nr);
        return;
    }

    if (!g_IEngine->IsInGame())
    {
        WriteCmd(this_, current_late_user_cmd, sequence_nr);
        return;
    }

    PROF_SECTION(CreateMoveEarly);

    // Run EC
    EC::run(EC::CreateMoveEarly);

    // Write the usercmd
    WriteCmd(this_, current_late_user_cmd, sequence_nr);
}
} // namespace hooked_methods
