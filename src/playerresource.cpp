/*
 * playerresource.cpp
 *
 *  Created on: Nov 13, 2016
 *      Author: nullifiedcat
 */

#include <playerresource.h>

#include "common.hpp"

void TFPlayerResource::Update()
{
    IClientEntity *ent;

    entity = 0;
    for (int i = 0; i <= HIGHEST_ENTITY; i++)
    {
        ent = g_IEntityList->GetClientEntity(i);
        if (ent && ent->GetClientClass()->m_ClassID == RCC_PLAYERRESOURCE)
        {
            entity = i;
            return;
        }
    }
}

int TFPlayerResource::GetHealth(CachedEntity *player)
{
    IClientEntity *ent;
    int idx;
    /* :thinking */
    IF_GAME(!IsTF()) return 100;
    ent = g_IEntityList->GetClientEntity(entity);
    if (!ent || ent->GetClientClass()->m_ClassID != RCC_PLAYERRESOURCE)
        return 0;
    idx = player->m_IDX;
    if (idx >= 64 || idx < 0)
        return 0;
    return *(int *) ((unsigned) ent + netvar.m_iHealth_Resource + 4 * idx);
}

int TFPlayerResource::GetMaxHealth(CachedEntity *player)
{
    IClientEntity *ent;
    int idx;
    /* :thinking */
    IF_GAME(!IsTF()) return 100;
    ent = g_IEntityList->GetClientEntity(entity);
    if (!ent || ent->GetClientClass()->m_ClassID != RCC_PLAYERRESOURCE)
        return 0;
    idx = player->m_IDX;
    if (idx >= 64 || idx < 0)
        return 0;
    return *(int *) ((unsigned) ent + netvar.res_iMaxHealth + 4 * idx);
}

int TFPlayerResource::GetMaxBuffedHealth(CachedEntity *player)
{
    IClientEntity *ent;
    int idx;

    IF_GAME(!IsTF()) return GetMaxHealth(player);
    ent = g_IEntityList->GetClientEntity(entity);
    if (!ent || ent->GetClientClass()->m_ClassID != RCC_PLAYERRESOURCE)
        return 0;
    idx = player->m_IDX;
    if (idx >= 64 || idx < 0)
        return 0;
    return *(int *) ((unsigned) ent + netvar.res_iMaxBuffedHealth + 4 * idx);
}

int TFPlayerResource::GetTeam(int idx)
{
    IClientEntity *ent;

    if (idx >= MAX_PLAYERS || idx < 0)
        return 0;
    ent = g_IEntityList->GetClientEntity(entity);
    if (!ent || ent->GetClientClass()->m_ClassID != RCC_PLAYERRESOURCE)
        return 0;
    return *(int *) ((unsigned) ent + netvar.res_iTeam + 4 * idx);
}

int TFPlayerResource::GetScore(int idx)
{
    IClientEntity *ent;

    if (idx >= MAX_PLAYERS || idx < 1)
        return 0;
    ent = g_IEntityList->GetClientEntity(entity);
    if (!ent || ent->GetClientClass()->m_ClassID != RCC_PLAYERRESOURCE)
        return 0;
    return *(int *) ((unsigned) ent + netvar.m_iTotalScore_Resource + 4 * idx);
}

int TFPlayerResource::GetKills(int idx)
{
    IClientEntity *ent;

    if (idx >= MAX_PLAYERS || idx < 1)
        return 0;
    ent = g_IEntityList->GetClientEntity(entity);
    if (!ent || ent->GetClientClass()->m_ClassID != RCC_PLAYERRESOURCE)
        return 0;
    return *(int *) ((unsigned) ent + netvar.m_iKills_Resource + 4 * idx);
}

int TFPlayerResource::GetDeaths(int idx)
{
    IClientEntity *ent;

    if (idx >= MAX_PLAYERS || idx < 1)
        return 0;
    ent = g_IEntityList->GetClientEntity(entity);
    if (!ent || ent->GetClientClass()->m_ClassID != RCC_PLAYERRESOURCE)
        return 0;
    return *(int *) ((unsigned) ent + netvar.m_iDeaths_Resource + 4 * idx);
}

int TFPlayerResource::GetLevel(int idx)
{
    IClientEntity *ent;

    if (idx >= MAX_PLAYERS || idx < 1)
        return 0;
    ent = g_IEntityList->GetClientEntity(entity);
    if (!ent || ent->GetClientClass()->m_ClassID != RCC_PLAYERRESOURCE)
        return 0;
    return *(int *) ((unsigned) ent + netvar.m_iPlayerLevel_Resource + 4 * idx);
}

int TFPlayerResource::GetDamage(int idx)
{
    IClientEntity *ent;

    if (idx >= MAX_PLAYERS || idx < 1)
        return 0;
    ent = g_IEntityList->GetClientEntity(entity);
    if (!ent || ent->GetClientClass()->m_ClassID != RCC_PLAYERRESOURCE)
        return 0;
    return *(int *) ((unsigned) ent + netvar.m_iDamage_Resource + 4 * idx);
}
int TFPlayerResource::GetPing(int idx)
{
    IClientEntity *ent;

    if (idx >= MAX_PLAYERS || idx < 1)
        return 0;
    ent = g_IEntityList->GetClientEntity(entity);
    if (!ent || ent->GetClientClass()->m_ClassID != RCC_PLAYERRESOURCE)
        return 0;
    return *(int *) ((unsigned) ent + netvar.m_iPing_Resource + 4 * idx);
}

int TFPlayerResource::GetClass(CachedEntity *player)
{
    IClientEntity *ent;
    int idx;

    ent = g_IEntityList->GetClientEntity(entity);
    if (!ent || ent->GetClientClass()->m_ClassID != RCC_PLAYERRESOURCE)
        return 0;
    idx = player->m_IDX;
    if (idx >= MAX_PLAYERS || idx < 0)
        return 0;
    return *(int *) ((unsigned) ent + netvar.res_iPlayerClass + 4 * idx);
}

bool TFPlayerResource::isAlive(int idx)
{
    IClientEntity *ent = g_IEntityList->GetClientEntity(entity);
    if (!ent || ent->GetClientClass()->m_ClassID != RCC_PLAYERRESOURCE)
        return 0;
    if (idx >= MAX_PLAYERS || idx < 0)
        return false;
    return *(bool *) ((unsigned) ent + netvar.res_bAlive + idx);
}

int TFPlayerResource::getClass(int idx)
{
    IClientEntity *ent = g_IEntityList->GetClientEntity(entity);
    if (!ent || ent->GetClientClass()->m_ClassID != RCC_PLAYERRESOURCE)
        return 0;
    if (idx >= MAX_PLAYERS || idx < 0)
        return 0;
    return *(int *) ((unsigned) ent + netvar.res_iPlayerClass + 4 * idx);
}

int TFPlayerResource::getTeam(int idx)
{
    IClientEntity *ent = g_IEntityList->GetClientEntity(entity);
    if (!ent || ent->GetClientClass()->m_ClassID != RCC_PLAYERRESOURCE)
        return 0;
    if (idx >= MAX_PLAYERS || idx < 0)
        return 0;
    return *(int *) ((unsigned) ent + netvar.res_iTeam + 4 * idx);
}

TFPlayerResource *g_pPlayerResource{ nullptr };
