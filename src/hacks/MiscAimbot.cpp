//
// Created by bencat07 on 28.09.18.
//

#include "common.hpp"
#include "settings/Bool.hpp"
#include "settings/Int.hpp"
#include "settings/Key.hpp"
#include "PlayerTools.hpp"
#include "hacks/Trigger.hpp"
#include "MiscAimbot.hpp"
#include "DetourHook.hpp"
#include "Backtrack.hpp"

namespace hacks::tf2::misc_aimbot
{
static settings::Boolean sandwichaim_enabled{ "sandwichaim.enable", "false" };
static settings::Button sandwichaim_aimkey{ "sandwichaim.aimkey", "<null>" };
static settings::Int sandwichaim_aimkey_mode{ "sandwichaim.aimkey-mode", "0" };

float sandwich_speed = 350.0f;
float grav           = 0.25f;
int prevent          = -1;
static Timer previous_entity_delay{};

// TODO: Refactor this jank
std::pair<CachedEntity *, Vector> FindBestEnt(bool teammate, bool Predict, bool zcheck, bool demoknight_mode, float range)
{
    CachedEntity *bestent = nullptr;
    float bestscr         = FLT_MAX;
    Vector predicted{};
    // Too long since we focused it
    if (previous_entity_delay.check(100))
        prevent = -1;
    for (int i = 0; i < 1; i++)
    {
        if (prevent != -1)
        {
            auto ent = ENTITY(prevent);
            if (CE_BAD(ent) || !ent->m_bAlivePlayer() || (teammate && ent->m_iTeam() != LOCAL_E->m_iTeam()) || ent == LOCAL_E)
                continue;
            if (!teammate && ent->m_iTeam() == LOCAL_E->m_iTeam())
                continue;
            if (!ent->hitboxes.GetHitbox(1))
                continue;
            if (!teammate && !player_tools::shouldTarget(ent))
                continue;
            Vector target{};
            if (Predict)
                target = ProjectilePrediction(ent, 1, sandwich_speed, grav, PlayerGravityMod(ent));
            else
                target = ent->hitboxes.GetHitbox(1)->center;
            if (!hacks::tf2::backtrack::isBacktrackEnabled && !IsEntityVectorVisible(ent, target))
                continue;
            if (zcheck && (ent->m_vecOrigin().z - LOCAL_E->m_vecOrigin().z) > 200.0f)
                continue;
            float scr = ent->m_flDistance();
            if (hacks::tf2::backtrack::isBacktrackEnabled && demoknight_mode)
            {
                auto data = hacks::tf2::backtrack::getClosestEntTick(ent, LOCAL_E->m_vecOrigin(), hacks::tf2::backtrack::defaultTickFilter);
                // No entity
                if (!data)
                    scr = FLT_MAX;
                else
                {
                    target = (*data).m_vecOrigin;
                    scr    = (*data).m_vecOrigin.DistTo(LOCAL_E->m_vecOrigin());
                }
            }
            // Demoknight
            if (demoknight_mode)
            {
                if (scr >= range)
                    continue;
                scr = GetFov(g_pLocalPlayer->v_OrigViewangles, g_pLocalPlayer->v_Eye, ent->m_vecOrigin());
                // Don't turn too harshly
                if (scr >= 90.0f)
                    continue;
            }
            if (g_pPlayerResource->GetClass(ent) == tf_medic)
                scr *= 0.5f;
            if (scr < bestscr)
            {
                bestent   = ent;
                predicted = target;
                bestscr   = scr;
                prevent   = ent->m_IDX;
            }
        }
        if (bestent && predicted.z)
        {
            previous_entity_delay.update();
            return { bestent, predicted };
        }
    }
    prevent = -1;
    for (int i = 0; i <= g_IEngine->GetMaxClients(); i++)
    {
        CachedEntity *ent = ENTITY(i);
        if (CE_BAD(ent) || !(ent->m_bAlivePlayer()) || (teammate && ent->m_iTeam() != LOCAL_E->m_iTeam()) || ent == LOCAL_E)
            continue;
        if (!teammate && ent->m_iTeam() == LOCAL_E->m_iTeam())
            continue;
        if (!ent->hitboxes.GetHitbox(1))
            continue;
        Vector target{};
        if (Predict)
            target = ProjectilePrediction(ent, 1, sandwich_speed, grav, PlayerGravityMod(ent));
        else
            target = ent->hitboxes.GetHitbox(1)->center;
        if (!hacks::tf2::backtrack::isBacktrackEnabled && !IsEntityVectorVisible(ent, target))
            continue;
        if (zcheck && (ent->m_vecOrigin().z - LOCAL_E->m_vecOrigin().z) > 200.0f)
            continue;
        float scr = ent->m_flDistance();
        if (hacks::tf2::backtrack::isBacktrackEnabled && demoknight_mode)
        {
            auto data = hacks::tf2::backtrack::getClosestEntTick(ent, LOCAL_E->m_vecOrigin(), hacks::tf2::backtrack::defaultTickFilter);
            // No entity
            if (!data)
                scr = FLT_MAX;
            else
            {
                target = (*data).m_vecOrigin;
                scr    = (*data).m_vecOrigin.DistTo(LOCAL_E->m_vecOrigin());
            }
        }
        // Demoknight
        if (demoknight_mode)
        {
            if (scr >= range)
                continue;
            scr = GetFov(g_pLocalPlayer->v_OrigViewangles, g_pLocalPlayer->v_Eye, ent->m_vecOrigin());
            // Don't turn too harshly
            if (scr >= 90.0f)
                continue;
        }
        if (g_pPlayerResource->GetClass(ent) == tf_medic)
            scr *= 0.5f;
        if (scr < bestscr)
        {
            bestent   = ent;
            predicted = target;
            bestscr   = scr;
            prevent   = ent->m_IDX;
        }
    }
    return { bestent, predicted };
}
static float slow_change_dist_y{};
static float slow_change_dist_p{};
void DoSlowAim(Vector &input_angle)
{
    auto viewangles = current_user_cmd->viewangles;

    // Yaw
    if (viewangles.y != input_angle.y)
    {
        float flChargeYawCap = re::CTFPlayerShared::CalculateChargeCap(re::CTFPlayerShared::GetPlayerShared(RAW_ENT(LOCAL_E)));
        flChargeYawCap *= 2.5f;

        // Check if input angle and user angle are on opposing sides of yaw so
        // we can correct for that
        bool slow_opposing = false;
        if ((input_angle.y < -90 && viewangles.y > 90) || (input_angle.y > 90 && viewangles.y < -90))
            slow_opposing = true;

        // Direction
        bool slow_dir = false;
        if (slow_opposing)
        {
            if (input_angle.y > 90 && viewangles.y < -90)
                slow_dir = true;
        }
        else if (viewangles.y > input_angle.y)
            slow_dir = true;

        // Speed, check if opposing. We dont get a new distance due to the
        // opposing sides making the distance spike, so just cheap out and reuse
        // our last one.
        if (!slow_opposing)
            slow_change_dist_y = fminf(std::abs(viewangles.y - input_angle.y), flChargeYawCap);

        // Move in the direction of the input angle
        if (slow_dir)
            input_angle.y = viewangles.y - slow_change_dist_y;
        else
            input_angle.y = viewangles.y + slow_change_dist_y;
    }

    // Pitch
    if (viewangles.x != input_angle.x)
    {
        // Get speed
        slow_change_dist_p = std::abs(viewangles.x - input_angle.x) / 5;

        // Move in the direction of the input angle
        if (viewangles.x > input_angle.x)
            input_angle.x = viewangles.x - slow_change_dist_p;
        else
            input_angle.x = viewangles.x + slow_change_dist_p;
    }

    // Clamp as we changed angles
    fClampAngle(input_angle);
}

static void SandwichAim()
{
    if (!*sandwichaim_enabled)
        return;
    if (CE_BAD(LOCAL_E) || !LOCAL_E->m_bAlivePlayer() || CE_BAD(LOCAL_W))
        return;
    if (sandwichaim_aimkey)
    {
        switch (*sandwichaim_aimkey_mode)
        {
        case 1:
            if (!sandwichaim_aimkey.isKeyDown())
                return;
            break;
        case 2:
            if (sandwichaim_aimkey.isKeyDown())
                return;
            break;
        default:
            break;
        }
    }
    if (LOCAL_W->m_iClassID() != CL_CLASS(CTFLunchBox))
        return;
    Vector Predict;
    CachedEntity *bestent = nullptr;
    std::pair<CachedEntity *, Vector> result{};
    result  = FindBestEnt(true, true, false, false);
    bestent = result.first;
    Predict = result.second;
    if (bestent)
    {
        Vector tr = Predict - g_pLocalPlayer->v_Eye;
        Vector angles;
        VectorAngles(tr, angles);
        // Clamping is important
        fClampAngle(angles);
        current_user_cmd->viewangles = angles;
        current_user_cmd->buttons |= IN_ATTACK2;
        g_pLocalPlayer->bUseSilentAngles = true;
    }
}

static settings::Boolean charge_aim{ "chargeaim.enable", "false" };
static settings::Button charge_key{ "chargeaim.key", "<null>" };

static void ChargeAimbot()
{
    if (!*charge_aim)
        return;
    if (charge_key && !charge_key.isKeyDown())
        return;
    if (CE_BAD(LOCAL_E) || !LOCAL_E->m_bAlivePlayer() || CE_BAD(LOCAL_W))
        return;
    if (!HasCondition<TFCond_Charging>(LOCAL_E))
        return;
    std::pair<CachedEntity *, Vector> result{};
    result                = FindBestEnt(false, false, true, true, 1000.0f);
    CachedEntity *bestent = result.first;
    if (bestent && result.second.IsValid())
    {
        auto angles = GetAimAtAngles(g_pLocalPlayer->v_Eye, result.second);
        angles.x    = current_user_cmd->viewangles.x;
        DoSlowAim(angles);
        current_user_cmd->viewangles = angles;
    }
}

static settings::Boolean autosapper_enabled("autosapper.enabled", "false");
static settings::Boolean autosapper_silent("autosapper.silent", "true");

static void SapperAimbot()
{
    if (!autosapper_enabled)
        return;
    if (CE_BAD(LOCAL_E) || CE_BAD(LOCAL_W) || !g_pLocalPlayer->holding_sapper)
        return;

    CachedEntity *target = nullptr;
    float distance       = FLT_MAX;

    for (int i = 0; i < entity_cache::max; i++)
    {
        CachedEntity *ent = ENTITY(i);
        if (CE_BAD(ent))
            continue;
        if (ent->m_Type() != ENTITY_BUILDING)
            continue;
        if (!ent->m_bEnemy())
            continue;
        if (CE_BYTE(ent, netvar.m_bHasSapper))
            continue;
        float new_distance = g_pLocalPlayer->v_Eye.DistTo(GetBuildingPosition(ent));
        if (distance <= new_distance)
            continue;

        distance = new_distance;
        target   = ent;
    }
    if (target)
    {
        float range    = re::C_TFWeaponBaseMelee::GetSwingRange(RAW_ENT(LOCAL_W));
        Vector angle   = GetAimAtAngles(g_pLocalPlayer->v_Eye, GetBuildingPosition(target));
        Vector forward = GetForwardVector(g_pLocalPlayer->v_Eye, angle, range, LOCAL_E);
        trace_t trace;
        if (IsEntityVectorVisible(target, forward, MASK_SHOT, &trace))
        {
            if (trace.DidHit() && (IClientEntity *) trace.m_pEnt == RAW_ENT(target))
            {
                current_user_cmd->viewangles = angle;
                if (autosapper_silent)
                    g_pLocalPlayer->bUseSilentAngles = true;
                current_user_cmd->buttons |= IN_ATTACK;
            }
        }
    }
}
// auto repair
static settings::Int autorepair_priority{ "autorepair.priority", "0" };
static settings::Boolean autorepair_enabled("autorepair.enabled", "false");
static settings::Boolean autorepair_silent("autorepair.silent", "true");
static settings::Boolean autorepair_repair_sentry("autorepair.sentry", "false");
static settings::Boolean autorepair_repair_dispenser("autorepair.dispenser", "false");
static settings::Boolean autorepair_repair_teleport("autorepair.teleport", "false");

// auto upgrade
static settings::Boolean autoupgrade_enabled("autoupgrade.enabled", "false");
static settings::Boolean autoupgrade_sentry("autoupgrade.sentry", "false");
static settings::Boolean autoupgrade_dispenser("autoupgrade.dispenser", "false");
static settings::Boolean autoupgrade_teleport("autoupgrade.teleport", "false");
static settings::Int autoupgrade_sentry_level("autoupgrade.sentry.level", "3");
static settings::Int autoupgrade_dispenser_level("autoupgrade.dispenser.level", "3");
static settings::Int autoupgrade_teleport_level("autoupgrade.teleport.level", "2");

bool ShouldHitBuilding(CachedEntity *ent)
{
    if (!autoupgrade_enabled && !autorepair_enabled)
        return false;
    // Current Metal
    int cur_ammo = CE_INT(LOCAL_E, netvar.m_iAmmo + 12);
    // Autorepair is on
    if (autorepair_enabled)
    {
        // Special Sentry logic
        if (ent->m_iClassID() == CL_CLASS(CObjectSentrygun))
        {
            // Current sentry ammo
            int sentry_ammo = CE_INT(ent, netvar.m_iAmmoShells);
            // Max Sentry ammo
            int max_ammo = 0;

            // Set Ammo depending on level
            switch (CE_INT(ent, netvar.iUpgradeLevel))
            {
            case 1:
                max_ammo = 150;
                break;
            case 2:
            case 3:
                max_ammo = 200;
            }

            // Sentry needs ammo
            if (sentry_ammo < max_ammo)
                return true;
        }
        // Buildings needs to be repaired
        if (cur_ammo && ent->m_iHealth() != ent->m_iMaxHealth())
            return true;
    }
    // Autoupgrade is on
    if (autoupgrade_enabled)
    {
        // Upgrade lvel
        int upgrade_level = CE_INT(ent, netvar.iUpgradeLevel);

        // Don't upgrade mini sentries
        if (CE_BYTE(ent, netvar.m_bMiniBuilding))
            return false;

        // Rvar to check
        int level = 0;

        // Pick The right rvar to check depending on building type
        switch (ent->m_iClassID())
        {

        case CL_CLASS(CObjectSentrygun):
            // Enabled check
            if (!autoupgrade_sentry)
                return false;
            level = *autoupgrade_sentry_level;
            break;

        case CL_CLASS(CObjectDispenser):
            // Enabled check
            if (!autoupgrade_dispenser)
                return false;
            level = *autoupgrade_dispenser_level;
            break;

        case CL_CLASS(CObjectTeleporter):
            // Enabled check
            if (!autoupgrade_teleport)
                return false;
            level = *autoupgrade_teleport_level;
            break;
        }

        // Can be upgraded
        if (upgrade_level < level && cur_ammo)
            return true;
    }
    return false;
}

CachedEntity *targetBuilding(bool priority)
{
    float wrench_range   = re::C_TFWeaponBaseMelee::GetSwingRange(RAW_ENT(LOCAL_W));
    CachedEntity *target = nullptr;
    float distance       = FLT_MAX;
    for (int i = 0; i < entity_cache::max; i++)
    {
        CachedEntity *ent = ENTITY(i);

        if (CE_BAD(ent))
            continue;

        // can't exactly repair Merasmus
        if (ent->m_Type() != ENTITY_BUILDING)
            continue;

        // yes, repair enemy buildings
        if (ent->m_bEnemy())
            continue;

        float new_distance = g_pLocalPlayer->v_Eye.DistTo(GetBuildingPosition(ent));

        // Further away than old one
        if (new_distance >= distance)
            continue;

        auto id = ent->m_iClassID();

        switch (id)
        {

        case CL_CLASS(CObjectSentrygun):
        {
            if (priority && *autorepair_priority != 1)
                continue;
            // Don't repair sentries
            if (!autorepair_repair_sentry && !autoupgrade_sentry)
                continue;
            break;
        }

        case CL_CLASS(CObjectDispenser):
        {
            if (priority && *autorepair_priority != 2)
                continue;
            // Repair Dispensers check
            if (!autorepair_repair_dispenser && !autoupgrade_dispenser)
                continue;
            break;
        }

        case CL_CLASS(CObjectTeleporter):
        {
            if (priority && *autorepair_priority != 3)
                continue;
            // Repair Teleporters check
            if (!autorepair_repair_teleport && !autoupgrade_teleport)
                continue;
            break;
        }

        default:
            continue;
        }

        float s_distance = ent->m_vecOrigin().DistTo(LOCAL_E->m_vecOrigin());

        if (!ShouldHitBuilding(ent))
            continue;

        if (s_distance > wrench_range)
            continue;

        target = ent;

        distance = new_distance;
    }
    return target;
};

static void BuildingAimbot()
{
    if (!autorepair_enabled && !autoupgrade_enabled)
        return;

    if (CE_BAD(LOCAL_E) || CE_BAD(LOCAL_W))
        return;

    CachedEntity *target = nullptr;
    // Metal
    int cur_ammo       = CE_INT(LOCAL_E, netvar.m_iAmmo + 12);
    float wrench_range = re::C_TFWeaponBaseMelee::GetSwingRange(RAW_ENT(LOCAL_W));
    // Center is further away than actual hit range for buildings, so add some more
    wrench_range += 50.f;

    if (cur_ammo == 0)
        return;

    if (g_pLocalPlayer->clazz != tf_engineer)
        return;

    if (GetWeaponMode() != weaponmode::weapon_melee)
        return;

    // Get priority buildings
    if (autorepair_priority)
        target = targetBuilding(true);
    // No building found
    if (!target)
        targetBuilding(false);

    // We have a target
    if (target)
    {
        Vector angle   = GetAimAtAngles(g_pLocalPlayer->v_Eye, GetBuildingPosition(target));
        Vector forward = GetForwardVector(g_pLocalPlayer->v_Eye, angle, wrench_range, LOCAL_E);

        trace_t trace;

        if (IsEntityVectorVisible(target, forward, MASK_SHOT, &trace))
        {
            if (trace.DidHit() && (IClientEntity *) trace.m_pEnt == RAW_ENT(target))
            {
                current_user_cmd->viewangles = angle;
                if (autorepair_silent)
                    g_pLocalPlayer->bUseSilentAngles = true;
                current_user_cmd->buttons |= IN_ATTACK;
            }
        }
    }
}

static void CreateMove()
{
    SandwichAim();
    ChargeAimbot();
    SapperAimbot();
    BuildingAimbot();
}

DetourHook CAM_CapYaw_detour;
typedef float (*CAM_CapYaw_t)(IInput *, float);
// Fix client side limit being applied weirdly, Note that most of this is taken from the source leak directly
float CAM_CapYaw_Hook(IInput *this_, float fVal)
{
    if (CE_INVALID(LOCAL_E))
        return fVal;

    if (HasCondition<TFCond_Charging>(LOCAL_E))
    {
        float flChargeYawCap = re::CTFPlayerShared::CalculateChargeCap(re::CTFPlayerShared::GetPlayerShared(RAW_ENT(LOCAL_E)));

        // Our only change
        flChargeYawCap *= 2.5f;

        if (fVal > flChargeYawCap)
            return flChargeYawCap;
        else if (fVal < -flChargeYawCap)
            return -flChargeYawCap;
    }

    return fVal;
}

#define foffset(p, i) ((unsigned char *) &p)[i]
static InitRoutine init([]() {
    EC::Register(EC::CreateMove, CreateMove, "cm_miscaimbot", EC::average);

    static auto signature = gSignatures.GetClientSignature("55 89 E5 53 83 EC 14 E8 ? ? ? ? 85 C0 74 ? 8D 98 ? ? ? ? C7 44 24 ? 11 00 00 00");

    CAM_CapYaw_detour.Init(signature, (void *) CAM_CapYaw_Hook);
    EC::Register(
        EC::Shutdown, []() { CAM_CapYaw_detour.Shutdown(); }, "chargeaim_shutdown");
});
} // namespace hacks::tf2::misc_aimbot
