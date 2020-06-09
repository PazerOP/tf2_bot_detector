/*
 * HEsp.cpp
 *
 *  Created on: Oct 6, 2016
 *      Author: nullifiedcat
 */

#include <hacks/ESP.hpp>
#include <PlayerTools.hpp>
#include <settings/Bool.hpp>
#include "common.hpp"
#include "soundcache.hpp"

namespace hacks::shared::esp
{
static settings::Boolean enable{ "esp.enable", "false" };
static settings::Int max_dist{ "esp.range", "4096" };

static settings::Int box_esp{ "esp.box.mode", "2" };
static settings::Int box_corner_size{ "esp.box.corner-size", "10" };
static settings::Boolean box_3d_player{ "esp.box.player-3d", "false" };
static settings::Boolean box_3d_building{ "esp.box.building-3d", "false" };

static settings::Int tracers{ "esp.tracers-mode", "0" };

static settings::Int emoji_esp{ "esp.emoji.mode", "0" };
static settings::Int emoji_esp_size{ "esp.emoji.size", "32" };
static settings::Boolean emoji_esp_scaling{ "esp.emoji.scaling", "true" };
static settings::Int emoji_min_size{ "esp.emoji.min-size", "20" };

static settings::Int healthbar{ "esp.health-bar", "3" };
static settings::Boolean draw_bones{ "esp.bones", "false" };
static settings::Boolean bones_color{ "esp.bones.color", "false" };
static settings::Int sightlines{ "esp.sightlines", "0" };
static settings::Int esp_text_position{ "esp.text-position", "0" };
static settings::Int esp_expand{ "esp.expand", "0" };
static settings::Boolean vischeck{ "esp.vischeck", "true" };
static settings::Boolean legit{ "esp.legit", "false" };

static settings::Boolean local_esp{ "esp.show.local", "true" };
static settings::Boolean buildings{ "esp.show.buildings", "true" };
static settings::Boolean teammates{ "esp.show.teammates", "true" };
static settings::Boolean tank{ "esp.show.tank", "true" };

static settings::Boolean show_weapon{ "esp.info.weapon", "false" };
static settings::Boolean show_distance{ "esp.info.distance", "true" };
static settings::Boolean show_health{ "esp.info.health", "true" };
static settings::Boolean show_name{ "esp.info.name", "true" };
static settings::Boolean show_class{ "esp.info.class", "true" };
static settings::Boolean show_conditions{ "esp.info.conditions", "true" };
static settings::Boolean show_ubercharge{ "esp.info.ubercharge", "true" };
static settings::Boolean show_bot_id{ "esp.info.bot-id", "true" };
static settings::Boolean powerup_esp{ "esp.info.powerup", "true" };

static settings::Boolean item_esp{ "esp.item.enable", "true" };
static settings::Boolean item_dropped_weapons{ "esp.item.weapons", "false" };
static settings::Boolean item_ammo_packs{ "esp.item.ammo", "false" };
static settings::Boolean item_health_packs{ "esp.item.health", "true" };
static settings::Boolean item_powerups{ "esp.item.powerup", "true" };
static settings::Boolean item_money{ "esp.item.money", "true" };
static settings::Boolean item_money_red{ "esp.item.money-red", "false" };
static settings::Boolean item_spellbooks{ "esp.item.spellbook", "true" };
// TF2C
static settings::Boolean item_weapon_spawners{ "esp.item.weapon-spawner", "true" };
static settings::Boolean item_adrenaline{ "esp.item.adrenaline", "true" };

static settings::Boolean proj_esp{ "esp.projectile.enable", "false" };
static settings::Int proj_rockets{ "esp.projectile.rockets", "1" };
static settings::Int proj_arrows{ "esp.projectile.arrows", "1" };
static settings::Int proj_pipes{ "esp.projectile.pipes", "1" };
static settings::Int proj_stickies{ "esp.projectile.stickies", "1" };
static settings::Boolean proj_enemy{ "esp.projectile.enemy-only", "true" };

static settings::Boolean entity_info{ "esp.debug.entity", "false" };
static settings::Boolean entity_model{ "esp.debug.model", "false" };
static settings::Boolean entity_id{ "esp.debug.id", "true" };

static settings::Boolean v9mode{ "esp.v952-mode", "false" };

// Unknown
std::mutex threadsafe_mutex;
// Storage array for keeping strings and other data
std::array<ESPData, 2048> data;
// Storage vars for entities that need to be re-drawn
std::vector<int> entities_need_repaint{};
std::mutex entities_need_repaint_mutex{};

// :b:one stuff needs to be up here as puting it in the header for sorting would
// be a pain.

// Vars to store what bones connect to what
const std::string bonenames_leg_r[]  = { "bip_foot_R", "bip_knee_R", "bip_hip_R" };
const std::string bonenames_leg_l[]  = { "bip_foot_L", "bip_knee_L", "bip_hip_L" };
const std::string bonenames_bottom[] = { "bip_hip_R", "bip_pelvis", "bip_hip_L" };
const std::string bonenames_spine[]  = { "bip_pelvis", "bip_spine_0", "bip_spine_1", "bip_spine_2", "bip_spine_3", "bip_neck", "bip_head" };
const std::string bonenames_arm_r[]  = { "bip_upperArm_R", "bip_lowerArm_R", "bip_hand_R" };
const std::string bonenames_arm_l[]  = { "bip_upperArm_L", "bip_lowerArm_L", "bip_hand_L" };
const std::string bonenames_up[]     = { "bip_upperArm_R", "bip_spine_3", "bip_upperArm_L" };

// Dont fully understand struct but a guess is a group of something.
// I will return once I have enough knowlage to reverse this.
// NOTE: No idea on why we cant just use gethitbox and use the displacement on
// that insted of having all this extra code. Shouldnt gethitbox use cached
// hitboxes, if so it should be nicer on performance
struct bonelist_s
{
    bool setup{ false };
    bool success{ false };
    int leg_r[3]{ 0 };
    int leg_l[3]{ 0 };
    int bottom[3]{ 0 };
    int spine[7]{ 0 };
    int arm_r[3]{ 0 };
    int arm_l[3]{ 0 };
    int up[3]{ 0 };

    void Setup(const studiohdr_t *hdr)
    {
        if (!hdr)
        {
            setup = true;
            return;
        }
        std::unordered_map<std::string, int> bones{};
        for (int i = 0; i < hdr->numbones; i++)
        {
            bones[std::string(hdr->pBone(i)->pszName())] = i;
        }
        try
        {
            for (int i = 0; i < 3; i++)
                leg_r[i] = bones.at(bonenames_leg_r[i]);
            for (int i = 0; i < 3; i++)
                leg_l[i] = bones.at(bonenames_leg_l[i]);
            for (int i = 0; i < 3; i++)
                bottom[i] = bones.at(bonenames_bottom[i]);
            for (int i = 0; i < 7; i++)
                spine[i] = bones.at(bonenames_spine[i]);
            for (int i = 0; i < 3; i++)
                arm_r[i] = bones.at(bonenames_arm_r[i]);
            for (int i = 0; i < 3; i++)
                arm_l[i] = bones.at(bonenames_arm_l[i]);
            for (int i = 0; i < 3; i++)
                up[i] = bones.at(bonenames_up[i]);
            success = true;
        }
        catch (std::exception &ex)
        {
            logging::Info("Bone list exception: %s", ex.what());
        }
        setup = true;
    }

    void _FASTCALL DrawBoneList(const matrix3x4_t *bones, int *in, int size, const rgba_t &color)
    {
        Vector last_screen;
        Vector current_screen;
        for (int i = 0; i < size; i++)
        {
            const auto &bone = bones[in[i]];
            Vector position(bone[0][3], bone[1][3], bone[2][3]);
            if (!draw::WorldToScreen(position, current_screen))
            {
                return;
            }
            if (i > 0)
            {
                draw::Line(last_screen.x, last_screen.y, current_screen.x - last_screen.x, current_screen.y - last_screen.y, color, 0.5f);
            }
            last_screen = current_screen;
        }
    }

