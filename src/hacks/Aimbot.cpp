/*
 * HAimbot.cpp
 *
 *  Created on: Oct 9, 2016
 *      Author: nullifiedcat
 */

#include <hacks/Aimbot.hpp>
#include <hacks/CatBot.hpp>
#include <hacks/AntiAim.hpp>
#include <hacks/ESP.hpp>
#include <hacks/Backtrack.hpp>
#include <PlayerTools.hpp>
#include <settings/Bool.hpp>
#include "common.hpp"
#include "MiscTemporary.hpp"
#include <targethelper.hpp>
#include "hitrate.hpp"
#include "FollowBot.hpp"

#if ENABLE_VISUALS
#ifndef FEATURE_EFFECTS_DISABLED
#include "EffectChams.hpp"
#endif
#endif

namespace hacks::shared::aimbot
{
static settings::Boolean normal_enable{ "aimbot.enable", "false" };
static settings::Button aimkey{ "aimbot.aimkey.button", "<null>" };
static settings::Int aimkey_mode{ "aimbot.aimkey.mode", "1" };
static settings::Boolean autoshoot{ "aimbot.autoshoot", "1" };
static settings::Boolean autoreload{ "aimbot.autoshoot.activate-heatmaker", "false" };
static settings::Boolean autoshoot_disguised{ "aimbot.autoshoot-disguised", "1" };
static settings::Boolean multipoint{ "aimbot.multipoint", "false" };
static settings::Int hitbox_mode{ "aimbot.hitbox-mode", "0" };
static settings::Float normal_fov{ "aimbot.fov", "0" };
static settings::Int priority_mode{ "aimbot.priority-mode", "0" };
static settings::Boolean wait_for_charge{ "aimbot.wait-for-charge", "0" };

static settings::Boolean silent{ "aimbot.silent", "1" };
static settings::Boolean target_lock{ "aimbot.lock-target", "0" };
#if ENABLE_VISUALS
static settings::Boolean assistance_only{ "aimbot.assistance.only", "0" };
#endif
static settings::Int hitbox{ "aimbot.hitbox", "0" };
static settings::Boolean zoomed_only{ "aimbot.zoomed-only", "1" };
static settings::Boolean only_can_shoot{ "aimbot.can-shoot-only", "1" };

static settings::Boolean extrapolate{ "aimbot.extrapolate", "0" };
static settings::Int normal_slow_aim{ "aimbot.slow", "0" };
static settings::Int miss_chance{ "aimbot.miss-chance", "0" };

static settings::Boolean projectile_aimbot{ "aimbot.projectile.enable", "true" };
static settings::Float proj_gravity{ "aimbot.projectile.gravity", "0" };
static settings::Float proj_speed{ "aimbot.projectile.speed", "0" };

static settings::Float sticky_autoshoot{ "aimbot.projectile.sticky-autoshoot", "0.5" };

static settings::Boolean aimbot_debug{ "aimbot.debug", "0" };
static settings::Boolean engine_projpred{ "aimbot.debug.engine-pp", "0" };

static settings::Boolean auto_spin_up{ "aimbot.auto.spin-up", "0" };
static settings::Boolean minigun_tapfire{ "aimbot.auto.tapfire", "false" };
static settings::Boolean auto_zoom{ "aimbot.auto.zoom", "0" };
static settings::Boolean auto_unzoom{ "aimbot.auto.unzoom", "0" };

static settings::Boolean backtrackAimbot{ "aimbot.backtrack", "0" };
static bool force_backtrack_aimbot = false;
static settings::Boolean backtrackVischeckAll{ "aimbot.backtrack.vischeck-all", "0" };

// TODO maybe these should be moved into "Targeting"
static settings::Float max_range{ "aimbot.target.max-range", "4096" };
static settings::Boolean ignore_vaccinator{ "aimbot.target.ignore-vaccinator", "1" };
static settings::Boolean ignore_deadringer{ "aimbot.target.ignore-deadringer", "1" };
static settings::Boolean buildings_sentry{ "aimbot.target.sentry", "1" };
static settings::Boolean buildings_other{ "aimbot.target.other-buildings", "1" };
static settings::Boolean stickybot{ "aimbot.target.stickybomb", "0" };
static settings::Boolean rageonly{ "aimbot.target.ignore-non-rage", "0" };
static settings::Int teammates{ "aimbot.target.teammates", "0" };

/*
 * 0 Always on
 * 1 Disable if being spectated in first person
 * 2 Disable if being spectated
 */
static settings::Int specmode("aimbot.spectator-mode", "0");
static settings::Boolean specenable("aimbot.spectator.enable", "0");
static settings::Float specfov("aimbot.spectator.fov", "0");
static settings::Int specslow("aimbot.spectator.slow", "0");

int slow_aim;
float fov;
bool enable;

static void spectatorUpdate()
{
    switch (*specmode)
    {
    // Always on
    default:
    case 0:
        break;
        // Disable if being spectated in first person
    case 1:
        if (g_pLocalPlayer->spectator_state == g_pLocalPlayer->FIRSTPERSON)
        {
            enable   = *specenable;
            slow_aim = *specslow;
            fov      = *specfov;
        }
        break;
        // Disable if being spectated
    case 2:
        if (g_pLocalPlayer->spectator_state != g_pLocalPlayer->NONE)
        {
            enable   = *specenable;
            slow_aim = *specslow;
            fov      = *specfov;
        }
    };
}

#if ENABLE_VISUALS
static settings::Boolean fov_draw{ "aimbot.fov-circle.enable", "0" };
static settings::Float fovcircle_opacity{ "aimbot.fov-circle.opacity", "0.7" };
#endif

int PreviousX, PreviousY;
int CurrentX, CurrentY;

float last_mouse_check = 0;
float stop_moving_time = 0;

int GetSentry()
{
    for (int i = 1; i <= HIGHEST_ENTITY; i++)
    {
        CachedEntity *ent = ENTITY(i);
        if (CE_BAD(ent))
            continue;
        if ((ent->m_Type() != ENTITY_BUILDING && ent->m_iClassID() != CL_CLASS(CTFTankBoss)) || ent->m_iClassID() != CL_CLASS(CObjectSentrygun))
            continue;
        if ((CE_INT(ent, netvar.m_hBuilder) & 0xFFF) != g_pLocalPlayer->entity_idx)
            continue;
        return i;
    }
    return -1;
}

settings::Boolean ignore_cloak{ "aimbot.target.ignore-cloaked-spies", "1" };
// Projectile info
bool projectile_mode{ false };
float cur_proj_speed{ 0.0f };
float cur_proj_grav{ 0.0f };

bool shouldBacktrack()
{
    return !projectile_mode && (*backtrackAimbot || force_backtrack_aimbot) && hacks::tf2::backtrack::isBacktrackEnabled;
}

// Am I holding Hitman's Heatmaker ?
static bool CarryingHeatmaker()
{
    return CE_INT(LOCAL_W, netvar.iItemDefinitionIndex) == 752;
}

// Backtrack filter for aimbot
bool aimbotTickFilter(CachedEntity *ent, hacks::tf2::backtrack::BacktrackData tick)
{
    // FOV check
    if (fov > 0.0f)
    {
        float fov_scr = GetFov(g_pLocalPlayer->v_OrigViewangles, g_pLocalPlayer->v_Eye, tick.hitboxes.at(0).center);
        // Failed FOV check
        if (fov_scr > fov)
            return false;
    }
    // Not hitscan, no vischeck needed
    if (g_pLocalPlayer->weapon_mode != weapon_hitscan)
        return true;
    // Return visibility
    return IsEntityVectorVisible(ent, tick.hitboxes.at(head).center, MASK_SHOT);
}
static void doAutoZoom(bool target_found)
{
    bool isIdle = target_found ? false : hacks::shared::followbot::isIdle();

    // Keep track of our zoom time
    static Timer zoomTime{};

    // Minigun spun up handler
    if (auto_spin_up && g_pLocalPlayer->weapon()->m_iClassID() == CL_CLASS(CTFMinigun))
    {
        if (target_found)
            zoomTime.update();
        if (isIdle || !zoomTime.check(3000))
        {
            current_user_cmd->buttons |= IN_ATTACK2;
        }
        return;
    }

    if (auto_zoom && g_pLocalPlayer->holding_sniper_rifle && (target_found || isIdle))
    {
        if (target_found)
            zoomTime.update();
        if (not g_pLocalPlayer->bZoomed)
            current_user_cmd->buttons |= IN_ATTACK2;
    }
    else if (!target_found)
    {
        // Auto-Unzoom
        if (auto_unzoom)
            if (g_pLocalPlayer->holding_sniper_rifle && g_pLocalPlayer->bZoomed && zoomTime.check(3000))
                current_user_cmd->buttons |= IN_ATTACK2;
    }
}

// Current Entity
int target_eid{ 0 };
CachedEntity *target      = 0;
CachedEntity *target_last = 0;
bool foundTarget          = false;

// If slow aimbot allows autoshoot
bool slow_can_shoot = false;
bool projectileAimbotRequired;

// This array will store calculated projectile/hitscan predictions
// for current frame, to avoid performing them again
AimbotCalculatedData_s calculated_data_array[2048]{};
// The main "loop" of the aimbot.
static void CreateMove()
{
    if (CE_BAD(LOCAL_E) || !LOCAL_E->m_bAlivePlayer() || CE_BAD(LOCAL_W))
        return;

    enable   = *normal_enable;
    slow_aim = *normal_slow_aim;
    fov      = *normal_fov;

    spectatorUpdate();

    if (!enable)
        return;

    doAutoZoom(false);

    if (LOCAL_W->m_iClassID() == CL_CLASS(CTFMinigun) && CE_INT(LOCAL_E, netvar.m_iAmmo + 4) == 0)
        return;
    // We do this as we need to pass whether the aimkey allows aiming to both
    // the find target and aiming system. If we just call the func than toggle
    // aimkey would break so we save it to a var to use it twice
    bool aimkey_status = UpdateAimkey();
    // check if we need to run projectile Aimbot code
    projectile_mode          = false;
    projectileAimbotRequired = false;
    if (projectile_aimbot && (g_pLocalPlayer->weapon_mode == weapon_projectile || g_pLocalPlayer->weapon_mode == weapon_throwable))
        projectileAimbotRequired = true;

    // Local player check + Aimkey
    if (!aimkey_status || !ShouldAim())
        return;

    // Refresh projectile info
    if (projectileAimbotRequired)
    {
        projectile_mode = GetProjectileData(g_pLocalPlayer->weapon(), cur_proj_speed, cur_proj_grav);
        if (!projectile_mode)
            return;
        if (proj_speed)
            cur_proj_speed = float(proj_speed);
        if (proj_gravity)
            cur_proj_grav = float(proj_gravity);
    }
    // Refresh our best target
    CachedEntity *target_entity = RetrieveBestTarget(aimkey_status);
    if (CE_BAD(target_entity) || !foundTarget)
        return;

    // Auto-zoom
    doAutoZoom(true);

    // If zoomed only is on, check if zoomed
    if (zoomed_only && g_pLocalPlayer->holding_sniper_rifle)
    {
        if (!g_pLocalPlayer->bZoomed && !(current_user_cmd->buttons & IN_ATTACK))
            return;
    }
    if (!g_IEntityList->GetClientEntity(target_entity->m_IDX))
        return;
    if (!target_entity->hitboxes.GetHitbox(calculated_data_array[target_entity->m_IDX].hitbox))
        return;

#if ENABLE_VISUALS
    if (target_entity->m_Type() == ENTITY_PLAYER)
    {
        hacks::shared::esp::SetEntityColor(target_entity, colors::pink);
        effect_chams::g_EffectChams.SetEntityColor(target_entity, colors::pink);
    }
#endif

    // Attemt to auto-shoot

    // flNextPrimaryAttack meme
    // target_eid = target_entity->m_IDX;
    if (only_can_shoot && g_pLocalPlayer->weapon()->m_iClassID() != CL_CLASS(CTFMinigun) && g_pLocalPlayer->weapon()->m_iClassID() != CL_CLASS(CTFLaserPointer))
    {

        // Handle Compound bow
        if (g_pLocalPlayer->weapon()->m_iClassID() == CL_CLASS(CTFCompoundBow))
        {
            bool release = false;
            // Grab time when charge began
            current_user_cmd->buttons |= IN_ATTACK;
            float begincharge = CE_FLOAT(g_pLocalPlayer->weapon(), netvar.flChargeBeginTime);
            float charge      = g_GlobalVars->curtime - begincharge;
            int damage        = std::floor(50.0f + 70.0f * fminf(1.0f, charge));
            int charge_damage = std::floor(50.0f + 70.0f * fminf(1.0f, charge)) * 3.0f;
            if (!wait_for_charge || (damage >= target_entity->m_iHealth() || charge_damage >= target_entity->m_iHealth()))
                release = true;
            if (release)
                DoAutoshoot();
            static bool currently_charging_huntsman = false;

            // Hunstman started charging
            if (CE_FLOAT(g_pLocalPlayer->weapon(), netvar.flChargeBeginTime) != 0)
                currently_charging_huntsman = true;

            // Huntsman was released
            if (!(current_user_cmd->buttons & IN_ATTACK) && currently_charging_huntsman)
            {
                currently_charging_huntsman = false;
                Aim(target_entity);
            }
            else
                return;

            // Not release type weapon
        }
        else if (LOCAL_W->m_iClassID() == CL_CLASS(CTFPipebombLauncher))
        {
            float chargebegin = *((float *) ((uintptr_t) RAW_ENT(LOCAL_W) + 3152));
            float chargetime  = g_GlobalVars->curtime - chargebegin;

            DoAutoshoot();
            static bool currently_charging_pipe = false;

            // Grenade started charging
            if (chargetime < 6.0f && chargetime)
                currently_charging_pipe = true;

            // Grenade was released
            if (!(current_user_cmd->buttons & IN_ATTACK) && currently_charging_pipe)
            {
                currently_charging_pipe = false;
                Aim(target_entity);
            }
            else
                return;

            // Not release type weapon
        }
        else if (GetWeaponMode() == weapon_melee)
        {
            DoAutoshoot();
            if (g_pLocalPlayer->weapon_melee_damage_tick)
                Aim(target_entity);
        }
        else if (CanShoot() && CE_INT(LOCAL_W, netvar.m_iClip1) != 0)
        {
            Aim(target_entity);
            DoAutoshoot(target_entity);
        }
    }
    else
    {
        Aim(target_entity);
        // We should tap fire with the minigun on Bigger ranges to maximize damage, else just shoot normally
        if (!minigun_tapfire || g_pLocalPlayer->weapon()->m_iClassID() != CL_CLASS(CTFMinigun))
            DoAutoshoot(target_entity);
        else if (minigun_tapfire)
        {
            // Used to keep track of what tick we're in right now
            static int tapfire_delay = 0;
            tapfire_delay++;

            // This is the exact delay needed to hit
            if (tapfire_delay == 17 || target_entity->m_flDistance() <= 1250.0f)
            {
                DoAutoshoot(target_entity);
                tapfire_delay = 0;
            }
        }
    }
}
#if ENABLE_VISUALS
bool MouseMoving()
{
    if ((g_GlobalVars->curtime - last_mouse_check) < 0.02)
    {
        auto previous = SDL_GetMouseState(&PreviousX, &PreviousY);
    }
    else
    {
        auto current     = SDL_GetMouseState(&CurrentX, &CurrentY);
        last_mouse_check = g_GlobalVars->curtime;
    }

    if (PreviousX != CurrentX || PreviousY != CurrentY)
    {
        stop_moving_time = g_GlobalVars->curtime + 0.5;
        return true;
    }
    else if (g_GlobalVars->curtime <= stop_moving_time)
    {
        return true;
    }
    else
    {
        return false;
    }
}
#endif

// The first check to see if the player should aim in the first place
bool ShouldAim()
{
    // Checks should be in order: cheap -> expensive

    // Check for +use
    if (current_user_cmd->buttons & IN_USE)
        return false;
    // Check if using action slot item
    if (g_pLocalPlayer->using_action_slot_item)
        return false;
    // Using a forbidden weapon?
    if (g_pLocalPlayer->weapon()->m_iClassID() == CL_CLASS(CTFKnife))
        return false;

    IF_GAME(IsTF2())
    {
        // Carrying A building?
        if (CE_BYTE(g_pLocalPlayer->entity, netvar.m_bCarryingObject))
            return false;
        // Deadringer out?
        if (CE_BYTE(g_pLocalPlayer->entity, netvar.m_bFeignDeathReady))
            return false;
        // Is taunting?
        if (HasCondition<TFCond_Taunting>(g_pLocalPlayer->entity))
            return false;
        // Is cloaked
        if (IsPlayerInvisible(g_pLocalPlayer->entity))
            return false;
    }
#if ENABLE_VISUALS
    if (assistance_only && !MouseMoving())
        return false;
#endif

    IF_GAME(IsTF2())
    {
        switch (GetWeaponMode())
        {
        case weapon_hitscan:
            break;
        case weapon_melee:
            break;
        // Check we need to run projectile Aimbot code
        case weapon_projectile:
            if (!projectileAimbotRequired)
                return false;
            break;
        // Check if player doesnt have a weapon usable by aimbot
        default:
            return false;
        };
    }

    IF_GAME(IsTF())
    {
        // Check if player is zooming
        if (g_pLocalPlayer->bZoomed)
        {
            if (!(current_user_cmd->buttons & (IN_ATTACK | IN_ATTACK2)))
            {
                if (!CanHeadshot())
                    return false;
            }
        }
    }
    return true;
}

// Function to find a suitable target
CachedEntity *RetrieveBestTarget(bool aimkey_state)
{
    // If we have a previously chosen target, target lock is on, and the aimkey
    // is allowed, then attemt to keep the previous target
    if (target_lock && foundTarget && aimkey_state)
    {
        if (CE_GOOD(target_last))
        {
            // Check if previous target is still good
            if (IsTargetStateGood(target_last))
            {
                // If it is then return it again
                return target_last;
            }
        }
    }

    // We dont have a target currently so we must find one, reset statuses
    foundTarget = false;
    target_last = nullptr;
    target_eid  = -1;

    float target_highest_score, scr = 0.0f;
    CachedEntity *ent;
    CachedEntity *target_highest_ent = 0;
    target_highest_score             = -256;
    for (int i = 1; i <= HIGHEST_ENTITY; i++)
    {
        ent = ENTITY(i);
        if (CE_BAD(ent))
            continue; // Check for null and dormant
        // Check whether the current ent is good enough to target
        if (IsTargetStateGood(ent))
        {
            // Distance Priority, Uses this is melee is used
            if (GetWeaponMode() == weaponmode::weapon_melee || (int) priority_mode == 2)
                scr = 4096.0f - calculated_data_array[i].aim_position.DistTo(g_pLocalPlayer->v_Eye);
            else
            {
                switch ((int) priority_mode)
                {
                case 0: // Smart Priority
                    scr = GetScoreForEntity(ent);
                    break;
                case 1: // Fov Priority
                    scr = 360.0f - calculated_data_array[ent->m_IDX].fov;
                    break;
                case 3: // Health Priority (Lowest)
                    scr = 450.0f - ent->m_iHealth();
                    break;
                case 4: // Distance Priority (Furthest Away)
                    scr = calculated_data_array[i].aim_position.DistTo(g_pLocalPlayer->v_Eye);
                    break;
                case 5: // Health Priority (Highest)
                    scr = ent->m_iHealth() * 4;
                    break;
                default:
                    break;
                }
            }
            // Compare the top score to our current ents score
            if (scr > target_highest_score)
            {
                foundTarget          = true;
                target_highest_score = scr;
                target_highest_ent   = ent;
            }
        }
    }

    // Save the ent for future use with target lock
    target_last = target_highest_ent;

    if (CE_GOOD(target_last))
        target_eid = target_last->m_IDX;

    return target_highest_ent;
}

// A second check to determine whether a target is good enough to be aimed at
bool IsTargetStateGood(CachedEntity *entity)
{
    PROF_SECTION(PT_aimbot_targetstatecheck);

    // Checks for Players
    if (entity->m_Type() == ENTITY_PLAYER)
    {
        // Local player check
        if (entity == LOCAL_E)
            return false;
        // Dead
        if (!entity->m_bAlivePlayer())
            return false;
        // Teammates
        if ((int) teammates != 2 && ((!entity->m_bEnemy() && !teammates) || (entity->m_bEnemy() && teammates)))
            return false;
        // Distance
        if (EffectiveTargetingRange())
        {
            if (g_pLocalPlayer->weapon_mode != weapon_melee)
            {
                if (entity->m_flDistance() > EffectiveTargetingRange())
                    return false;
            }
            else
            {
                float swingrange = EffectiveTargetingRange();
                if (!shouldBacktrack() || entity->m_Type() != ENTITY_PLAYER)
                {
                    int hb = BestHitbox(entity);
                    if (hb == -1)
                        return false;
                    Vector newangle = GetAimAtAngles(g_pLocalPlayer->v_Eye, entity->hitboxes.GetHitbox(hb)->center, LOCAL_E);
                    trace_t trace;
                    Ray_t ray;
                    trace::filter_default.SetSelf(RAW_ENT(g_pLocalPlayer->entity));
                    ray.Init(g_pLocalPlayer->v_Eye, GetForwardVector(g_pLocalPlayer->v_Eye, newangle, swingrange, LOCAL_E));
                    g_ITrace->TraceRay(ray, MASK_SHOT_HULL, &trace::filter_default, &trace);
                    if ((IClientEntity *) trace.m_pEnt != RAW_ENT(entity))
                        return false;
                }
                else
                {
                    // This does vischecks and everything
                    auto data = hacks::tf2::backtrack::getClosestEntTick(entity, LOCAL_E->m_vecOrigin(), aimbotTickFilter);
                    // No data found
                    if (!data)
                        return false;
                    // Is within range?
                    Vector melee_vec = data->m_vecOrigin;
                    melee_vec.z      = g_pLocalPlayer->v_Eye.z;
                    // Out of range
                    if (melee_vec.DistTo(g_pLocalPlayer->v_Eye) >= swingrange)
                        return false;
                }
            }
        }
        // Rage only check
        if (rageonly)
        {
            if (playerlist::AccessData(entity).state != playerlist::k_EState::RAGE)
            {
                return false;
            }
        }
        IF_GAME(IsTF())
        {
            // don't aim if holding sapper
            if (g_pLocalPlayer->holding_sapper)
                return false;

            // Wait for charge
            if (wait_for_charge && g_pLocalPlayer->holding_sniper_rifle)
            {
                float cdmg  = CE_FLOAT(LOCAL_W, netvar.flChargedDamage) * 3;
                float maxhs = 450.0f;
                if (CE_INT(LOCAL_W, netvar.iItemDefinitionIndex) == 230 || HasCondition<TFCond_Jarated>(entity))
                {
                    cdmg  = int(CE_FLOAT(LOCAL_W, netvar.flChargedDamage) * 1.35f);
                    maxhs = 203.0f;
                }
                bool maxCharge = cdmg >= maxhs;

                // Darwins damage correction, Darwins protects against 15% of
                // damage
                //                if (HasDarwins(entity))
                //                    cdmg = (cdmg * .85) - 1;
                // Vaccinator damage correction, Vac charge protects against 75%
                // of damage
                if (HasCondition<TFCond_UberBulletResist>(entity))
                {
                    cdmg = (cdmg * .25) - 1;
                    // Passive bullet resist protects against 10% of damage
                }
                else if (HasCondition<TFCond_SmallBulletResist>(entity))
                {
                    cdmg = (cdmg * .90) - 1;
                }
                // Invis damage correction, Invis spies get protection from 10%
                // of damage
                if (IsPlayerInvisible(entity))
                    cdmg = (cdmg * .80) - 1;

                // Check if player will die from headshot or if target has more
                // than 450 health and sniper has max chage
                float hsdmg = 150.0f;
                if (CE_INT(LOCAL_W, netvar.iItemDefinitionIndex) == 230)
                    hsdmg = int(50.0f * 1.35f);
                if (!(entity->m_iHealth() <= hsdmg || entity->m_iHealth() <= cdmg || !g_pLocalPlayer->bZoomed || (maxCharge && entity->m_iHealth() > maxhs)))
                {
                    return false;
                }
            }

            // Some global checks
            if (!player_tools::shouldTarget(entity))
                return false;
            if (hacks::shared::catbot::should_ignore_player(entity))
                return false;
            // Invulnerable players, ex: uber, bonk
            if (IsPlayerInvulnerable(entity))
                return false;
            // cloaked/deadringed players
            if (ignore_cloak || ignore_deadringer)
            {
                if (IsPlayerInvisible(entity))
                {
                    // Item id for deadringer is 59 as of time of creation
                    if (HasWeapon(entity, 59))
                    {
                        if (ignore_deadringer)
                            return false;
                    }
                    else
                    {
                        if (ignore_cloak)
                            return false;
                    }
                }
            }
            // Vaccinator
            if (ignore_vaccinator && IsPlayerResistantToCurrentWeapon(entity))
                return false;
        }

        // Preform hitbox prediction
        int hitbox                 = BestHitbox(entity);
        AimbotCalculatedData_s &cd = calculated_data_array[entity->m_IDX];
        cd.hitbox                  = hitbox;

        // Vis check + fov check
        if (!VischeckPredictedEntity(entity))
            return false;
        if (LOCAL_W->m_iClassID() == CL_CLASS(CTFLaserPointer))
        {
            int sentry = GetSentry();
            if (sentry == -1)
                return false;
            Vector pos = GetBuildingPosition(ENTITY(sentry));
            if (hitbox == -1 || !entity->hitboxes.GetHitbox(cd.hitbox))
                return false;
            if (shouldBacktrack())
            {
                // This does vischecks and everything
                auto data = hacks::tf2::backtrack::getClosestEntTick(entity, LOCAL_E->m_vecOrigin(), aimbotTickFilter);
                // No data found
                if (!data)
                    return false;
            }
            else if (!IsVectorVisible(pos, entity->hitboxes.GetHitbox(cd.hitbox)->center, false, ENTITY(sentry)))
                return false;
        }
        if (fov > 0.0f && cd.fov > fov)
            return false;

        return true;

        // Check for buildings
    }
    else if (entity->m_Type() == ENTITY_BUILDING || entity->m_iClassID() == CL_CLASS(CTFTankBoss))
    {
        // Don't aim if holding sapper
        if (g_pLocalPlayer->holding_sapper)
            return false;
        // Enabled check
        if (!(buildings_other || buildings_sentry))
            return false;
        // Teammates, Even with friendly fire enabled, buildings can NOT be
        // damaged
        if (!entity->m_bEnemy())
            return false;
        // Distance
        if (EffectiveTargetingRange())
        {
            if (entity->m_flDistance() > (int) EffectiveTargetingRange())
                return false;
        }

        // Building type
        if (!(buildings_other && buildings_sentry))
        {
            // Check if target is a sentrygun
            if (entity->m_iClassID() == CL_CLASS(CObjectSentrygun))
            {
                if (!buildings_sentry)
                    return false;
                // Other
            }
            else
            {
                if (!buildings_other)
                    return false;
            }
        }

        // Grab the prediction var
        AimbotCalculatedData_s &cd = calculated_data_array[entity->m_IDX];

        // Vis and fov checks
        if (!VischeckPredictedEntity(entity))
            return false;
        if (LOCAL_W->m_iClassID() == CL_CLASS(CTFLaserPointer))
        {
            int sentry = GetSentry();
            if (sentry == -1)
                return false;
            Vector pos = GetBuildingPosition(ENTITY(sentry));
            if (!IsVectorVisible(pos, GetBuildingPosition(entity), false, ENTITY(sentry)))
                return false;
        }
        if (fov > 0.0f && cd.fov > fov)
            return false;

        return true;

        // Check for stickybombs
    }
    else if (entity->m_iClassID() == CL_CLASS(CTFGrenadePipebombProjectile))
    {
        // Enabled
        if (!stickybot)
            return false;

        // Only hitscan weapons can break stickys so check for them.
        if (!(GetWeaponMode() == weapon_hitscan || GetWeaponMode() == weapon_melee))
            return false;

        // Distance
        if (EffectiveTargetingRange())
        {
            if (entity->m_flDistance() > (int) EffectiveTargetingRange())
                return false;
        }

        // Teammates, Even with friendly fire enabled, stickys can NOT be
        // destroied
        if (!entity->m_bEnemy())
            return false;

        // Check if target is a pipe bomb
        if (CE_INT(entity, netvar.iPipeType) != 1)
            return false;

        // Moving Sticky?
        if (!CE_VECTOR(entity, netvar.vVelocity).IsZero(1.0f))
            return false;

        // Grab the prediction var
        AimbotCalculatedData_s &cd = calculated_data_array[entity->m_IDX];

        // Vis and fov check
        if (!VischeckPredictedEntity(entity))
            return false;
        if (LOCAL_W->m_iClassID() == CL_CLASS(CTFLaserPointer))
        {
            int sentry = GetSentry();
            if (sentry == -1)
                return false;
            Vector pos = GetBuildingPosition(ENTITY(sentry));
            if (shouldBacktrack())
            {
                // This does vischecks and everything
                auto data = hacks::tf2::backtrack::getClosestEntTick(entity, LOCAL_E->m_vecOrigin(), aimbotTickFilter);
                // No data found
                if (!data)
                    return false;
            }
            else if (!IsVectorVisible(pos, entity->m_vecOrigin(), false))
                return false;
        }
        if (fov > 0.0f && cd.fov > fov)
            return false;

        return true;
    }
    else
    {
        // Target not valid
        return false;
    }
}

// A function to aim at a specific entitiy
void Aim(CachedEntity *entity)
{
    if (*miss_chance > 0 && UniformRandomInt(0, 99) < *miss_chance)
        return;

    // Dont aim at a bad entity
    if (CE_BAD(entity))
        return;

    // Get angles
    Vector angles = GetAimAtAngles(g_pLocalPlayer->v_Eye, PredictEntity(entity), LOCAL_E);

    // Multipoint
    if (multipoint && !projectile_mode)
    {
        // Get hitbox num
        AimbotCalculatedData_s &cd = calculated_data_array[entity->m_IDX];
        float minx, maxx, miny, maxy, minz, maxz, centerx, centery, centerz;
        auto hb           = entity->hitboxes.GetHitbox(cd.hitbox);
        auto hitboxmin    = hb->min;
        auto hitboxmax    = hb->max;
        auto hitboxcenter = hb->center;
        if (shouldBacktrack() && entity->m_Type() == ENTITY_PLAYER)
        {
            // This does vischecks and everything
            auto data    = hacks::tf2::backtrack::getClosestEntTick(entity, LOCAL_E->m_vecOrigin(), aimbotTickFilter);
            auto bt_hb   = data->hitboxes.at(cd.hitbox);
            hitboxcenter = bt_hb.center;
            hitboxmin    = bt_hb.min;
            hitboxmax    = bt_hb.max;
        }
        // get positions
        minx    = hitboxmin.x;
        miny    = hitboxmin.y;
        maxx    = hitboxmax.x;
        maxy    = hitboxmax.y;
        minz    = hitboxmin.z;
        maxz    = hitboxmax.z;
        centerx = hitboxcenter.x;
        centery = hitboxcenter.y;
        centerz = hitboxcenter.z;

        // Shrink positions
        minx += (maxx - minx) / 6;
        maxx -= (maxx - minx) / 6;
        maxy -= (maxy - miny) / 6;
        miny += (maxy - miny) / 6;
        maxz -= (maxz - minz) / 6;
        minz += (maxz - minz) / 6;
        // Create Vectors
        const Vector positions[13] = { { minx, centery, minz }, { maxx, centery, minz }, { minx, centery, maxz }, { maxx, centery, maxz }, { centerx, miny, minz }, { centerx, maxy, minz }, { centerx, miny, maxz }, { centerx, maxy, maxz }, { minx, miny, centerz }, { maxx, maxy, centerz }, { minx, miny, centerz }, { maxx, maxy, centerz }, hitboxcenter };
        for (int i = 0; i < 13; ++i)
            if (IsEntityVectorVisible(entity, positions[i]))
            {
                angles = GetAimAtAngles(g_pLocalPlayer->v_Eye, positions[i], LOCAL_E);
                break;
            }
    }

    // Slow aim
    if (slow_aim)
        DoSlowAim(angles);

    // Set angles
    current_user_cmd->viewangles = angles;

    if (silent && !slow_aim)
        g_pLocalPlayer->bUseSilentAngles = true;
    // Set tick count to targets (backtrack messes with this)
    if (!shouldBacktrack() && nolerp && entity->m_IDX <= g_IEngine->GetMaxClients())
        current_user_cmd->tick_count = TIME_TO_TICKS(CE_FLOAT(entity, netvar.m_flSimulationTime));
    // Set Backtrack data
    if (shouldBacktrack() && entity->m_Type() == ENTITY_PLAYER)
    {
        auto data = hacks::tf2::backtrack::getClosestEntTick(entity, LOCAL_E->m_vecOrigin(), aimbotTickFilter);
        if (data)
            hacks::tf2::backtrack::SetBacktrackData(entity, *data);
    }
    // Finish function
    return;
}

// A function to check whether player can autoshoot
bool begancharge = false;
int begansticky  = 0;
void DoAutoshoot(CachedEntity *target_entity)
{
    // Enable check
    if (!autoshoot)
        return;
    if (IsPlayerDisguised(g_pLocalPlayer->entity) && !autoshoot_disguised)
        return;
    // Handle Compound bow
    if (g_pLocalPlayer->weapon()->m_iClassID() == CL_CLASS(CTFCompoundBow))
    {
        // Release hunstman if over huntsmans limit
        if (begancharge)
        {
            current_user_cmd->buttons &= ~IN_ATTACK;
            hacks::shared::antiaim::SetSafeSpace(5);
            begancharge = false;
            // Pull string if charge isnt enough
        }
        else
        {
            current_user_cmd->buttons |= IN_ATTACK;
            begancharge = true;
        }
        return;
    }
    else
        begancharge = false;
    if (g_pLocalPlayer->weapon()->m_iClassID() == CL_CLASS(CTFPipebombLauncher))
    {
        float chargebegin = *((float *) ((unsigned) RAW_ENT(LOCAL_W) + 3152));
        float chargetime  = g_GlobalVars->curtime - chargebegin;

        // Release Sticky if > chargetime, 3.85 is the max second chargetime,
        // but we want a percent so here we go
        if ((chargetime >= 3.85f * *sticky_autoshoot) && begansticky > 3)
        {
            current_user_cmd->buttons &= ~IN_ATTACK;
            hacks::shared::antiaim::SetSafeSpace(5);
            begansticky = 0;
        }
        // Else just keep charging
        else
        {
            current_user_cmd->buttons |= IN_ATTACK;
            begansticky++;
        }
        return;
    }
    else
        begansticky = 0;
    bool attack = true;

    // Rifle check
    IF_GAME(IsTF())
    {
        if (g_pLocalPlayer->clazz == tf_class::tf_sniper)
        {
            if (g_pLocalPlayer->holding_sniper_rifle)
            {
                if (zoomed_only && !CanHeadshot())
                    attack = false;
            }
        }
    }

    // Ambassador check
    IF_GAME(IsTF2())
    {
        if (IsAmbassador(g_pLocalPlayer->weapon()))
        {
            // Check if ambasador can headshot
            if (!AmbassadorCanHeadshot() && wait_for_charge)
                attack = false;
        }
    }

    // Autoshoot breaks with Slow aimbot, so use a workaround to detect when it
    // can
    if (slow_aim && !slow_can_shoot)
        attack = false;

    // Dont autoshoot without anything in clip
    if (CE_INT(g_pLocalPlayer->weapon(), netvar.m_iClip1) == 0)
        attack = false;

    if (attack)
    {
        // TO DO: Sending both reload and attack will activate the hitmans heatmaker ability
        // Don't activate it only on first kill (or somehow activate it before a shot)
        current_user_cmd->buttons |= IN_ATTACK | (*autoreload && CarryingHeatmaker() ? IN_RELOAD : 0);
        if (target_entity)
        {
            auto hitbox = calculated_data_array[target_entity->m_IDX].hitbox;
            hitrate::AimbotShot(target_entity->m_IDX, hitbox != head);
        }
    }
    if (LOCAL_W->m_iClassID() == CL_CLASS(CTFLaserPointer))
        current_user_cmd->buttons |= IN_ATTACK2;
}

// Grab a vector for a specific ent
const Vector &PredictEntity(CachedEntity *entity)
{
    // Pull out predicted data
    AimbotCalculatedData_s &cd = calculated_data_array[entity->m_IDX];
    Vector &result             = cd.aim_position;
    if (cd.predict_tick == tickcount)
        return result;

    if (!shouldBacktrack() || entity->m_Type() != ENTITY_PLAYER)
    {
        // Players
        if ((entity->m_Type() == ENTITY_PLAYER))
        {
            // If using projectiles, predict a vector
            if (projectileAimbotRequired)
            {
                // Use prediction engine if user settings allow
                if (engine_projpred)
                    result = ProjectilePrediction_Engine(entity, cd.hitbox, cur_proj_speed, cur_proj_grav, 0);
                else
                    result = ProjectilePrediction(entity, cd.hitbox, cur_proj_speed, cur_proj_grav, PlayerGravityMod(entity));
            }
            else
            {
                // If using extrapolation, then predict a vector
                if (extrapolate)
                    result = SimpleLatencyPrediction(entity, cd.hitbox);
                // else just grab strait from the hitbox
                else
                    GetHitbox(entity, cd.hitbox, result);
            }
            // Buildings
        }
        else if (entity->m_Type() == ENTITY_BUILDING || entity->m_iClassID() != CL_CLASS(CTFTankBoss))
        {
            if (cur_proj_grav || cur_proj_grav)
                result = BuildingPrediction(entity, GetBuildingPosition(entity), cur_proj_speed, cur_proj_grav);
            else
                result = GetBuildingPosition(entity);
            // Other
        }
        else
        {
            result = entity->m_vecOrigin();
        }

        cd.predict_tick = tickcount;

        cd.fov = GetFov(g_pLocalPlayer->v_OrigViewangles, g_pLocalPlayer->v_Eye, result);
    }
    else
    {
        auto data = hacks::tf2::backtrack::getClosestEntTick(entity, LOCAL_E->m_vecOrigin(), aimbotTickFilter);
        if (data)
        {
            result          = data->hitboxes.at(cd.hitbox).center;
            cd.predict_tick = tickcount;
            cd.fov          = GetFov(g_pLocalPlayer->v_OrigViewangles, g_pLocalPlayer->v_Eye, result);
        }
    }
    // Return the found vector
    return result;
}

// A function to find the best hitbox for a target
int BestHitbox(CachedEntity *target)
{
    // Switch based apon the hitbox mode set by the user
    switch (*hitbox_mode)
    {
    case 0:
    { // AUTO-HEAD priority
        int preferred = int(hitbox);
        bool headonly = false; // Var to keep if we can bodyshot

        IF_GAME(IsTF())
        {
            int ci    = g_pLocalPlayer->weapon()->m_iClassID();
            preferred = hitbox_t::spine_2;
            // Sniper rifle
            if (g_pLocalPlayer->holding_sniper_rifle)
            {
                headonly = CanHeadshot();
                // Hunstman
            }
            else if (ci == CL_CLASS(CTFCompoundBow))
            {
                float begincharge = CE_FLOAT(g_pLocalPlayer->weapon(), netvar.flChargeBeginTime);
                float charge      = g_GlobalVars->curtime - begincharge;
                int damage        = std::floor(50.0f + 70.0f * fminf(1.0f, charge));
                if (damage >= target->m_iHealth())
                    preferred = hitbox_t::spine_3;
                else
                    preferred = hitbox_t::head;
            }
            else if (IsAmbassador(g_pLocalPlayer->weapon()))
            {
                headonly = AmbassadorCanHeadshot();
                // 18 health is a good number to use as thats the usual minimum
                // damage it can do with a bodyshot, but damage could
                // potentially be higher
                if (target->m_iHealth() <= 18 || IsPlayerCritBoosted(g_pLocalPlayer->entity) || target->m_flDistance() > 1200)
                    headonly = false;
                // Rocket launcher
            }
            else if (ci == CL_CLASS(CTFRocketLauncher) || ci == CL_CLASS(CTFRocketLauncher_AirStrike) || ci == CL_CLASS(CTFRocketLauncher_DirectHit) || ci == CL_CLASS(CTFRocketLauncher_Mortar) || ci == CL_CLASS(CTFPipebombLauncher))
            {
                preferred = hitbox_t::hip_L;
            }
            // Airborn projectile
            if (GetWeaponMode() == weaponmode::weapon_projectile)
            {
                if (g_pLocalPlayer->weapon()->m_iClassID() != CL_CLASS(CTFCompoundBow))
                {
                    bool ground = CE_INT(target, netvar.iFlags) & (1 << 0);
                    if (!ground)
                        preferred = hitbox_t::spine_3;
                }
            }

            // Bodyshot handling
            if (g_pLocalPlayer->holding_sniper_rifle)
            {

                float cdmg = CE_FLOAT(LOCAL_W, netvar.flChargedDamage);
                float bdmg = 50;
                if (CarryingHeatmaker())
                {
                    bdmg = (bdmg * .80) - 1;
                    cdmg = (cdmg * .80) - 1;
                }
                // Darwins damage correction, protects against 15% of damage
                //                if (HasDarwins(target))
                //                {
                //                    bdmg = (bdmg * .85) - 1;
                //                    cdmg = (cdmg * .85) - 1;
                //                }
                // Vaccinator damage correction, protects against 75% of damage
                if (HasCondition<TFCond_UberBulletResist>(target))
                {
                    bdmg = (bdmg * .25) - 1;
                    cdmg = (cdmg * .25) - 1;
                    // Passive bullet resist protects against 10% of damage
                }
                else if (HasCondition<TFCond_SmallBulletResist>(target))
                {
                    bdmg = (bdmg * .90) - 1;
                    cdmg = (cdmg * .90) - 1;
                }
                // Invis damage correction, Invis spies get protection from 10%
                // of damage
                if (IsPlayerInvisible(target))
                {
                    bdmg = (bdmg * .80) - 1;
                    cdmg = (cdmg * .80) - 1;
                }
                // If can headshot and if bodyshot kill from charge damage, or
                // if crit boosted and they have 150 health, or if player isnt
                // zoomed, or if the enemy has less than 40, due to darwins, and
                // only if they have less than 150 health will it try to
                // bodyshot
                if (CanHeadshot() && (std::floor(cdmg) >= target->m_iHealth() || IsPlayerCritBoosted(g_pLocalPlayer->entity) || !g_pLocalPlayer->bZoomed || target->m_iHealth() <= std::floor(bdmg)) && target->m_iHealth() <= 150)
                {
                    // We dont need to hit the head as a bodyshot will kill
                    preferred = hitbox_t::spine_1;
                    headonly  = false;
                }
            }
            // In counter-strike source, headshots are what we want
        }
        else IF_GAME(IsCSS())
        {
            headonly = true;
        }
        // Head only
        if (headonly)
        {
            IF_GAME(IsTF())
            return hitbox_t::head;
            IF_GAME(IsCSS())
            return 12;
        }

        // Backtracking and preferred hitbox
        if (shouldBacktrack())
        {
            auto data = hacks::tf2::backtrack::getClosestEntTick(target, LOCAL_E->m_vecOrigin(), aimbotTickFilter);

            if (data)
            {
                // First check preferred hitbox
                if (IsEntityVectorVisible(target, (*data).hitboxes[preferred].center))
                    return preferred;

                // Then check the rest
                if (*backtrackVischeckAll)
                    for (int j = head; j < foot_R + 1; j++)
                    {
                        if (IsEntityVectorVisible(target, (*data).hitboxes[j].center))
                            return j;
                    }
                else if (IsEntityVectorVisible(target, (*data).hitboxes.at(head).center))
                    return 0;
            }
            // Nothing found, falling through to further below
        }
        else if (target->hitboxes.VisibilityCheck(preferred))
            return preferred;
        // Else attempt to find any hitbox at all
        if (!shouldBacktrack())
            for (int i = projectile_mode ? 1 : 0; i < target->hitboxes.GetNumHitboxes() && i < 6; i++)
            {
                if (target->hitboxes.VisibilityCheck(i))
                    return i;
            }
    }
    break;
    case 1:
    { // AUTO-CLOSEST priority, Return closest hitbox to crosshair
        return ClosestHitbox(target);
    }
    break;
    case 2:
    { // STATIC priority, Return a user chosen hitbox
        return *hitbox;
    }
    break;
    default:
        break;
    }
    // Hitbox machine :b:roke
    return -1;
}

// Function to find the closesnt hitbox to the crosshair for a given ent
int ClosestHitbox(CachedEntity *target)
{
    // FIXME this will break multithreading if it will be ever implemented. When
    // implementing it, these should be made non-static
    int closest;
    float closest_fov, fov = 0.0f;

    closest     = -1;
    closest_fov = 256;
    for (int i = 0; i < target->hitboxes.GetNumHitboxes(); i++)
    {
        fov = GetFov(g_pLocalPlayer->v_OrigViewangles, g_pLocalPlayer->v_Eye, target->hitboxes.GetHitbox(i)->center);
        if (fov < closest_fov || closest == -1)
        {
            closest     = i;
            closest_fov = fov;
        }
    }
    return closest;
}

// Function to get predicted visual checks
bool VischeckPredictedEntity(CachedEntity *entity)
{
    // Retrieve predicted data
    AimbotCalculatedData_s &cd = calculated_data_array[entity->m_IDX];
    if (cd.vcheck_tick == tickcount)
        return cd.visible;
    if (!shouldBacktrack() || entity->m_Type() != ENTITY_PLAYER)
    {
        // Update info
        cd.vcheck_tick = tickcount;
        if (extrapolate || projectileAimbotRequired || entity->m_Type() != ENTITY_PLAYER)
            cd.visible = IsEntityVectorVisible(entity, PredictEntity(entity));
        else
        {
            trace_t trace;
            cd.visible = IsEntityVectorVisible(entity, PredictEntity(entity), MASK_SHOT, &trace);
            if (cd.visible && cd.hitbox == head && trace.hitbox != head)
                cd.visible = false;
        }
    }
    else
    {
        auto data = hacks::tf2::backtrack::getClosestEntTick(entity, LOCAL_E->m_vecOrigin(), aimbotTickFilter);
        if (data && IsEntityVectorVisible(entity, data->hitboxes.at((cd.hitbox == -1 || cd.hitbox >= 18) ? 0 : cd.hitbox).center, MASK_SHOT))
            cd.visible = true;
        else
            cd.visible = false;
    }
    return cd.visible;
}

// A helper function to find a user angle that isnt directly on the target
// angle, effectively slowing the aiming process
void DoSlowAim(Vector &input_angle)
{
    LookAtPathTimer.update();
    slow_can_shoot  = false;
    auto old_input  = input_angle;
    auto viewangles = current_user_cmd->viewangles;

    // Get difference
    float delta_x = input_angle.x - viewangles.x;
    float delta_y = input_angle.y - viewangles.y;

    // Clamp
    while (delta_x > 89)
        delta_x -= 180;
    while (delta_x < -89)
        delta_x += 180;
    while (delta_y > 180)
        delta_y -= 360;
    while (delta_y < -180)
        delta_y += 360;

    // Determine which direction to steer in
    bool flip_pitch = delta_x < 0.0f;
    bool flip_yaw   = delta_y < 0.0f;

    // Set the step distance based on slow_aim variable
    float step_amount = 40.0f / std::max(1, slow_aim);

    if (input_angle.x != viewangles.x)
    {
        float new_pitch;
        // Check if closer than step amount, in that case slow down until it's slow enough or we hit the bottom cap
        if (fabsf(delta_x) <= step_amount)
        {
            float pitch_amount = step_amount;
            // Smoothen attempts scale anti-proportional to slow_aim aswell
            for (int i = 0; i <= 5.0f / slow_aim; i++)
            {
                pitch_amount /= 2.0f;
                if (fabsf(delta_x) <= pitch_amount)
                    break;
            }
            // Pitch still too high
            if (fabsf(delta_x) <= pitch_amount)
                new_pitch = input_angle.x;
            else
                new_pitch = viewangles.x + (flip_pitch ? -1.0f : 1.0f) * pitch_amount;
        }
        else
            new_pitch = viewangles.x + (flip_pitch ? -1.0f : 1.0f) * step_amount;

        input_angle.x = new_pitch;
    }

    if (input_angle.y != viewangles.y)
    {
        float new_yaw;
        // Check if closer than step amount, in that case slow down until it's slow enough or we hit the bottom cap
        if (fabsf(delta_y) <= step_amount)
        {
            float yaw_amount = step_amount;
            // Smoothen attempts scale anti-proportional to slow_aim aswell
            for (int i = 0; i <= 5.0f / slow_aim; i++)
            {
                yaw_amount /= 2.0f;
                if (fabsf(delta_y) <= yaw_amount)
                    break;
            }
            // Yaw still too high
            if (fabsf(delta_y) <= yaw_amount)
                new_yaw = input_angle.y;
            else
                new_yaw = viewangles.y + (flip_yaw ? -1.0f : 1.0f) * yaw_amount;
        }
        else
            new_yaw = viewangles.y + (flip_yaw ? -1.0f : 1.0f) * step_amount;

        input_angle.y = new_yaw;
    }
    fClampAngle(input_angle);
    if (input_angle.x == old_input.x && input_angle.y == old_input.y)
        slow_can_shoot = true;
}

// A function that determins whether aimkey allows aiming
bool UpdateAimkey()
{
    static bool aimkey_flip       = false;
    static bool pressed_last_tick = false;
    bool allow_aimkey             = true;

    // Check if aimkey is used
    if (aimkey && aimkey_mode)
    {
        // Grab whether the aimkey is depressed
        bool key_down = aimkey.isKeyDown();
        switch ((int) aimkey_mode)
        {
        case 1: // Only while key is depressed
            if (!key_down)
                allow_aimkey = false;
            break;
        case 2: // Only while key is not depressed, enable
            if (key_down)
                allow_aimkey = false;
            break;
        case 3: // Aimkey acts like a toggle switch
            if (!pressed_last_tick && key_down)
                aimkey_flip = !aimkey_flip;
            if (!aimkey_flip)
                allow_aimkey = false;
            break;
        default:
            break;
        }
        pressed_last_tick = key_down;
    }
    // Return whether the aimkey allows aiming
    return allow_aimkey;
}

// Func to find value of how far to target ents
float EffectiveTargetingRange()
{
    if (GetWeaponMode() == weapon_melee)
        return (float) re::C_TFWeaponBaseMelee::GetSwingRange(RAW_ENT(LOCAL_W));
    if (g_pLocalPlayer->weapon()->m_iClassID() == CL_CLASS(CTFFlameThrower))
        return 310.0f; // Pyros only have so much until their flames hit
    if (g_pLocalPlayer->weapon()->m_iClassID() == CL_CLASS(CTFWeaponFlameBall))
        return 512.0f; // Dragons Fury is fast but short range

    return (float) max_range;
}

// A function used by gui elements to determine the current target
CachedEntity *CurrentTarget()
{
    if (foundTarget)
        return target; // Doesnt work for some reason

    return nullptr;
}

// Used for when you join and leave maps to reset aimbot vars
void Reset()
{
    target_last     = nullptr;
    projectile_mode = false;
}

#if ENABLE_VISUALS
static void DrawText()
{
    // Dont draw to screen when aimbot is disabled
    if (!enable)
        return;

    // Fov ring to represent when a target will be shot
    if (fov_draw)
    {
        // It cant use fovs greater than 180, so we check for that
        if (float(fov) > 0.0f && float(fov) < 180)
        {
            // Dont show ring while player is dead
            if (CE_GOOD(LOCAL_E) && LOCAL_E->m_bAlivePlayer())
            {
                rgba_t color = GUIColor();
                color.a      = float(fovcircle_opacity);

                int width, height;
                g_IEngine->GetScreenSize(width, height);

                // Math
                float mon_fov  = (float(width) / float(height) / (4.0f / 3.0f));
                float fov_real = RAD2DEG(2 * atanf(mon_fov * tanf(DEG2RAD(draw::fov / 2))));
                float radius   = tan(DEG2RAD(float(fov)) / 2) / tan(DEG2RAD(fov_real) / 2) * (width);

                draw::Circle(width / 2, height / 2, radius, color, 1, 100);
            }
        }
    }
    // Debug stuff
    if (!aimbot_debug)
        return;
    for (int i = 1; i < PLAYER_ARRAY_SIZE; i++)
    {
        CachedEntity *ent = ENTITY(i);
        if (CE_GOOD(ent))
        {
            Vector screen;
            Vector oscreen;
            if (draw::WorldToScreen(calculated_data_array[i].aim_position, screen) && draw::WorldToScreen(ent->m_vecOrigin(), oscreen))
            {
                draw::Rectangle(screen.x - 2, screen.y - 2, 4, 4, colors::white);
                draw::Line(oscreen.x, oscreen.y, screen.x - oscreen.x, screen.y - oscreen.y, colors::EntityF(ent), 0.5f);
            }
        }
    }
}
#endif
void rvarCallback(settings::VariableBase<float> &, float after)
{
    force_backtrack_aimbot = after >= 200.0f;
}
static InitRoutine EC([]() {
    hacks::tf2::backtrack::latency.installChangeCallback(rvarCallback);
    EC::Register(EC::LevelInit, Reset, "INIT_Aimbot", EC::average);
    EC::Register(EC::LevelShutdown, Reset, "RESET_Aimbot", EC::average);
    EC::Register(EC::CreateMove, CreateMove, "CM_Aimbot", EC::late);
#if ENABLE_VISUALS
    EC::Register(EC::Draw, DrawText, "DRAW_Aimbot", EC::average);
#endif
});

} // namespace hacks::shared::aimbot
