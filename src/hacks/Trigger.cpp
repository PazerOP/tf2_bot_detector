
/*
 * HTrigger.cpp
 *
 *  Created on: Oct 5, 2016
 *      Author: nullifiedcat
 */

#include <hacks/Trigger.hpp>
#include "common.hpp"
#include <hacks/Backtrack.hpp>
#include <PlayerTools.hpp>
#include <settings/Bool.hpp>
#include "Backtrack.hpp"

namespace hacks::shared::triggerbot
{
static settings::Boolean enable{ "trigger.enable", "false" };
static settings::Int hitbox_mode{ "trigger.hitbox-mode", "0" };
static settings::Int accuracy{ "trigger.accuracy", "1" };
static settings::Boolean wait_for_charge{ "trigger.wait-for-charge", "false" };
static settings::Boolean zoomed_only{ "trigger.zoomed-only", "true" };
static settings::Float delay{ "trigger.delay", "0" };

static settings::Button trigger_key{ "trigger.key.button", "<null>" };
static settings::Int trigger_key_mode{ "trigger.key.mode", "1" };
// FIXME move these into targeting
static settings::Boolean ignore_cloak{ "trigger.target.ignore-cloaked-spies", "true" };
static settings::Boolean ignore_vaccinator{ "trigger.target.ignore-vaccinator", "true" };
static settings::Boolean buildings_sentry{ "trigger.target.buildings-sentry", "true" };
static settings::Boolean buildings_other{ "trigger.target.buildings-other", "true" };
static settings::Boolean stickybot{ "trigger.target.stickybombs", "false" };
static settings::Boolean teammates{ "trigger.target.teammates", "false" };
static settings::Int max_range{ "trigger.target.max-range", "4096" };

// Vars for usersettings

float target_time = 0.0f;

int last_hb_traced = 0;
Vector forward;

// Filters for backtrack
bool tick_filter(CachedEntity *entity, hacks::tf2::backtrack::BacktrackData tick)
{
    // Check if it intersects any hitbox
    int num_hitboxes = 18;

    // Only need head hitbox
    if (HeadPreferable(entity))
        num_hitboxes = 1;
    for (int i = 0; i < num_hitboxes; i++)
    {
        auto hitbox = tick.hitboxes.at(i);
        auto min    = hitbox.min;
        auto max    = hitbox.max;
        // Get the min and max for the hitbox
        Vector minz(fminf(min.x, max.x), fminf(min.y, max.y), fminf(min.z, max.z));
        Vector maxz(fmaxf(min.x, max.x), fmaxf(min.y, max.y), fmaxf(min.z, max.z));

        // Shrink the hitbox here
        Vector size = maxz - minz;
        // Use entire hitbox if accuracy unset
        Vector smod = size * 0.05f * (*accuracy > 0 ? *accuracy : 20);

        // Save the changes to the vectors
        minz += smod;
        maxz -= smod;

        // Trace and test if it hits the smaller hitbox, if it hits we can return true
        Vector hit;
        // Found a good one
        if (CheckLineBox(minz, maxz, g_pLocalPlayer->v_Eye, forward, hit))
        {
            // Is tick visible
            if (IsVectorVisible(g_pLocalPlayer->v_Eye, hitbox.center))
                return true;
        }
    }
    return false;
}

// The main function of the triggerbot
void CreateMove()
{
    float backup_time = target_time;
    target_time       = 0;

    // Check if trigerbot is enabled, weapon is valid and if player can aim
    if (!enable || CE_BAD(LOCAL_W) || !ShouldShoot())
        return;

    // Reset our last hitbox traced
    last_hb_traced = -1;

    // Get an ent in front of the player
    CachedEntity *ent = nullptr;
    std::optional<hacks::tf2::backtrack::BacktrackData> bt_data;

    if (!hacks::tf2::backtrack::isBacktrackEnabled)
        ent = FindEntInSight(EffectiveTargetingRange());
    // Backtrack, use custom filter to check if tick is in crosshair
    else
    {
        // Set up forward Vector
        forward = GetForwardVector(EffectiveTargetingRange(), LOCAL_E);

        // Call closest tick with our Tick filter func
        auto closest_data = hacks::tf2::backtrack::getClosestTick(g_pLocalPlayer->v_Eye, hacks::tf2::backtrack::defaultEntFilter, tick_filter);

        // No results, try to grab a building
        if (!closest_data)
        {
            ent = FindEntInSight(EffectiveTargetingRange(), true);
        }
        else
        {
            // Assign entity
            ent     = (*closest_data).first;
            bt_data = (*closest_data).second;
            hacks::tf2::backtrack::SetBacktrackData(ent, *bt_data);
        }
    }

    // Check if dormant or null to prevent crashes
    if (CE_BAD(ent))
        return;

    // Determine whether the triggerbot should shoot, then act accordingly
    if (IsTargetStateGood(ent, bt_data ? &*bt_data : nullptr))
    {
        target_time = backup_time;
        if (delay)
        {
            if (target_time > g_GlobalVars->curtime)
            {
                target_time = 0.0f;
            }
            if (!target_time)
            {
                target_time = g_GlobalVars->curtime;
            }
            else
            {
                if (g_GlobalVars->curtime - float(delay) >= target_time)
                {
                    current_user_cmd->buttons |= IN_ATTACK;
                }
            }
        }
        else
        {
            current_user_cmd->buttons |= IN_ATTACK;
        }
    }
}

// The first check to see if the player should shoot in the first place
bool ShouldShoot()
{

    // Check for +use
    if (current_user_cmd->buttons & IN_USE)
        return false;

    // Check if using action slot item
    if (g_pLocalPlayer->using_action_slot_item)
        return false;

    // Check the aimkey to determine if it should shoot
    if (!UpdateAimkey())
        return false;

    IF_GAME(IsTF2())
    {
        // Check if Carrying A building
        if (CE_BYTE(g_pLocalPlayer->entity, netvar.m_bCarryingObject))
            return false;
        // Check if deadringer out
        if (CE_BYTE(g_pLocalPlayer->entity, netvar.m_bFeignDeathReady))
            return false;
        // If zoomed only is on, check if zoomed
        if (zoomed_only && g_pLocalPlayer->holding_sniper_rifle)
        {
            if (!g_pLocalPlayer->bZoomed && !(current_user_cmd->buttons & IN_ATTACK))
                return false;
        }
        // Check if player is taunting
        if (HasCondition<TFCond_Taunting>(g_pLocalPlayer->entity))
            return false;
        // Check if player is cloaked
        if (IsPlayerInvisible(g_pLocalPlayer->entity))
            return false;

        if (IsAmbassador(g_pLocalPlayer->weapon()))
        {
            // Check if ambasador can headshot
            if (!AmbassadorCanHeadshot())
                return false;
        }
    }

    IF_GAME(IsTF2())
    {
        switch (GetWeaponMode())
        {
        case weapon_hitscan:
            break;
        case weapon_melee:
            break;
        // Check if player is using a projectile based weapon
        case weapon_projectile:
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

// A second check to determine whether a target is good enough to be aimed at
bool IsTargetStateGood(CachedEntity *entity, hacks::tf2::backtrack::BacktrackData *tick)
{
    // Check for Players
    if (entity->m_Type() == ENTITY_PLAYER)
    {
        // Check if target is The local player
        if (entity == LOCAL_E)
            return false;
        // Dont aim at dead player
        if (!entity->m_bAlivePlayer())
            return false;
        // Dont aim at teammates
        if (!entity->m_bEnemy() && !teammates)
            return false;

        // Global checks
        if (!player_tools::shouldTarget(entity))
            return false;

        IF_GAME(IsTF())
        {
            // If settings allow waiting for charge, and current charge cant
            // kill target, dont aim
            if (*wait_for_charge && g_pLocalPlayer->holding_sniper_rifle)
            {
                float bdmg = CE_FLOAT(g_pLocalPlayer->weapon(), netvar.flChargedDamage);
                if (g_GlobalVars->curtime - g_pLocalPlayer->flZoomBegin <= 1.0f)
                    bdmg = 50.0f;
                //                if ((bdmg * 3) < (HasDarwins(entity)
                //                                      ? (entity->m_iHealth() *
                //                                      1.15)
                //                                      : entity->m_iHealth()))
                if (bdmg * 3 < entity->m_iHealth())
                {
                    return false;
                }
            }
            // Dont target invulnerable players, ex: uber, bonk
            if (IsPlayerInvulnerable(entity))
                return false;
            // If settings allow, dont target cloaked players
            if (ignore_cloak && IsPlayerInvisible(entity))
                return false;
            // If settings allow, dont target vaccinated players
            if (ignore_vaccinator && IsPlayerResistantToCurrentWeapon(entity))
                return false;
        }

        // Backtrack did these in the tick check
        if (!tick)
        {
            // Head hitbox detection
            if (HeadPreferable(entity))
            {
                if (last_hb_traced != hitbox_t::head)
                    return false;
            }

            // If usersettings tell us to use accuracy improvements and the cached
            // hitbox isnt null, then we check if it hits here
            if (*accuracy)
            {

                // Get a cached hitbox for the one traced
                hitbox_cache::CachedHitbox *hb = entity->hitboxes.GetHitbox(last_hb_traced);
                // Check for null
                if (hb)
                {
                    // Get the min and max for the hitbox
                    Vector minz(fminf(hb->min.x, hb->max.x), fminf(hb->min.y, hb->max.y), fminf(hb->min.z, hb->max.z));
                    Vector maxz(fmaxf(hb->min.x, hb->max.x), fmaxf(hb->min.y, hb->max.y), fmaxf(hb->min.z, hb->max.z));

                    // Shrink the hitbox here
                    Vector size = maxz - minz;
                    Vector smod = size * 0.05f * *accuracy;

                    // Save the changes to the vectors
                    minz += smod;
                    maxz -= smod;

                    // Trace and test if it hits the smaller hitbox, if it fails
                    // we
                    // return false
                    Vector hit;
                    if (!CheckLineBox(minz, maxz, g_pLocalPlayer->v_Eye, forward, hit))
                        return false;
                }
            }
        }
        // Target passed the tests so return true
        return true;

        // Check for buildings
    }
    else if (entity->m_Type() == ENTITY_BUILDING)
    {
        // Check if building aimbot is enabled
        if (!(buildings_other || buildings_sentry))
            return false;
        // Check if enemy building
        if (!entity->m_bEnemy())
            return false;

        // If needed, Check if building type is allowed
        if (!(buildings_other && buildings_sentry))
        {
            // Check if target is a sentrygun
            if (entity->m_iClassID() == CL_CLASS(CObjectSentrygun))
            {
                // If sentrys are not allowed, dont target
                if (!buildings_sentry)
                    return false;
            }
            else
            {
                // If target is not a sentry, check if other buildings are
                // allowed
                if (!buildings_other)
                    return false;
            }
        }

        // Target passed the tests so return true
        return true;

        // Check for stickybombs
    }
    else if (entity->m_iClassID() == CL_CLASS(CTFGrenadePipebombProjectile))
    {
        // Check if sticky aimbot is enabled
        if (!stickybot)
            return false;

        // Check if thrower is a teammate
        if (!entity->m_bEnemy())
            return false;

        // Check if target is a pipe bomb
        if (CE_INT(entity, netvar.iPipeType) != 1)
            return false;

        // Target passed the tests so return true
        return true;
    }
    else
    {
        // If target is not player, building or sticky, return false
        return false;
    }
    // An impossible error so just return false
    return false;
}

// A function to return a potential entity in front of the player
CachedEntity *FindEntInSight(float range, bool no_players)
{
    // We dont want to hit ourself so we set an ignore
    trace_t trace;
    trace::filter_default.SetSelf(RAW_ENT(g_pLocalPlayer->entity));

    // Get Forward vector
    forward = GetForwardVector(range, LOCAL_E);

    // Setup the trace starting with the origin of the local players eyes
    // attemting to hit the end vector we determined
    Ray_t ray;
    ray.Init(g_pLocalPlayer->v_Eye, forward);

    // Ray trace
    g_ITrace->TraceRay(ray, 0x4200400B, &trace::filter_default, &trace);

    // Return an ent if that is what we hit
    if (trace.m_pEnt)
    {
        last_hb_traced    = trace.hitbox;
        CachedEntity *ent = ENTITY(((IClientEntity *) trace.m_pEnt)->entindex());
        // Player check
        if (!no_players || ent->m_Type() != ENTITY_PLAYER)
            return ent;
    }

    // Since we didnt hit and entity, the vis check failed so return 0
    return nullptr;
}

// A function to find whether the head should be used for a target
bool HeadPreferable(CachedEntity *target)
{

    // Switch based on the priority type we need
    switch (*hitbox_mode)
    {
    case 0:
    { // AUTO-HEAD priority
        // Var to keep if we can bodyshot
        bool headonly = false;
        // Save the local players current weapon to a var
        int ci = g_pLocalPlayer->weapon()->m_iClassID();
        IF_GAME(IsTF())
        {
            // If user is using a sniper rifle, Set headonly to whether we can
            // headshot or not,
            if (g_pLocalPlayer->holding_sniper_rifle)
            {
                headonly = CanHeadshot();
                // If player is using an ambassador, set headonly to true
            }
            else if (IsAmbassador(g_pLocalPlayer->weapon()))
            {
                headonly = true;
            }
            // Bodyshot handling
            if (g_pLocalPlayer->holding_sniper_rifle)
            {
                // Some keeper vars
                float cdmg, bdmg;
                // Grab netvar for current charge damage
                cdmg = CE_FLOAT(LOCAL_W, netvar.flChargedDamage);
                // Set our baseline bodyshot damage
                bdmg = 50;
                // Darwins damage correction
                //                if (HasDarwins(target))
                //                {
                // Darwins protects against 15% of damage
                //                    bdmg = (bdmg * .85) - 1;
                //                    cdmg = (cdmg * .85) - 1;
                //                }
                // Vaccinator damage correction
                if (HasCondition<TFCond_UberBulletResist>(target))
                {
                    // Vac charge protects against 75% of damage
                    bdmg = (bdmg * .25) - 1;
                    cdmg = (cdmg * .25) - 1;
                }
                else if (HasCondition<TFCond_SmallBulletResist>(target))
                {
                    // Passive bullet resist protects against 10% of damage
                    bdmg = (bdmg * .90) - 1;
                    cdmg = (cdmg * .90) - 1;
                }
                // Invis damage correction
                if (IsPlayerInvisible(target))
                {
                    // Invis spies get protection from 10% of damage
                    bdmg = (bdmg * .80) - 1;
                    cdmg = (cdmg * .80) - 1;
                }
                // If can headshot and if bodyshot kill from charge damage, or
                // if crit boosted and they have 150 health, or if player isnt
                // zoomed, or if the enemy has less than 40, due to darwins, and
                // only if they have less than 150 health will it try to
                // bodyshot
                if (CanHeadshot() && (cdmg >= target->m_iHealth() || IsPlayerCritBoosted(g_pLocalPlayer->entity) || !g_pLocalPlayer->bZoomed || target->m_iHealth() <= bdmg) && target->m_iHealth() <= 150)
                {
                    // We dont need to hit the head as a bodyshot will kill
                    headonly = false;
                }
            }
            // In counter-strike source, headshots are what we want
        }
        else IF_GAME(IsCSS())
        {
            headonly = true;
        }
        // Return our var of if we need to headshot
        return headonly;
    }
    break;
    case 1:
    { // AUTO-CLOSEST priority
        // We dont need the head so just use anything
        return false;
    }
    break;
    case 2:
    { // Head only
        // User wants the head only
        return true;
    }
    break;
    }
    // We dont know what the user wants so just use anything
    return false;
}

// A function that determins whether aimkey allows aiming
bool UpdateAimkey()
{
    static bool trigger_key_flip  = false;
    static bool pressed_last_tick = false;
    bool allow_trigger_key        = true;
    // Check if aimkey is used
    if (trigger_key && trigger_key_mode)
    {
        // Grab whether the aimkey is depressed
        bool key_down = trigger_key.isKeyDown();
        // Switch based on the user set aimkey mode
        switch ((int) trigger_key_mode)
        {
        // Only while key is depressed, enable
        case 1:
            if (!key_down)
                allow_trigger_key = false;
            break;
        // Only while key is not depressed, enable
        case 2:
            if (key_down)
                allow_trigger_key = false;
            break;
        // Aimkey acts like a toggle switch
        case 3:
            if (!pressed_last_tick && key_down)
                trigger_key_flip = !trigger_key_flip;
            if (!trigger_key_flip)
                allow_trigger_key = false;
        }
        pressed_last_tick = key_down;
    }
    // Return whether the aimkey allows aiming
    return allow_trigger_key;
}

// Func to find value of how far to target ents
float EffectiveTargetingRange()
{
    if (GetWeaponMode() == weapon_melee)
        return re::C_TFWeaponBaseMelee::GetSwingRange(RAW_ENT(LOCAL_W));
    // Pyros only have so much untill their flames hit
    else if (g_pLocalPlayer->weapon()->m_iClassID() == CL_CLASS(CTFFlameThrower))
        return 300.0f;
    // If user has set a max range, then use their setting,
    if (max_range)
        return *max_range;
    // else use a pre-set range
    else
        return 8012.0f;
}

// Helper functions to trace for hitboxes

// TEMPORARY CODE.
// TODO
bool GetIntersection(float fDst1, float fDst2, Vector P1, Vector P2, Vector &Hit)
{
    if ((fDst1 * fDst2) >= 0.0f)
        return false;
    if (fDst1 == fDst2)
        return false;
    Hit = P1 + (P2 - P1) * (-fDst1 / (fDst2 - fDst1));
    return true;
}

bool InBox(Vector Hit, Vector B1, Vector B2, int Axis)
{
    if (Axis == 1 && Hit.z > B1.z && Hit.z < B2.z && Hit.y > B1.y && Hit.y < B2.y)
        return true;
    if (Axis == 2 && Hit.z > B1.z && Hit.z < B2.z && Hit.x > B1.x && Hit.x < B2.x)
        return true;
    if (Axis == 3 && Hit.x > B1.x && Hit.x < B2.x && Hit.y > B1.y && Hit.y < B2.y)
        return true;
    return false;
}

bool CheckLineBox(Vector B1, Vector B2, Vector L1, Vector L2, Vector &Hit)
{
    if (L2.x < B1.x && L1.x < B1.x)
        return false;
    if (L2.x > B2.x && L1.x > B2.x)
        return false;
    if (L2.y < B1.y && L1.y < B1.y)
        return false;
    if (L2.y > B2.y && L1.y > B2.y)
        return false;
    if (L2.z < B1.z && L1.z < B1.z)
        return false;
    if (L2.z > B2.z && L1.z > B2.z)
        return false;
    if (L1.x > B1.x && L1.x < B2.x && L1.y > B1.y && L1.y < B2.y && L1.z > B1.z && L1.z < B2.z)
    {
        Hit = L1;
        return true;
    }
    if ((GetIntersection(L1.x - B1.x, L2.x - B1.x, L1, L2, Hit) && InBox(Hit, B1, B2, 1)) || (GetIntersection(L1.y - B1.y, L2.y - B1.y, L1, L2, Hit) && InBox(Hit, B1, B2, 2)) || (GetIntersection(L1.z - B1.z, L2.z - B1.z, L1, L2, Hit) && InBox(Hit, B1, B2, 3)) || (GetIntersection(L1.x - B2.x, L2.x - B2.x, L1, L2, Hit) && InBox(Hit, B1, B2, 1)) || (GetIntersection(L1.y - B2.y, L2.y - B2.y, L1, L2, Hit) && InBox(Hit, B1, B2, 2)) || (GetIntersection(L1.z - B2.z, L2.z - B2.z, L1, L2, Hit) && InBox(Hit, B1, B2, 3)))
        return true;

    return false;
}

void Draw()
{
}

static InitRoutine EC([]() { EC::Register(EC::CreateMove, CreateMove, "triggerbot", EC::average); });
} // namespace hacks::shared::triggerbot
