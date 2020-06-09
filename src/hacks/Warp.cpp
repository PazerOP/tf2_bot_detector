/*
 * Created on 16.4.2020
 * Author: BenCat07
 *
 * Copyright Nullworks 2020
 */

#include "common.hpp"
#if ENABLE_VISUALS
#include "drawing.hpp"
#endif
#include "MiscAimbot.hpp"
#include "PlayerTools.hpp"
#include "DetourHook.hpp"

namespace hacks::tf2::warp
{
static settings::Boolean enabled{ "warp.enabled", "false" };
static settings::Float speed{ "warp.speed", "23" };
static settings::Boolean draw{ "warp.draw", "false" };
static settings::Button warp_key{ "warp.key", "<null>" };
static settings::Boolean charge_passively{ "warp.charge-passively", "true" };
static settings::Boolean charge_in_jump{ "warp.charge-passively.jump", "true" };
static settings::Boolean charge_no_input{ "warp.charge-passively.no-inputs", "false" };
static settings::Int warp_movement_ratio{ "warp.movement-ratio", "6" };
static settings::Boolean warp_demoknight{ "warp.demoknight", "false" };
static settings::Boolean warp_peek{ "warp.peek", "false" };
static settings::Boolean warp_on_damage{ "warp.on-hit", "false" };
static settings::Boolean warp_forward{ "warp.on-hit.forward", "false" };
static settings::Boolean warp_backwards{ "warp.on-hit.backwards", "false" };
static settings::Boolean warp_left{ "warp.on-hit.left", "true" };
static settings::Boolean warp_right{ "warp.on-hit.right", "true" };

// Hidden control rvars for communtiy servers
static settings::Int maxusrcmdprocessticks("warp.maxusrcmdprocessticks", "24");

// Draw control
static settings::Int size{ "warp.bar-size", "100" };
static settings::Int bar_x{ "warp.bar-x", "50" };
static settings::Int bar_y{ "warp.bar-y", "200" };

static bool should_charge       = false;
static int warp_amount          = 0;
static int warp_amount_override = 0;
static bool should_melee        = false;
static bool charged             = false;

static bool should_warp = true;
static bool was_hurt    = false;

// Should we warp?
bool shouldWarp(bool check_amount)
{
    return ((warp_key && warp_key.isKeyDown()) || was_hurt) && (!check_amount || warp_amount);
}

// How many ticks of excess we have (for decimal speeds)
float excess_ticks = 0.0f;
int GetWarpAmount(bool finalTick)
{
    int max_extra_ticks = *maxusrcmdprocessticks - 1;
    // No limit set
    if (!*maxusrcmdprocessticks)
        max_extra_ticks = INT_MAX;
    float warp_amount_preprocessed = std::max(*speed, 0.05f);

    // How many ticks to warp, add excess too
    int warp_amount_processed = std::floor(warp_amount_preprocessed) + std::floor(excess_ticks);

    // Remove the used amount from excess
    if (finalTick)
        excess_ticks -= std::floor(excess_ticks);

    // Store excess
    if (finalTick)
        excess_ticks += warp_amount_preprocessed - std::floor(warp_amount_preprocessed);

    // Return smallest of the two
    return std::min(warp_amount_processed, max_extra_ticks);
}

DetourHook cl_move_detour;
typedef void (*CL_Move_t)(float accumulated_extra_samples, bool bFinalTick);

// Warping part, uses CL_Move
void Warp(float accumulated_extra_samples, bool finalTick)
{
    auto ch = g_IEngine->GetNetChannelInfo();
    if (!ch)
        return;
    if (!should_warp)
    {
        if (finalTick)
            should_warp = true;
        return;
    }

    int warp_ticks = warp_amount;
    if (warp_amount_override)
        warp_ticks = warp_amount_override;

    CL_Move_t original = (CL_Move_t) cl_move_detour.GetOriginalFunc();

    // Call CL_Move once for every warp tick
    int warp_amnt = GetWarpAmount(finalTick);
    if (warp_amnt)
    {
        int calls = std::min(warp_ticks, warp_amnt);
        for (int i = 0; i < calls; i++)
        {
            original(accumulated_extra_samples, finalTick);
            // Only decrease ticks for the final CL_Move tick
            if (finalTick)
            {
                warp_amount--;
                warp_ticks--;
            }
        }
    }
    cl_move_detour.RestorePatch();

    if (warp_amount_override)
        warp_amount_override = warp_ticks;

    if (warp_ticks <= 0)
    {
        was_hurt   = false;
        warp_ticks = 0;
        if (warp_amount_override)
            warp_amount_override = 0;
    }
}

int GetMaxWarpTicks()
{
    int ticks = *maxusrcmdprocessticks;
    // No limit set
    if (!ticks)
        ticks = INT_MAX;
    return ticks;
}

void SendNetMessage(INetMessage &msg)
{
    if (!enabled)
        return;

    // Credits to MrSteyk for this btw
    if (msg.GetGroup() == 0xA)
    {
        // Charge
        if (should_charge && !charged)
        {
            int ticks    = GetMaxWarpTicks();
            auto movemsg = (CLC_Move *) &msg;

            // Just null it :shrug:
            movemsg->m_nBackupCommands = 0;
            movemsg->m_nNewCommands    = 0;
            movemsg->m_DataOut.Reset();
            movemsg->m_DataOut.m_nDataBits  = 0;
            movemsg->m_DataOut.m_nDataBytes = 0;
            movemsg->m_DataOut.m_iCurBit    = 0;

            warp_amount++;
            if (warp_amount >= ticks)
            {
                warp_amount = ticks;
                charged     = true;
            }
        }
    }
    should_charge = false;
}

// Approximate demoknight shield speed at a given tick
float approximateSpeedAtTick(int ticks_since_start, float initial_speed)
{
    // Formula only holds up until like 20 ticks
    float speed = ticks_since_start >= 20 ? 750.0f : (ticks_since_start * (113.8f - 2.8f * ticks_since_start) + 1.0f);
    return std::min(750.0f * g_GlobalVars->interval_per_tick, initial_speed * g_GlobalVars->interval_per_tick + speed * g_GlobalVars->interval_per_tick);
}

// Approximate the amount of ticks needed for a given distance as demoknight
int approximateTicksForDist(float distance, float initial_speed, int max_ticks)
{
    float travelled_dist = 0.0f;
    for (int i = 0; i <= max_ticks; i++)
    {
        travelled_dist += approximateSpeedAtTick(i, initial_speed);

        // We hit the needed range
        if (travelled_dist >= distance)
            return i;
    }
    // Not within Max range
    return -1;
}

static bool move_last_tick     = true;
static bool warp_last_tick     = false;
static bool was_hurt_last_tick = false;
static int ground_ticks        = 0;
// Left and right by default
static std::vector<float> yaw_selections{ 90.0f, -90.0f };

enum charge_state
{
    ATTACK = 0,
    CHARGE,
    WARP,
    DONE
};

enum peek_state
{
    IDLE = 0,
    MOVE_TOWARDS,
    MOVE_BACK,
    STOP
};

charge_state current_state    = ATTACK;
peek_state current_peek_state = IDLE;

// Used to determine when we should start moving back with peek
static int charge_at_start = 0;

// Used to determine when demoknight warp should be over
static bool was_overridden = false;
void CreateMove()
{
    if (!enabled)
        return;
    if (CE_BAD(LOCAL_E) || !LOCAL_E->m_bAlivePlayer())
        return;
    if (!shouldWarp(false))
    {
        warp_last_tick     = false;
        current_state      = ATTACK;
        current_peek_state = IDLE;

        Vector velocity{};
        velocity::EstimateAbsVelocity(RAW_ENT(LOCAL_E), velocity);

        if (!charge_in_jump)
        {
            if (CE_INT(LOCAL_E, netvar.iFlags) & FL_ONGROUND)
                ground_ticks++;
            else
                ground_ticks = 0;
        }

        // Bunch of checks, if they all pass we are standing still
        if ((ground_ticks > 1 || charge_in_jump) && (charge_no_input || velocity.IsZero()) && !HasCondition<TFCond_Charging>(LOCAL_E) && !current_user_cmd->forwardmove && !current_user_cmd->sidemove && !current_user_cmd->upmove && !(current_user_cmd->buttons & IN_JUMP) && !(current_user_cmd->buttons & (IN_ATTACK | IN_ATTACK2)))
        {
            if (!move_last_tick)
                should_charge = true;
            move_last_tick = false;

            return;
        }
        else if (charge_passively && (charge_in_jump || ground_ticks > 1) && !(current_user_cmd->buttons & (IN_ATTACK | IN_ATTACK2)))
        {
            // Use everxy xth tick for charging
            if (*warp_movement_ratio > 0 && !(tickcount % *warp_movement_ratio))
                should_charge = true;
            move_last_tick = true;
        }
    }
    // Warp when hurt
    else if (was_hurt)
    {
        // Store direction
        static float yaw = 0.0f;
        // Select yaw
        if (!was_hurt_last_tick)
        {
            yaw = 0.0f;
            if (yaw_selections.empty())
                return;
            // Select randomly
            yaw = yaw_selections[UniformRandomInt(0, yaw_selections.size() - 1)];
        }
        // The yaw we want to achieve
        float actual_yaw = DEG2RAD(yaw);
        if (g_pLocalPlayer->bUseSilentAngles)
            actual_yaw = DEG2RAD(yaw);

        // Set forward/sidemove
        current_user_cmd->forwardmove = cos(actual_yaw) * 450.0f;
        current_user_cmd->sidemove    = -sin(actual_yaw) * 450.0f;
    }
    // Demoknight Warp
    else if (warp_demoknight)
    {
        switch (current_state)
        {
        case ATTACK:
        {
            // Get charge meter (0 - 100 range)
            float charge_meter = re::CTFPlayerShared::GetChargeMeter(re::CTFPlayerShared::GetPlayerShared(RAW_ENT(LOCAL_E)));

            // If our charge meter is full
            if (charge_meter == 100.0f)
            {
                // Find an entity meeting the Criteria and closest to crosshair
                std::pair<CachedEntity *, float> result{ nullptr, FLT_MAX };

                for (int i = 1; i <= g_IEngine->GetMaxClients(); i++)
                {
                    CachedEntity *ent = ENTITY(i);
                    if (CE_BAD(ent) || !ent->m_bAlivePlayer() || !ent->m_bEnemy() || !player_tools::shouldTarget(ent))
                        continue;
                    // No hitboxes
                    if (!ent->hitboxes.GetHitbox(2))
                        continue;
                    float FOVScore = GetFov(g_pLocalPlayer->v_OrigViewangles, g_pLocalPlayer->v_Eye, ent->hitboxes.GetHitbox(spine_1)->center);
                    if (FOVScore < result.second)
                    {
                        result.second = FOVScore;
                        result.first  = ent;
                    }
                }
                // We found a good entity within range, Set needed warp amount
                if (result.first)
                {
                    float distance = LOCAL_E->m_vecOrigin().DistTo(result.first->m_vecOrigin());

                    // We want to hit their bounding box, not their center
                    distance -= 40.0f;

                    // This approximates the ticks needed for the distance
                    Vector vel;
                    velocity::EstimateAbsVelocity(RAW_ENT(LOCAL_E), vel);
                    // +11 to account for the melee delay
                    int charge_ticks = approximateTicksForDist(distance, vel.Length(), GetMaxWarpTicks() + 11);

                    // Not in range, try again with melee range taken into compensation
                    if (charge_ticks <= 0)
                        charge_ticks = approximateTicksForDist(distance - 128.0f, vel.Length(), GetMaxWarpTicks() + 11);
                    // Out of range
                    if (charge_ticks <= 0)
                    {
                        charge_ticks = warp_amount;
                        should_melee = false;
                    }
                    else
                    {
                        charge_ticks = std::clamp(charge_ticks, 0, warp_amount);
                        // For every started batch We can subtract 1 because we'll get an extra CreateMove call.
                        charge_ticks -= std::ceil(charge_ticks / *speed);
                        should_melee = true;
                    }
                    // Use these ticks
                    warp_amount_override = charge_ticks;

                    was_overridden = true;
                }
                else
                {
                    should_melee   = false;
                    was_overridden = false;
                }

                // Force a crit
                criticals::force_crit_this_tick = true;
                if (should_melee)
                    current_user_cmd->buttons |= IN_ATTACK;
                current_state = CHARGE;
            }
            // Just warp normally if meter isn't full
            else
            {
                was_overridden = false;
                current_state  = WARP;
            }

            should_warp = false;

            break;
        }
        case CHARGE:
        {
            current_user_cmd->buttons |= IN_ATTACK2;
            current_state = WARP;
            should_warp   = false;
            break;
        }
        case WARP:
        {
            should_warp = true;
            if ((was_overridden && !warp_amount_override) || !warp_amount)
            {
                should_warp   = false;
                current_state = DONE;
            }
            break;
        }
        case DONE:
        {
            // Since we are done Warping we should stop trying to use any more
            should_warp = false;
            break;
        }
        default:
            break;
        }
    }
    // Warp peaking
    else if (warp_peek)
    {
        switch (current_peek_state)
        {
        case IDLE:
        {
            // Not doing anything, update warp amount
            charge_at_start = warp_amount;

            Vector vel;
            velocity::EstimateAbsVelocity(RAW_ENT(LOCAL_E), vel);

            // if we move more than 1.0 HU/s and buttons are pressed, and we are grounded, go to move towards statement...
            if (CE_INT(LOCAL_E, netvar.iFlags) & FL_ONGROUND && !vel.IsZero(1.0f) && current_user_cmd->buttons & (IN_FORWARD | IN_BACK | IN_MOVELEFT | IN_MOVERIGHT))
                current_peek_state = MOVE_TOWARDS;
            // ...else don't warp
            else
            {
                should_warp = false;
                break;
            }

            [[fallthrough]];
        }
        case MOVE_TOWARDS:
        {
            // Just wait until we used about a third of our warp, the rest has to be used for moving back
            if (warp_amount <= charge_at_start * (2.0f / 3.0f))
                current_peek_state = MOVE_BACK;
            break;
        }
        case MOVE_BACK:
        {
            // Inverse direction if we still have warp left
            if (warp_amount)
            {
                current_user_cmd->forwardmove *= -1.0f;
                current_user_cmd->sidemove *= -1.0f;
                break;
            }
            // ... Else we stop in our tracks
            else
                current_peek_state = STOP;

            [[fallthrough]];
        }
        case STOP:
        {
            // Stop dead in our tracks while key is still held
            current_user_cmd->forwardmove = 0.0f;
            current_user_cmd->sidemove    = 0.0f;
            break;
        }
        default:
            break;
        }
    }
    was_hurt_last_tick = was_hurt;
}

#if ENABLE_VISUALS
void Draw()
{
    if (!enabled || !draw)
        return;
    if (!g_IEngine->IsInGame())
        return;
    if (CE_BAD(LOCAL_E))
        return;

    float charge_percent = (float) warp_amount / (float) GetMaxWarpTicks();
    // Draw background
    static rgba_t background_color = colors::FromRGBA8(96, 96, 96, 150);
    float bar_bg_x_size            = *size * 2.0f;
    float bar_bg_y_size            = *size / 5.0f;
    draw::Rectangle(*bar_x - 5.0f, *bar_y - 5.0f, bar_bg_x_size + 10.0f, bar_bg_y_size + 10.0f, background_color);
    // Draw bar
    rgba_t color_bar = colors::orange;
    if (GetMaxWarpTicks() == warp_amount)
        color_bar = colors::green;
    color_bar.a = 100 / 255.0f;
    draw::Rectangle(*bar_x, *bar_y, *size * 2.0f * charge_percent, *size / 5.0f, color_bar);
}
#endif

void LevelShutdown()
{
    charged     = false;
    warp_amount = 0;
}

class WarpHurtListener : public IGameEventListener2
{
public:
    virtual void FireGameEvent(IGameEvent *event)
    { // Not enabled
        if (!enabled || !warp_on_damage)
            return;
        // We have no warp
        if (!warp_amount)
            return;
        // Store userids
        int victim   = event->GetInt("userid");
        int attacker = event->GetInt("attacker");
        player_info_s kinfo{};
        player_info_s vinfo{};

        // Check if both are valid (Attacker & victim)
        if (!g_IEngine->GetPlayerInfo(g_IEngine->GetPlayerForUserID(victim), &vinfo) || !g_IEngine->GetPlayerInfo(g_IEngine->GetPlayerForUserID(attacker), &kinfo))
            return;
        // Check if victim is local player
        if (g_IEngine->GetPlayerForUserID(victim) != g_pLocalPlayer->entity_idx)
            return;
        // Ignore projectiles for now
        if (GetWeaponMode(ENTITY(attacker)) == weapon_projectile)
            return;
        // We got hurt
        was_hurt = true;
    }
};

static WarpHurtListener listener;

void rvarCallback(settings::VariableBase<bool> &, bool)
{
    yaw_selections.clear();
    if (warp_forward)
        yaw_selections.push_back(0.0f);
    if (warp_backwards)
        yaw_selections.push_back(-180.0f);
    if (warp_left)
        yaw_selections.push_back(-90.0f);
    if (warp_right)
        yaw_selections.push_back(90.0f);
}

DetourHook cl_sendmove_detour;
typedef void (*CL_SendMove_t)();
void CL_SendMove_hook()
{
    byte data[4000];

    // the +4 one is choked commands
    int nextcommandnr = NET_INT(g_IBaseClientState, offsets::lastoutgoingcommand()) + NET_INT(g_IBaseClientState, offsets::lastoutgoingcommand() + 4) + 1;

    // send the client update packet

    CLC_Move moveMsg;

    moveMsg.m_DataOut.StartWriting(data, sizeof(data));

    // Determine number of backup commands to send along
    int cl_cmdbackup = 2;

    // How many real new commands have queued up
    moveMsg.m_nNewCommands = 1 + NET_INT(g_IBaseClientState, offsets::lastoutgoingcommand() + 4);
    moveMsg.m_nNewCommands = std::clamp(moveMsg.m_nNewCommands, 0, 15);

    // Excessive commands (Used for longer fakelag, credits to https://www.unknowncheats.me/forum/source-engine/370916-23-tick-guwop-fakelag-break-lag-compensation-running.html)
    int extra_commands        = NET_INT(g_IBaseClientState, offsets::lastoutgoingcommand() + 4) + 1 - moveMsg.m_nNewCommands;
    cl_cmdbackup              = std::max(2, extra_commands);
    moveMsg.m_nBackupCommands = std::clamp(cl_cmdbackup, 0, 7);

    int numcmds = moveMsg.m_nNewCommands + moveMsg.m_nBackupCommands;

    int from = -1; // first command is deltaed against zeros

    bool bOK = true;

    for (int to = nextcommandnr - numcmds + 1; to <= nextcommandnr; to++)
    {
        bool isnewcmd = to >= (nextcommandnr - moveMsg.m_nNewCommands + 1);

        // Call the write to buffer
        typedef bool (*WriteUsercmdDeltaToBuffer_t)(IBaseClientDLL *, bf_write *, int, int, bool);

        // first valid command number is 1
        bOK  = bOK && vfunc<WriteUsercmdDeltaToBuffer_t>(g_IBaseClient, offsets::PlatformOffset(23, offsets::undefined, offsets::undefined), 0)(g_IBaseClient, &moveMsg.m_DataOut, from, to, isnewcmd);
        from = to;
    }

    if (bOK)
    {
        // Make fakelag work as we want it to
        if (extra_commands > 0)
            ((INetChannel *) g_IEngine->GetNetChannelInfo())->m_nChokedPackets -= extra_commands;

        // only write message if all usercmds were written correctly, otherwise parsing would fail
        ((INetChannel *) g_IEngine->GetNetChannelInfo())->SendNetMsg(moveMsg);
    }
}

void CL_Move_hook(float accumulated_extra_samples, bool bFinalTick)
{
    CL_Move_t original = (CL_Move_t) cl_move_detour.GetOriginalFunc();
    original(accumulated_extra_samples, bFinalTick);
    cl_move_detour.RestorePatch();

    // Should we warp?
    if (shouldWarp(true))
    {
        Warp(accumulated_extra_samples, bFinalTick);
        if (warp_amount < GetMaxWarpTicks())
            charged = false;
    }
}

static InitRoutine init([]() {
    static auto cl_sendmove_addr = gSignatures.GetEngineSignature("55 89 E5 57 56 53 81 EC 2C 10 00 00 C6 85 ? ? ? ? 01");
    cl_sendmove_detour.Init(cl_sendmove_addr, (void *) CL_SendMove_hook);
    static auto cl_move_addr = gSignatures.GetEngineSignature("55 89 E5 57 56 53 81 EC 9C 00 00 00 83 3D ? ? ? ? 01");
    cl_move_detour.Init(cl_move_addr, (void *) CL_Move_hook);

    EC::Register(EC::LevelShutdown, LevelShutdown, "warp_levelshutdown");
    EC::Register(EC::CreateMove, CreateMove, "warp_createmove", EC::very_late);
    g_IEventManager2->AddListener(&listener, "player_hurt", false);
    EC::Register(
        EC::Shutdown,
        []() {
            g_IEventManager2->RemoveListener(&listener);
            cl_sendmove_detour.Shutdown();
            cl_move_detour.Shutdown();
        },
        "warp_shutdown");
    warp_forward.installChangeCallback(rvarCallback);
    warp_backwards.installChangeCallback(rvarCallback);
    warp_left.installChangeCallback(rvarCallback);
    warp_right.installChangeCallback(rvarCallback);

#if ENABLE_VISUALS
    EC::Register(EC::Draw, Draw, "warp_draw");
#endif
});
} // namespace hacks::tf2::warp