    void _FASTCALL Draw(CachedEntity *ent, const rgba_t &color)
    {
        const model_t *model = RAW_ENT(ent)->GetModel();
        if (not model)
        {
            return;
        }

        studiohdr_t *hdr = g_IModelInfo->GetStudiomodel(model);

        if (!setup)
        {
            Setup(hdr);
        }
        if (!success)
            return;

        // ent->m_bBonesSetup = false;
        const auto &bones = ent->hitboxes.GetBones();
        DrawBoneList(bones, leg_r, 3, color);
        DrawBoneList(bones, leg_l, 3, color);
        DrawBoneList(bones, bottom, 3, color);
        DrawBoneList(bones, spine, 7, color);
        DrawBoneList(bones, arm_r, 3, color);
        DrawBoneList(bones, arm_l, 3, color);
        DrawBoneList(bones, up, 3, color);
        /*for (int i = 0; i < hdr->numbones; i++) {
            const auto& bone = ent->GetBones()[i];
            Vector pos(bone[0][3], bone[1][3], bone[2][3]);
            //pos += orig;
            Vector screen;
            if (draw::WorldToScreen(pos, screen)) {
                if (hdr->pBone(i)->pszName()) {
                    draw::FString(fonts::ESP, screen.x, screen.y, fg, 2, "%s
        [%d]", hdr->pBone(i)->pszName(), i); } else draw::FString(fonts::ESP,
        screen.x, screen.y, fg, 2, "%d", i);
            }
        }*/
    }
};

std::unordered_map<studiohdr_t *, bonelist_s> bonelist_map{};
// Function called on draw
static void Draw()
{
    if (!enable)
        return;
    std::lock_guard<std::mutex> esp_lock(threadsafe_mutex);
    for (auto &i : entities_need_repaint)
    {
        ProcessEntityPT(ENTITY(i));
#ifndef FEATURE_EMOJI_ESP_DISABLED
        emoji(ENTITY(i));
#endif
    }
}

// Function called on create move
static void cm()
{
    // Check usersettings if enabled
    if (!*enable)
        return;
    if (CE_BAD(LOCAL_E) || CE_BAD(LOCAL_W))
        return;
    // Something
    std::lock_guard<std::mutex> esp_lock(threadsafe_mutex);

    ResetEntityStrings();          // Clear any strings entities have
    entities_need_repaint.clear(); // Clear data on entities that need redraw
    int max_clients = g_IEngine->GetMaxClients();
    int limit       = HIGHEST_ENTITY;

    // If not using any other special esp, we lower the min to the max
    // clients
    if (!buildings && !proj_esp && !item_esp)
        limit = std::min(max_clients, HIGHEST_ENTITY);

    { // Prof section ends when out of scope, these brackets here.
        PROF_SECTION(CM_ESP_EntityLoop);
        // Loop through entities
        for (int i = 0; i < limit; i++)
        {
            // Get an entity from the loop tick and process it
            CachedEntity *ent = ENTITY(i);
            if (CE_INVALID(ent) || !ent->m_bAlivePlayer())
                continue;
            ProcessEntity(ent);
            // Update Bones
            if (i <= MAX_PLAYERS)
                ent->hitboxes.GetHitbox(0);
            // Dont know what this check is for
            if (data[i].string_count)
            {

                // Set entity color
                rgba_t color = colors::EntityF(ent);
                if (RAW_ENT(ent)->IsDormant())
                {
                    color.r *= 0.5f;
                    color.g *= 0.5f;
                    color.b *= 0.5f;
                }
                SetEntityColor(ent, color);

                // If snow distance, add string here
                if (show_distance)
                {
                    AddEntityString(ent, format((int) (ENTITY(i)->m_flDistance() / 64 * 1.22f), 'm'));
                }
            }
            // No idea, this is confusing
            if (data[ent->m_IDX].needs_paint)
            {
                if (vischeck)
                    data[ent->m_IDX].transparent = !ent->IsVisible();
                entities_need_repaint.push_back(ent->m_IDX);
            }
        }
    }
}

static draw::Texture atlas{ paths::getDataPath("/textures/atlas.png") };
static draw::Texture idspec{ paths::getDataPath("/textures/idspec.png") };

Timer retry{};
void Init()
{
    /*esp_font_scale.InstallChangeCallback(
        [](IConVar *var, const char *pszOldValue, float flOldValue) {
            logging::Info("current font: %p %s %d", fonts::esp.get(),
       fonts::esp->path.c_str(), fonts::esp->isLoaded());
            fonts::esp.reset(new fonts::font(DATA_PATH "/fonts/megasans.ttf",
       esp_font_scale));
        });*/
}

void _FASTCALL emoji(CachedEntity *ent)
{
    // Check to prevent crashes
    if (CE_BAD(ent) || !ent->m_bAlivePlayer())
        return;
    // Emoji esp
    if (emoji_esp)
    {
        if (ent->m_Type() == ENTITY_PLAYER)
        {
            auto hit = ent->hitboxes.GetHitbox(0);
            if (!hit)
                return;
            Vector hbm, hbx;
            if (draw::WorldToScreen(hit->min, hbm) && draw::WorldToScreen(hit->max, hbx))
            {
                Vector head_scr;
                if (draw::WorldToScreen(hit->center, head_scr))
                {
                    float size = emoji_esp_scaling ? fabs(hbm.y - hbx.y) : *emoji_esp_size;
                    if (v9mode)
                        size *= 1.4;
                    if (!size || !emoji_min_size)
                        return;
                    if (emoji_esp_scaling && (size < *emoji_min_size))
                        size = *emoji_min_size;
                    player_info_s info{};
                    unsigned int steamID = 0;
                    unsigned int steamidarray[32]{};
                    bool hascall    = false;
                    steamidarray[0] = 479487126;
                    steamidarray[1] = 263966176;
                    steamidarray[2] = 840255344;
                    steamidarray[3] = 147831332;
                    steamidarray[4] = 854198748;
                    if (g_IEngine->GetPlayerInfo(ent->m_IDX, &info))
                        steamID = info.friendsID;
                    if (playerlist::AccessData(steamID).state == playerlist::k_EState::CAT)
                        draw::RectangleTextured(head_scr.x - size / 2, head_scr.y - size / 2, size, size, colors::white, idspec, 2 * 64, 1 * 64, 64, 64, 0);
                    for (int i = 0; i < 4; i++)
                    {
                        if (steamID == steamidarray[i])
                        {
                            static int ii = 1;
                            while (i > 3)
                            {
                                ii++;
                                i -= 4;
                            }
                            draw::RectangleTextured(head_scr.x - size / 2, head_scr.y - size / 2, size, size, colors::white, idspec, i * 64, ii * 64, 64, 64, 0);
                            hascall = true;
                        }
                    }
                    if (!hascall)
                        draw::RectangleTextured(head_scr.x - size / 2, head_scr.y - size / 2, size, size, colors::white, atlas, (3 + (v9mode ? 3 : (int) emoji_esp)) * 64, (v9mode ? 3 : 4) * 64, 64, 64, 0);
                }
            }
        }
    }
}
// Used when processing entitys with cached data from createmove in draw
void _FASTCALL ProcessEntityPT(CachedEntity *ent)
{
    PROF_SECTION(PT_esp_process_entity);

    // Check to prevent crashes
    if (CE_INVALID(ent) || !ent->m_bAlivePlayer())
        return;
    // Dormant
    bool dormant = false;
    if (RAW_ENT(ent)->IsDormant())
    {
        if (!ent->m_vecDormantOrigin())
            return;
        dormant = true;
    }

    int classid     = ent->m_iClassID();
    EntityType type = ent->m_Type();
    // Grab esp data
    ESPData &ent_data = data[ent->m_IDX];

    // Get color of entity
    // TODO, check if we can move this after world to screen check
    rgba_t fg = ent_data.color;
    if (!fg || fg.a == 0.0f)
    {
        fg = colors::EntityF(ent);
        if (dormant)
        {
            fg.r *= 0.75f;
            fg.g *= 0.75f;
            fg.b *= 0.75f;
        }
        ent_data.color = fg;
    }

    // Check if entity is on screen, then save screen position if true
    auto position = ent->m_vecDormantOrigin();
    if (!position)
        return;
    static Vector screen, origin_screen;
    if (!draw::EntityCenterToScreen(ent, screen) && !draw::WorldToScreen(*position, origin_screen))
        return;

    // Reset the collide cache
    ent_data.has_collide = false;

    // Get if ent should be transparent
    bool transparent = vischeck && ent_data.transparent;

    // Tracers
    if (tracers && type == ENTITY_PLAYER)
    {

        // Grab the screen resolution and save to some vars
        int width, height;
        g_IEngine->GetScreenSize(width, height);

        // Center values on screen
        width = width / 2;
        // Only center height if we are using center mode
        if ((int) tracers == 1)
            height = height / 2;

        // Get world to screen
        Vector scn;
        draw::WorldToScreen(*position, scn);

        // Draw a line
        draw::Line(scn.x, scn.y, width - scn.x, height - scn.y, fg, 0.5f);
    }

    // Sightline esp
    if (sightlines && type == ENTITY_PLAYER)
    {

        // Logic for using the enum to sort out snipers
        if ((int) sightlines == 2 || ((int) sightlines == 1 && CE_INT(ent, netvar.iClass) == tf_sniper))
        {
            PROF_SECTION(PT_esp_sightlines);

            // Get players angle and head position
            Vector &eye_angles = NET_VECTOR(RAW_ENT(ent), netvar.m_angEyeAngles);
            Vector eye_position;
            eye_position = ent->hitboxes.GetHitbox(0)->center;

            // Main ray tracing area
            float sy         = sinf(DEG2RAD(eye_angles.y)); // yaw
            float cy         = cosf(DEG2RAD(eye_angles.y));
            float sp         = sinf(DEG2RAD(eye_angles.x)); // pitch
            float cp         = cosf(DEG2RAD(eye_angles.x));
            Vector forward_t = Vector(cp * cy, cp * sy, -sp);
            // We dont want the sightlines endpoint to go behind us because the
            // world to screen check will fail, but keep it at most 4096
            Vector forward = forward_t * 4096.0F + eye_position;
            Ray_t ray;
            ray.Init(eye_position, forward);
            trace_t trace;
            g_ITrace->TraceRay(ray, MASK_SHOT_HULL, &trace::filter_no_player, &trace);

            // Screen vectors
            Vector scn1, scn2;

            // Status vars
            bool found_scn2 = true;

            // Get end point on screen
            if (!draw::WorldToScreen(trace.endpos, scn2))
            {
                // Set status
                found_scn2 = false;
                // Get the end distance from the trace
                float end_distance = trace.endpos.DistTo(eye_position);

                // Loop and look back untill we have a vector on screen
                for (int i = 1; i > 400; i++)
                {
                    // Subtract 40 multiplyed by the tick from the end distance
                    // and use that as our length to check
                    Vector end_vector = forward_t * (end_distance - (10 * i)) + eye_position;
                    if (end_vector.DistTo(eye_position) < 1)
                        break;
                    if (draw::WorldToScreen(end_vector, scn2))
                    {
                        found_scn2 = true;
                        break;
                    }
                }
            }

            if (found_scn2)
            {
                // Set status
                bool found_scn1 = true;

                // If we dont have a vector on screen, attempt to find one
                if (!draw::WorldToScreen(eye_position, scn1))
                {
                    // Set status
                    found_scn1 = false;
                    // Get the end distance from the trace
                    float start_distance = trace.endpos.DistTo(eye_position);

                    // Loop and look back untill we have a vector on screen
                    for (int i = 1; i > 400; i++)
                    {
                        // Multiply starting distance by 40, multiplyed by the
                        // loop tick
                        Vector start_vector = forward_t * (10 * i) + eye_position;
                        // We dont want it to go too far
                        if (start_vector.DistTo(trace.endpos) < 1)
                            break;
                        // Check if we have a vector on screen, if we do then we
                        // set our status
                        if (draw::WorldToScreen(start_vector, scn1))
                        {
                            found_scn1 = true;
                            break;
                        }
                    }
                }
                // We have both vectors, draw
                if (found_scn1)
                {
                    draw::Line(scn1.x, scn1.y, scn2.x - scn1.x, scn2.y - scn1.y, fg, 0.5f);
                }
            }
        }
    }
    // Box esp
    if (box_esp || box_3d_player || box_3d_building)
    {
        switch (type)
        {
        case ENTITY_PLAYER:
            if (vischeck && !ent->IsVisible())
                transparent = true;
            if (!fg)
                fg = colors::EntityF(ent);
            if (transparent)
                fg = colors::Transparent(fg);
            if (RAW_ENT(ent)->IsDormant())
            {
                fg.r *= 0.75f;
                fg.g *= 0.75f;
                fg.b *= 0.75f;
            }
            if (!box_3d_player && box_esp)
                DrawBox(ent, fg);
            else if (box_3d_player)
                Draw3DBox(ent, fg);
            break;
        case ENTITY_BUILDING:
            if (CE_INT(ent, netvar.iTeamNum) == g_pLocalPlayer->team && !teammates)
                break;
            if (!transparent && vischeck && !ent->IsVisible())
                transparent = true;
            if (!fg)
                fg = colors::EntityF(ent);
            if (transparent)
                fg = colors::Transparent(fg);
            if (RAW_ENT(ent)->IsDormant())
            {
                fg.r *= 0.75f;
                fg.g *= 0.75f;
                fg.b *= 0.75f;
            }
            // Draw exit arrow
            // YawToExit is 0.0f on exit and on newly placed still disabled entrances
            // m_iState is 0 when the TP is disabled
            if (ent->m_iClassID() == CL_CLASS(CObjectTeleporter) && CE_FLOAT(ent, netvar.m_flTeleYawToExit) == 0.0f && CE_INT(ent, netvar.m_iTeleState) > 1)
            {
                float sin_a, cos_a;
                // for some reason vAngRotation yaw differs from exit direction, unsure why
                SinCos(DEG2RAD(CE_VECTOR(ent, netvar.m_angRotation).y - 90.0f), &sin_a, &cos_a);

                // pseudo used to rotate properly
                // screen is passed to filledpolygon
                Vector pseudo[3];
                Vector screen[3];

                // 24 = teleporter width and height
                // 16 = arrow size
                pseudo[0].x = 0.0f; // Undefined behaviour bruh
                pseudo[0].y = 16.0f + 24.0f;
                pseudo[1].x = -16.0f;
                pseudo[1].y = 24.0f;
                pseudo[2].x = 16.0f;
                pseudo[2].y = 24.0f;

                // pass vector, get vector
                // 12 is teleporter height, arrow is 12 HU off the ground
                // sin and cos are already passed in captures
                auto rotateVector = [sin_a = sin_a, cos_a = cos_a](Vector &in) { return Vector(in.x * cos_a - in.y * sin_a, in.x * sin_a + in.y * cos_a, 12.0f); };

                // fail check
                bool visible = true;

                // rotate, add vecorigin AND check worldtoscreen at the same time in single loop
                for (int p = 0; p < 3; p++)
                    if (!(visible = draw::WorldToScreen(rotateVector(pseudo[p]) + ent->m_vecOrigin(), screen[p])))
                        break;

                // if visible, pass it and draw the whole thing, ez pz
                if (visible)
                    draw::Triangle(screen[0].x, screen[0].y, screen[1].x, screen[1].y, screen[2].x, screen[2].y, fg);
            }
            if (!box_3d_building && box_esp)
                DrawBox(ent, fg);
            else if (box_3d_building)
                Draw3DBox(ent, fg);
            break;
        }
    }

    if (draw_bones)
    {
        if (vischeck && !ent->IsVisible())
            transparent = true;
        rgba_t bone_color = colors::EntityF(ent);
        if (transparent)
            bone_color = colors::Transparent(bone_color);

        bonelist_s bl;
        if (!CE_INVALID(ent) && ent->m_bAlivePlayer() && !RAW_ENT(ent)->IsDormant() && LOCAL_E->m_bAlivePlayer())
        {
            if (bones_color)
                bl.Draw(ent, bone_color);
            else
                bl.Draw(ent, colors::white);
        }
    }

    // Top horizontal health bar
    if (*healthbar == 1)
    {

        // We only want health bars on players and buildings
        if (type == ENTITY_PLAYER || type == ENTITY_BUILDING)
        {

            // Get collidable from the cache
            if (GetCollide(ent))
            {

                // Pull the cached collide info
                int max_x = ent_data.collide_max.x;
                int max_y = ent_data.collide_max.y;
                int min_x = ent_data.collide_min.x;
                int min_y = ent_data.collide_min.y;

                // Get health values
                int health    = 0;
                int healthmax = 0;
                switch (type)
                {
                case ENTITY_PLAYER:
                    health    = g_pPlayerResource->GetHealth(ent);
                    healthmax = g_pPlayerResource->GetMaxHealth(ent);
                    break;
                case ENTITY_BUILDING:
                    health    = CE_INT(ent, netvar.iBuildingHealth);
                    healthmax = CE_INT(ent, netvar.iBuildingMaxHealth);
                    break;
                }

                // Get Colors
                rgba_t hp     = colors::Transparent(colors::Health(health, healthmax), fg.a);
                rgba_t border = ((classid == RCC_PLAYER) && IsPlayerInvisible(ent)) ? colors::FromRGBA8(160, 160, 160, fg.a * 255.0f) : colors::Transparent(colors::black, fg.a);
                // Get bar width
                int hbw = (max_x - min_x - 1) * std::min((float) health / (float) healthmax, 1.0f);

                // Draw
                draw::RectangleOutlined(min_x, min_y - 6, max_x - min_x + 1, 7, border, 0.5f);
                draw::Rectangle(min_x + hbw, min_y - 5, -hbw, 5, hp);
            }
        }
    }

    // Bottom horizontal health bar
    else if (*healthbar == 2)
    {

        // We only want health bars on players and buildings
        if (type == ENTITY_PLAYER || type == ENTITY_BUILDING)
        {

            // Get collidable from the cache
            if (GetCollide(ent))
            {

                // Pull the cached collide info
                int max_x = ent_data.collide_max.x;
                int max_y = ent_data.collide_max.y;
                int min_x = ent_data.collide_min.x;
                int min_y = ent_data.collide_min.y;

                // Get health values
                int health    = 0;
                int healthmax = 0;
                switch (type)
                {
                case ENTITY_PLAYER:
                    health    = g_pPlayerResource->GetHealth(ent);
                    healthmax = g_pPlayerResource->GetMaxHealth(ent);
                    break;
                case ENTITY_BUILDING:
                    health    = CE_INT(ent, netvar.iBuildingHealth);
                    healthmax = CE_INT(ent, netvar.iBuildingMaxHealth);
                    break;
                }

                // Get Colors
                rgba_t hp     = colors::Transparent(colors::Health(health, healthmax), fg.a);
                rgba_t border = ((classid == RCC_PLAYER) && IsPlayerInvisible(ent)) ? colors::FromRGBA8(160, 160, 160, fg.a * 255.0f) : colors::Transparent(colors::black, fg.a);
                // Get bar width
                int hbw = (max_x - min_x - 1) * std::min((float) health / (float) healthmax, 1.0f);

                // Draw
                draw::RectangleOutlined(min_x, max_y, max_x - min_x + 1, 7, border, 0.5f);
                draw::Rectangle(min_x + hbw, max_y + 1, -hbw, 5, hp);
            }
        }
    }

    // Vertical health bar
    else if (*healthbar == 3)
    {

        // We only want health bars on players and buildings
        if (type == ENTITY_PLAYER || type == ENTITY_BUILDING)
        {

            // Get collidable from the cache
            if (GetCollide(ent))
            {

                // Pull the cached collide info
                int max_x = ent_data.collide_max.x;
                int max_y = ent_data.collide_max.y;
                int min_x = ent_data.collide_min.x;
                int min_y = ent_data.collide_min.y;

                // Get health values
                int health    = 0;
                int healthmax = 0;
                switch (type)
                {
                case ENTITY_PLAYER:
                    health    = g_pPlayerResource->GetHealth(ent);
                    healthmax = g_pPlayerResource->GetMaxHealth(ent);
                    break;
                case ENTITY_BUILDING:
                    health    = CE_INT(ent, netvar.iBuildingHealth);
                    healthmax = CE_INT(ent, netvar.iBuildingMaxHealth);
                    break;
                }

                // Get Colors
                rgba_t hp     = colors::Transparent(colors::Health(health, healthmax), fg.a);
                rgba_t border = ((classid == RCC_PLAYER) && IsPlayerInvisible(ent)) ? colors::FromRGBA8(160, 160, 160, fg.a * 255.0f) : colors::Transparent(colors::black, fg.a);
                // Get bar height
                int hbh = (max_y - min_y - 2) * std::min((float) health / (float) healthmax, 1.0f);

                // Draw
                draw::RectangleOutlined(min_x - 7, min_y, 7, max_y - min_y, border, 0.5f);
                draw::Rectangle(min_x - 6, max_y - hbh - 1, 5, hbh, hp);
            }
        }
    }

    // Check if entity has strings to draw
    if (ent_data.string_count)
    {
        PROF_SECTION(PT_esp_drawstrings);

        // Create our initial point at the center of the entity
        Vector draw_point   = screen;
        bool origin_is_zero = true;

        // Only get collidable for players and buildings
        if (type == ENTITY_PLAYER || type == ENTITY_BUILDING)
        {

            // Get collidable from the cache
            if (GetCollide(ent))
            {

                // Origin could change so we set to false
                origin_is_zero = false;

                // Pull the cached collide info
                int max_x = ent_data.collide_max.x;
                int max_y = ent_data.collide_max.y;
                int min_x = ent_data.collide_min.x;
                int min_y = ent_data.collide_min.y;

                // Change the position of the draw point depending on the user
                // settings
                switch ((int) esp_text_position)
                {
                case 0:
                { // TOP RIGHT
                    draw_point = Vector(max_x + 2, min_y, 0);
                }
                break;
                case 1:
                { // BOTTOM RIGHT
                    draw_point = Vector(max_x + 2,
                                        max_y - data.at(ent->m_IDX).string_count *
                                                    /*((int)fonts::font_main->height)*/ 14,
                                        0);
                }
                break;
                case 2:
                {                          // CENTER
                    origin_is_zero = true; // origin is still zero so we set to true
                }
                break;
                case 3:
                { // ABOVE
                    draw_point = Vector(min_x,
                                        min_y - data.at(ent->m_IDX).string_count *
                                                    /*((int)fonts::font_main->height)*/ 14,
                                        0);
                }
                break;
                case 4:
                { // BELOW
                    draw_point = Vector(min_x, max_y, 0);
                }
                }
            }
        }

        // if user setting allows vis check and ent isnt visable, make
        // transparent
        if (vischeck && !ent->IsVisible())
            transparent = true;

        // Loop through strings
        for (int j = 0; j < ent_data.string_count; j++)
        {

            // Pull string from the entity's cached string array
            const ESPString &string = ent_data.strings[j];

            // If string has a color assined to it, apply that otherwise use
            // entities color
            rgba_t color = string.color ? string.color : ent_data.color;
            if (transparent)
                color = colors::Transparent(color); // Apply transparency if needed

            // If the origin is centered, we use one method. if not, the other
            if (!origin_is_zero || true)
            {
                draw::String(draw_point.x, draw_point.y, color, string.data.c_str(), *fonts::esp);
            }
            else
            { /*
          int size_x;
          FTGL_StringLength(string.data, fonts::font_main, &size_x);
          FTGL_Draw(string.data, draw_point.x - size_x / 2, draw_point.y,
          fonts::font_main, color);
      */
            }

            // Add to the y due to their being text in that spot
            draw_point.y += /*((int)fonts::font_main->height)*/ 15 - 1;
        }
    }

    // TODO Add Rotation matix
    // TODO Currently crashes, needs null check somewhere
    // Draw Hitboxes
    /*if (draw_hitbox && type == ENTITY_PLAYER) {
        PROF_SECTION(PT_esp_drawhitbboxes);

        // Loop through hitboxes
        for (int i = 0; i <= 17; i++) { // I should probs get how many hitboxes
    instead of using a fixed number...

            // Get a hitbox from the entity
            hitbox_cache::CachedHitbox* hb = ent->hitboxes.GetHitbox(i);

            // Create more points from min + max
            Vector box_points[8];
            Vector vec_tmp;
            for (int ii = 0; ii <= 8; ii++) { // 8 points to the box

                // logic le paste from sdk
                vec_tmp[0] = ( ii & 0x1 ) ? hb->max[0] : hb->min[0];
                vec_tmp[1] = ( ii & 0x2 ) ? hb->max[1] : hb->min[1];
                vec_tmp[2] = ( ii & 0x4 ) ? hb->max[2] : hb->min[2];

                // save to points array
                box_points[ii] = vec_tmp;
            }

            // Draw box from points
            // Draws a point to every other point. Ineffient, use now fix
    later... Vector scn1, scn2; // to screen for (int ii = 0; ii < 8; ii++) {

                // Get first point
                if (!draw::WorldToScreen(box_points[ii], scn1)) continue;

                for (int iii = 0; iii < 8; iii++) {

                    // Get second point
                    if (!draw::WorldToScreen(box_points[iii], scn2)) continue;

                    // Draw between points
                    draw_api::Line(scn1.x, scn1.y, scn2.x - scn1.x, scn2.y -
    scn1.y, fg);
                }
            }
        }
    }*/
}

// Used to process entities from CreateMove
void _FASTCALL ProcessEntity(CachedEntity *ent)
{
    if (!enable)
        return; // Esp enable check
    if (CE_INVALID(ent) || !ent->m_bAlivePlayer())
        return; // CE_INVALID check to prevent crashes
    // Dormant
    if (RAW_ENT(ent)->IsDormant())
    {
        if (!ent->m_vecDormantOrigin())
            return;
    }
    if (max_dist && ent->m_flDistance() > *max_dist)
        return;
    int classid = ent->m_iClassID();
    // Entity esp
    if (entity_info)
    {
        AddEntityString(ent, format(RAW_ENT(ent)->GetClientClass()->m_pNetworkName, " [", classid, "]"));
        if (entity_id)
        {
            AddEntityString(ent, std::to_string(ent->m_IDX));
        }
        if (entity_model)
        {
            const model_t *model = RAW_ENT(ent)->GetModel();
            if (model)
                AddEntityString(ent, std::string(g_IModelInfo->GetModelName(model)));
        }
    }

    // Get esp data from current ent
    ESPData &espdata = data[ent->m_IDX];

    // Projectile esp
    if (ent->m_Type() == ENTITY_PROJECTILE && proj_esp && (ent->m_bEnemy() || (teammates && !proj_enemy)))
    {

        // Rockets
        if (classid == CL_CLASS(CTFProjectile_Rocket) || classid == CL_CLASS(CTFProjectile_SentryRocket))
        {
            if (proj_rockets)
            {
                if ((int) proj_rockets != 2 || ent->m_bCritProjectile())
                {
                    AddEntityString(ent, "[ ==> ]");
                }
            }

            // Pills/Stickys
        }
        else if (classid == CL_CLASS(CTFGrenadePipebombProjectile))
        {
            // Switch based on pills/stickys
            switch (CE_INT(ent, netvar.iPipeType))
            {
            case 0: // Pills
                if (!proj_pipes)
                    break;
                if ((int) proj_pipes == 2 && !ent->m_bCritProjectile())
                    break;
                AddEntityString(ent, "[ (PP) ]");
                break;
            case 1: // Stickys
                if (!proj_stickies)
                    break;
                if ((int) proj_stickies == 2 && !ent->m_bCritProjectile())
                    break;
                AddEntityString(ent, "[ {*} ]");
            }

            // Huntsman
        }
        else if (classid == CL_CLASS(CTFProjectile_Arrow))
        {
            if ((int) proj_arrows != 2 || ent->m_bCritProjectile())
            {
                AddEntityString(ent, "[ >>---> ]");
            }
        }
    }

    // Hl2DM dropped item esp
    IF_GAME(IsHL2DM())
    {
        if (item_esp && item_dropped_weapons)
        {
            if (CE_BYTE(ent, netvar.hOwner) == (unsigned char) -1)
            {
                int string_count_backup = data[ent->m_IDX].string_count;
                if (classid == CL_CLASS(CWeapon_SLAM))
                    AddEntityString(ent, "SLAM");
                else if (classid == CL_CLASS(CWeapon357))
                    AddEntityString(ent, ".357");
                else if (classid == CL_CLASS(CWeaponAR2))
                    AddEntityString(ent, "AR2");
                else if (classid == CL_CLASS(CWeaponAlyxGun))
                    AddEntityString(ent, "Alyx Gun");
                else if (classid == CL_CLASS(CWeaponAnnabelle))
                    AddEntityString(ent, "Annabelle");
                else if (classid == CL_CLASS(CWeaponBinoculars))
                    AddEntityString(ent, "Binoculars");
                else if (classid == CL_CLASS(CWeaponBugBait))
                    AddEntityString(ent, "Bug Bait");
                else if (classid == CL_CLASS(CWeaponCrossbow))
                    AddEntityString(ent, "Crossbow");
                else if (classid == CL_CLASS(CWeaponShotgun))
                    AddEntityString(ent, "Shotgun");
                else if (classid == CL_CLASS(CWeaponSMG1))
                    AddEntityString(ent, "SMG");
                else if (classid == CL_CLASS(CWeaponRPG))
                    AddEntityString(ent, "RPG");
                if (string_count_backup != data[ent->m_IDX].string_count)
                {
                    SetEntityColor(ent, colors::yellow);
                }
            }
        }
    }

    int itemtype = ent->m_ItemType();
    // Tank esp
    if (classid == CL_CLASS(CTFTankBoss) && tank)
    {
        AddEntityString(ent, "Tank");

        // Dropped weapon esp
    }
    else if (classid == CL_CLASS(CTFDroppedWeapon) && item_esp && item_dropped_weapons)
    {
        AddEntityString(ent, format("WEAPON ", RAW_ENT(ent)->GetClientClass()->GetName()));

        // MVM Money esp
    }
    else if (classid == CL_CLASS(CCurrencyPack) && item_money)
    {
        if (CE_BYTE(ent, netvar.bDistributed))
        {
            if (item_money_red)
            {
                AddEntityString(ent, "~$~");
            }
        }
        else
        {
            AddEntityString(ent, "$$$");
        }

        // Other item esp
    }
    else if (itemtype != ITEM_NONE && item_esp)
    {

        // Health pack esp
        if (item_health_packs && (itemtype >= ITEM_HEALTH_SMALL && itemtype <= ITEM_HEALTH_LARGE || itemtype == ITEM_HL_BATTERY))
        {
            if (itemtype == ITEM_HEALTH_SMALL)
                AddEntityString(ent, "[+]");
            if (itemtype == ITEM_HEALTH_MEDIUM)
                AddEntityString(ent, "[++]");
            if (itemtype == ITEM_HEALTH_LARGE)
                AddEntityString(ent, "[+++]");
            if (itemtype == ITEM_HL_BATTERY)
                AddEntityString(ent, "[Z]");

            // TF2C Adrenaline esp
        }
        else if (item_adrenaline && itemtype == ITEM_TF2C_PILL)
        {
            AddEntityString(ent, "[a]");

            // Ammo pack esp
        }
        else if (item_ammo_packs && itemtype >= ITEM_AMMO_SMALL && itemtype <= ITEM_AMMO_LARGE)
        {
            if (itemtype == ITEM_AMMO_SMALL)
                AddEntityString(ent, "{i}");
            if (itemtype == ITEM_AMMO_MEDIUM)
                AddEntityString(ent, "{ii}");
            if (itemtype == ITEM_AMMO_LARGE)
                AddEntityString(ent, "{iii}");

            // Powerup esp
        }
        else if (item_powerups && itemtype >= ITEM_POWERUP_FIRST && itemtype <= ITEM_POWERUP_LAST)
        {
            AddEntityString(ent, format(powerups[itemtype - ITEM_POWERUP_FIRST], " PICKUP"));

            // TF2C weapon spawner esp
        }
        else if (item_weapon_spawners && itemtype >= ITEM_TF2C_W_FIRST && itemtype <= ITEM_TF2C_W_LAST)
        {
            AddEntityString(ent, format(tf2c_weapon_names[itemtype - ITEM_TF2C_W_FIRST], " SPAWNER"));
            if (CE_BYTE(ent, netvar.bRespawning))
                AddEntityString(ent, "-- RESPAWNING --");

            // Halloween spell esp
        }
        else if (item_spellbooks && (itemtype == ITEM_SPELL || itemtype == ITEM_SPELL_RARE))
        {
            if (itemtype == ITEM_SPELL)
            {
                AddEntityString(ent, "Spell", colors::green);
            }
            else
            {
                AddEntityString(ent, "Rare Spell", colors::FromRGBA8(139, 31, 221, 255));
            }
        }

        // Building esp
    }
    else if (ent->m_Type() == ENTITY_BUILDING && buildings)
    {

        // Check if enemy building
        if (!ent->m_bEnemy() && !teammates)
            return;

        // TODO maybe...
        /*if (legit && ent->m_iTeam() != g_pLocalPlayer->team) {
            if (ent->m_lLastSeen > v_iLegitSeenTicks->GetInt()) {
                return;
            }
        }*/

        // Make a name for the building based on the building type and level
        if (show_name || show_class)
        {
            const std::string &name = (classid == CL_CLASS(CObjectTeleporter) ? "Teleporter" : (classid == CL_CLASS(CObjectSentrygun) ? "Sentry Gun" : "Dispenser"));
            int level               = CE_INT(ent, netvar.iUpgradeLevel);
            bool IsMini             = CE_BYTE(ent, netvar.m_bMiniBuilding);
            bool IsSapped           = CE_BYTE(ent, netvar.m_bHasSapper);
            if (!IsMini)
                AddEntityString(ent, format("LV ", level, ' ', name));
            else
                AddEntityString(ent, format("Mini ", name));
            if (IsSapped)
                AddEntityString(ent, "*Sapped*");
            if (classid == CL_CLASS(CObjectTeleporter))
            {
                float next_teleport = CE_FLOAT(ent, netvar.m_flTeleRechargeTime);
                float yaw_to_exit   = CE_FLOAT(ent, netvar.m_flTeleYawToExit);
                if (yaw_to_exit)
                {
                    if (next_teleport < g_GlobalVars->curtime)
                        AddEntityString(ent, "Ready");
                    else
                        AddEntityString(ent, format(next_teleport - g_GlobalVars->curtime, "s"));
                }
            }
        }
        // If text health is true, then add a string with the health
        if (show_health)
        {
            AddEntityString(ent, format(ent->m_iHealth(), '/', ent->m_iMaxHealth(), " HP"), colors::Health(ent->m_iHealth(), ent->m_iMaxHealth()));
        }
        // Set the entity to repaint
        espdata.needs_paint = true;
        return;

        // Player esp
    }
    else if (ent->m_Type() == ENTITY_PLAYER && ent->m_bAlivePlayer())
    {

        // Local player handling
        if (!(local_esp && g_IInput->CAM_IsThirdPerson()) && ent->m_IDX == g_IEngine->GetLocalPlayer())
            return;

        // Get player class
        int pclass = CE_INT(ent, netvar.iClass);

        // Attempt to get player info, and if cant, return
        player_info_s info;
        if (!g_IEngine->GetPlayerInfo(ent->m_IDX, &info))
            return;

        // TODO, check if u can just use "ent->m_bEnemy()" instead of m_iTeam
        // Legit mode handling
        if (legit && ent->m_iTeam() != g_pLocalPlayer->team && playerlist::IsDefault(info.friendsID))
        {
            if (IsPlayerInvisible(ent))
                return; // Invis check
            if (vischeck && !ent->IsVisible())
                return; // Vis check
                        // TODO, maybe...
                        // if (ent->m_lLastSeen >
                        // (unsigned)v_iLegitSeenTicks->GetInt())
            // return;
        }

        // Powerup handling
        if (powerup_esp)
        {
            powerup_type power = GetPowerupOnPlayer(ent);
            if (power != not_powerup)
                AddEntityString(ent, format("^ ", powerups[power], " ^"));
        }

        // Dont understand reasoning for this check
        if (ent->m_bEnemy() || teammates || player_tools::shouldAlwaysRenderEsp(ent))
        {

            // Playername
            if (show_name)
                AddEntityString(ent, std::string(info.name));

            // Player class
            if (show_class)
            {
                if (pclass > 0 && pclass < 10)
                    AddEntityString(ent, classes[pclass - 1]);
            }

#if ENABLE_IPC
            // ipc bot esp
            if (show_bot_id && ipc::peer && ent != LOCAL_E)
            {
                for (unsigned i = 0; i < cat_ipc::max_peers; i++)
                {
                    if (!ipc::peer->memory->peer_data[i].free && ipc::peer->memory->peer_user_data[i].friendid == info.friendsID)
                    {
                        AddEntityString(ent, format("BOT #", i));
                        break;
                    }
                }
            }
#endif
            // Health esp
            if (show_health)
            {
                AddEntityString(ent, format(g_pPlayerResource->GetHealth(ent), '/', g_pPlayerResource->GetMaxHealth(ent), " HP"), colors::Health(g_pPlayerResource->GetHealth(ent), g_pPlayerResource->GetMaxHealth(ent)));
            }
            IF_GAME(IsTF())
            {
                // Medigun Ubercharge esp
                if (show_ubercharge)
                {
                    if (CE_INT(ent, netvar.iClass) == tf_medic)
                    {
                        int *weapon_list = (int *) ((unsigned) (RAW_ENT(ent)) + netvar.hMyWeapons);
                        for (int i = 0; weapon_list[i]; i++)
                        {
                            int handle = weapon_list[i];
                            int eid    = handle & 0xFFF;
                            if (eid > MAX_PLAYERS && eid <= HIGHEST_ENTITY)
                            {
                                CachedEntity *weapon = ENTITY(eid);
                                if (!CE_INVALID(weapon) && weapon->m_iClassID() == CL_CLASS(CWeaponMedigun) && weapon)
                                {
                                    if (CE_INT(weapon, netvar.iItemDefinitionIndex) != 998)
                                    {
                                        AddEntityString(ent, format(floor(CE_FLOAT(weapon, netvar.m_flChargeLevel) * 100), '%', " Uber"), colors::Health((CE_FLOAT(weapon, netvar.m_flChargeLevel) * 100), 100));
                                    }
                                    else
                                        AddEntityString(ent, format(floor(CE_FLOAT(weapon, netvar.m_flChargeLevel) * 100), '%', " Uber | Charges: ", floor(CE_FLOAT(weapon, netvar.m_flChargeLevel) / 0.25f)), colors::Health((CE_FLOAT(weapon, netvar.m_flChargeLevel) * 100), 100));
                                    break;
                                }
                            }
                        }
                    }
                }
                // Conditions esp
                if (show_conditions)
                {
                    auto clr = colors::EntityF(ent);
                    if (RAW_ENT(ent)->IsDormant())
                    {
                        clr.r *= 0.5f;
                        clr.g *= 0.5f;
                        clr.b *= 0.5f;
                    }
                    // Invis
                    if (IsPlayerInvisible(ent))
                    {
                        if (HasCondition<TFCond_DeadRingered>(ent))
                            AddEntityString(ent, "*DEADRINGERED*", colors::FromRGBA8(178.0f, 0.0f, 255.0f, 255.0f));
                        else
                            AddEntityString(ent, "*CLOAKED*", colors::FromRGBA8(220.0f, 220.0f, 220.0f, 255.0f));
                    }
                    if (CE_BYTE(ent, netvar.m_bFeignDeathReady))
                        AddEntityString(ent, "*DEADRINGER OUT*", colors::FromRGBA8(178.0f, 0.0f, 255.0f, 255.0f));
                    // Uber/Bonk
                    if (IsPlayerInvulnerable(ent))
                        AddEntityString(ent, "*INVULNERABLE*");
                    // Vaccinator
                    if (HasCondition<TFCond_UberBulletResist>(ent))
                    {
                        AddEntityString(ent, "*BULLET VACCINATOR*", colors::FromRGBA8(220, 220, 220, 255));
                    }
                    else if (HasCondition<TFCond_SmallBulletResist>(ent))
                    {
                        AddEntityString(ent, "*BULLET PASSIVE*");
                    }
                    if (HasCondition<TFCond_UberFireResist>(ent))
                    {
                        AddEntityString(ent, "*FIRE VACCINATOR*", colors::FromRGBA8(220, 220, 220, 255));
                    }
                    else if (HasCondition<TFCond_SmallFireResist>(ent))
                    {
                        AddEntityString(ent, "*FIRE PASSIVE*");
                    }
                    if (HasCondition<TFCond_UberBlastResist>(ent))
                    {
                        AddEntityString(ent, "*BLAST VACCINATOR*", colors::FromRGBA8(220, 220, 220, 255));
                    }
                    else if (HasCondition<TFCond_SmallBlastResist>(ent))
                    {
                        AddEntityString(ent, "*BLAST PASSIVE*");
                    }
                    // Crit
                    if (IsPlayerCritBoosted(ent))
                        AddEntityString(ent, "*CRITS*", colors::orange);
                    // Zoomed
                    if (HasCondition<TFCond_Zoomed>(ent))
                    {
                        AddEntityString(ent, "*ZOOMING*", colors::FromRGBA8(220.0f, 220.0f, 220.0f, 255.0f));
                        // Slowed
                    }
                    else if (HasCondition<TFCond_Slowed>(ent))
                    {
                        AddEntityString(ent, "*SLOWED*", colors::FromRGBA8(220.0f, 220.0f, 220.0f, 255.0f));
                    }
                    // Jarated
                    if (HasCondition<TFCond_Jarated>(ent))
                        AddEntityString(ent, "*JARATED*", colors::yellow);
                    // Dormant
                    if (CE_VALID(ent) && RAW_ENT(ent)->IsDormant())
                        AddEntityString(ent, "*DORMANT*", colors::red);
                }
            }
            // Hoovy Esp
            if (IsHoovy(ent))
                AddEntityString(ent, "Hoovy");

            // Active weapon esp
            int widx = CE_INT(ent, netvar.hActiveWeapon) & 0xFFF;
            if (IDX_GOOD(widx))
            {
                CachedEntity *weapon = ENTITY(widx);
                if (CE_VALID(weapon) && re::C_BaseCombatWeapon::IsBaseCombatWeapon(RAW_ENT(weapon)))
                {
                    if (show_weapon)
                    {
                        const char *weapon_name = re::C_BaseCombatWeapon::GetPrintName(RAW_ENT(weapon));
                        if (weapon_name)
                            AddEntityString(ent, std::string(weapon_name));
                    }
                }
            }

            // Notify esp to repaint
            espdata.needs_paint = true;
        }
        return;
    }
}

// Draw 3D box around player/building
void _FASTCALL Draw3DBox(CachedEntity *ent, const rgba_t &clr)
{
    if (CE_INVALID(ent) || !ent->m_bAlivePlayer())
        return;

    Vector origin = RAW_ENT(ent)->GetCollideable()->GetCollisionOrigin();
    Vector mins   = RAW_ENT(ent)->GetCollideable()->OBBMins();
    Vector maxs   = RAW_ENT(ent)->GetCollideable()->OBBMaxs();
    // Dormant
    if (RAW_ENT(ent)->IsDormant())
    {
        auto vec = ent->m_vecDormantOrigin();
        if (!vec)
            origin = *vec;
        else
            return;
    }

    // Create a array for storing box points
    Vector corners[8]; // World vectors
    Vector points[8];  // Screen vectors

    // Create points for the box based on max and mins
    float x    = maxs.x - mins.x;
    float y    = maxs.y - mins.y;
    float z    = maxs.z - mins.z;
    corners[0] = mins;
    corners[1] = mins + Vector(x, 0, 0);
    corners[2] = mins + Vector(x, y, 0);
    corners[3] = mins + Vector(0, y, 0);
    corners[4] = mins + Vector(0, 0, z);
    corners[5] = mins + Vector(x, 0, z);
    corners[6] = mins + Vector(x, y, z);
    corners[7] = mins + Vector(0, y, z);

    // Rotate the box and check if any point of the box isnt on the screen
    bool success = true;
    for (int i = 0; i < 8; i++)
    {
        float yaw    = NET_VECTOR(RAW_ENT(ent), netvar.m_angEyeAngles).y;
        float s      = sinf(DEG2RAD(yaw));
        float c      = cosf(DEG2RAD(yaw));
        float xx     = corners[i].x;
        float yy     = corners[i].y;
        corners[i].x = (xx * c) - (yy * s);
        corners[i].y = (xx * s) + (yy * c);
        corners[i] += origin;

        if (!draw::WorldToScreen(corners[i], points[i]))
            success = false;
    }

    // Don't continue if a point isn't on the screen
    if (!success)
        return;

    rgba_t draw_clr = clr;
    // Draw the actual box
    for (int i = 1; i <= 4; i++)
    {
        draw::Line((points[i - 1].x), (points[i - 1].y), (points[i % 4].x) - (points[i - 1].x), (points[i % 4].y) - (points[i - 1].y), draw_clr, 0.5f);
        draw::Line((points[i - 1].x), (points[i - 1].y), (points[i + 3].x) - (points[i - 1].x), (points[i + 3].y) - (points[i - 1].y), draw_clr, 0.5f);
        draw::Line((points[i + 3].x), (points[i + 3].y), (points[i % 4 + 4].x) - (points[i + 3].x), (points[i % 4 + 4].y) - (points[i + 3].y), draw_clr, 0.5f);
    }
}

// Draw a box around a player
void _FASTCALL DrawBox(CachedEntity *ent, const rgba_t &clr)
{
    PROF_SECTION(PT_esp_drawbox);

    // Check if ent is bad to prevent crashes
    if (CE_INVALID(ent) || !ent->m_bAlivePlayer())
        return;

    // Get our collidable bounds
    if (!GetCollide(ent))
        return;

    // Pull the cached collide info
    ESPData &ent_data = data[ent->m_IDX];
    int max_x         = ent_data.collide_max.x;
    int max_y         = ent_data.collide_max.y;
    int min_x         = ent_data.collide_min.x;
    int min_y         = ent_data.collide_min.y;

    // Depending on whether the player is cloaked, we change the color
    // acordingly
    rgba_t border = ((ent->m_iClassID() == RCC_PLAYER) && IsPlayerInvisible(ent)) ? colors::FromRGBA8(160, 160, 160, clr.a * 255.0f) : colors::Transparent(colors::black, clr.a);
    // With box corners, we draw differently
    if ((int) box_esp == 2)
        BoxCorners(min_x, min_y, max_x, max_y, clr, (clr.a != 1.0f));
    // Otherwise, we just do simple draw funcs
    else
    {
        draw::RectangleOutlined(min_x, min_y, max_x - min_x, max_y - min_y, border, 0.5f);
        draw::RectangleOutlined(min_x + 1, min_y + 1, max_x - min_x - 2, max_y - min_y - 2, clr, 0.5f);
        draw::RectangleOutlined(min_x + 2, min_y + 2, max_x - min_x - 4, max_y - min_y - 4, border, 0.5f);
    }
}

// Function to draw box corners, Used by DrawBox
void BoxCorners(int minx, int miny, int maxx, int maxy, const rgba_t &color, bool transparent)
{
    const rgba_t &black = transparent ? colors::Transparent(colors::black) : colors::black;
    const int size      = *box_corner_size;

    // Black corners
    // Top Left
    draw::Rectangle(minx, miny, size, 3, black);
    draw::Rectangle(minx, miny + 3, 3, size - 3, black);
    // Top Right
    draw::Rectangle(maxx - size + 1, miny, size, 3, black);
    draw::Rectangle(maxx - 3 + 1, miny + 3, 3, size - 3, black);
    // Bottom Left
    draw::Rectangle(minx, maxy - 3, size, 3, black);
    draw::Rectangle(minx, maxy - size, 3, size - 3, black);
    // Bottom Right
    draw::Rectangle(maxx - size + 1, maxy - 3, size, 3, black);
    draw::Rectangle(maxx - 2, maxy - size, 3, size - 3, black);

    // Colored corners
    // Top Left
    draw::Line(minx + 1, miny + 1, size - 2, 0, color, 0.5f);
    draw::Line(minx + 1, miny + 1, 0, size - 2, color, 0.5f);
    // Top Right
    draw::Line(maxx - 1, miny + 1, -(size - 2), 0, color, 0.5f);
    draw::Line(maxx - 1, miny + 1, 0, size - 2, color, 0.5f);
    // Bottom Left
    draw::Line(minx + 1, maxy - 2, size - 2, 0, color, 0.5f);
    draw::Line(minx + 1, maxy - 2, 0, -(size - 2), color, 0.5f);
    // Bottom Right
    draw::Line(maxx - 1, maxy - 2, -(size - 2), 0, color, 0.5f);
    draw::Line(maxx - 1, maxy - 2, 0, -(size - 2), color, 0.5f);
}

// Used for caching collidable bounds
bool GetCollide(CachedEntity *ent)
{
    PROF_SECTION(PT_esp_getcollide);

    // Null check to prevent crashing
    if (CE_INVALID(ent) || !ent->m_bAlivePlayer())
        return false;

    // Grab esp data
    ESPData &ent_data = data[ent->m_IDX];

    // If entity has cached collides, return it. Otherwise generate new bounds
    if (!ent_data.has_collide)
    {

        // Get collision center, max, and mins
        Vector origin = RAW_ENT(ent)->GetCollideable()->GetCollisionOrigin();
        // Dormant
        if (RAW_ENT(ent)->IsDormant())
        {
            auto vec = ent->m_vecDormantOrigin();
            if (!vec)
                return false;
            origin = *vec;
        }
        Vector mins = RAW_ENT(ent)->GetCollideable()->OBBMins() + origin;
        Vector maxs = RAW_ENT(ent)->GetCollideable()->OBBMaxs() + origin;

        // Create a array for storing box points
        Vector points_r[8]; // World vectors
        Vector points[8];   // Screen vectors

        // If user setting for box expnad is true, spread the max and mins
        if (esp_expand)
        {
            const float exp = *esp_expand;
            maxs.x += exp;
            maxs.y += exp;
            maxs.z += exp;
            mins.x -= exp;
            mins.y -= exp;
            mins.z -= exp;
        }

        // Create points for the box based on max and mins
        float x     = maxs.x - mins.x;
        float y     = maxs.y - mins.y;
        float z     = maxs.z - mins.z;
        points_r[0] = mins;
        points_r[1] = mins + Vector(x, 0, 0);
        points_r[2] = mins + Vector(x, y, 0);
        points_r[3] = mins + Vector(0, y, 0);
        points_r[4] = mins + Vector(0, 0, z);
        points_r[5] = mins + Vector(x, 0, z);
        points_r[6] = mins + Vector(x, y, z);
        points_r[7] = mins + Vector(0, y, z);

        // Check if any point of the box isnt on the screen
        bool success = true;
        for (int i = 0; i < 8; i++)
        {
            if (!draw::WorldToScreen(points_r[i], points[i]))
                success = false;
        }
        // If a point isnt on the screen, return here
        if (!success)
            return false;

        // Get max and min of the box using the newly created screen vector
        int max_x = -1;
        int max_y = -1;
        int min_x = 65536;
        int min_y = 65536;
        for (int i = 0; i < 8; i++)
        {
            if (points[i].x > max_x)
                max_x = points[i].x;
            if (points[i].y > max_y)
                max_y = points[i].y;
            if (points[i].x < min_x)
                min_x = points[i].x;
            if (points[i].y < min_y)
                min_y = points[i].y;
        }

        // Save the info to the esp data and notify cached that we cached info.
        ent_data.collide_max = Vector(max_x, max_y, 0);
        ent_data.collide_min = Vector(min_x, min_y, 0);
        ent_data.has_collide = true;

        return true;
    }
    else
    {
        // We already have collidable so return true.
        return true;
    }
    // Impossible error, return false
    return false;
}

// Use to add a esp string to an entity
void AddEntityString(CachedEntity *entity, const std::string &string, const rgba_t &color)
{
    ESPData &entity_data = data[entity->m_IDX];
    if (entity_data.string_count >= 15)
        return;
    entity_data.strings[entity_data.string_count].data  = string;
    entity_data.strings[entity_data.string_count].color = color;
    entity_data.string_count++;
    entity_data.needs_paint = true;
}

// Function to reset entitys strings
void ResetEntityStrings()
{
    for (auto &i : data)
    {
        i.string_count = 0;
        i.color        = colors::empty;
        i.needs_paint  = false;
    }
}

// Sets an entitys esp color
void SetEntityColor(CachedEntity *entity, const rgba_t &color)
{
    if (entity->m_IDX > 2047 || entity->m_IDX < 0)
        return;
    data[entity->m_IDX].color = color;
}

static InitRoutine init([]() {
    EC::Register(EC::CreateMove, cm, "cm_esp", EC::average);
#if ENABLE_VISUALS
    EC::Register(EC::Draw, Draw, "draw_esp", EC::average);
    Init();
#endif
});

} // namespace hacks::shared::esp
