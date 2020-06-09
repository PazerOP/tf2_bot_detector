/*
 * usercmd.h
 *
 *  Created on: Oct 5, 2016
 *      Author: nullifiedcat
 */

#pragma once

#include <stdint.h>

#include <mathlib/vector.h>
#include "checksum_crc.h"

class CUserCmd
{
public:
    CUserCmd()
    {
        Reset();
    }

    void Reset()
    {
        command_number = 0;
        tick_count     = 0;
        viewangles.Init();
        forwardmove   = 0.0f;
        sidemove      = 0.0f;
        upmove        = 0.0f;
        buttons       = 0;
        impulse       = 0;
        weaponselect  = 0;
        weaponsubtype = 0;
        random_seed   = 0;
        mousedx       = 0;
        mousedy       = 0;

        hasbeenpredicted = false;
    }

    CUserCmd &operator=(const CUserCmd &src)
    {
        if (this == &src)
            return *this;

        command_number = src.command_number;
        tick_count     = src.tick_count;
        viewangles     = src.viewangles;
        forwardmove    = src.forwardmove;
        sidemove       = src.sidemove;
        upmove         = src.upmove;
        buttons        = src.buttons;
        impulse        = src.impulse;
        weaponselect   = src.weaponselect;
        weaponsubtype  = src.weaponsubtype;
        random_seed    = src.random_seed;
        mousedx        = src.mousedx;
        mousedy        = src.mousedy;

        hasbeenpredicted = src.hasbeenpredicted;

        return *this;
    }

    CUserCmd(const CUserCmd &src)
    {
        *this = src;
    }

    CRC32_t GetChecksum(void) const
    {
        CRC32_t crc;

        CRC32_Init(&crc);
        CRC32_ProcessBuffer(&crc, &command_number, sizeof(command_number));
        CRC32_ProcessBuffer(&crc, &tick_count, sizeof(tick_count));
        CRC32_ProcessBuffer(&crc, &viewangles, sizeof(viewangles));
        CRC32_ProcessBuffer(&crc, &forwardmove, sizeof(forwardmove));
        CRC32_ProcessBuffer(&crc, &sidemove, sizeof(sidemove));
        CRC32_ProcessBuffer(&crc, &upmove, sizeof(upmove));
        CRC32_ProcessBuffer(&crc, &buttons, sizeof(buttons));
        CRC32_ProcessBuffer(&crc, &impulse, sizeof(impulse));
        CRC32_ProcessBuffer(&crc, &weaponselect, sizeof(weaponselect));
        CRC32_ProcessBuffer(&crc, &weaponsubtype, sizeof(weaponsubtype));
        CRC32_ProcessBuffer(&crc, &random_seed, sizeof(random_seed));
        CRC32_ProcessBuffer(&crc, &mousedx, sizeof(mousedx));
        CRC32_ProcessBuffer(&crc, &mousedy, sizeof(mousedy));
        CRC32_Final(&crc);

        return crc;
    }

    // Allow command, but negate gameplay-affecting values
    void MakeInert(void)
    {
        viewangles.Init();
        forwardmove = 0.f;
        sidemove    = 0.f;
        upmove      = 0.f;
        buttons     = 0;
        impulse     = 0;
    }

    char pad0[4];
    // For matching server and client commands for debugging
    int command_number;

    // the tick the client created this command
    int tick_count;

    // Player instantaneous view angles.
    Vector viewangles;
    // Intended velocities
    //	forward velocity.
    float forwardmove;
    //  sideways velocity.
    float sidemove;
    //  upward velocity.
    float upmove;
    // Attack button states
    int buttons;
    // Impulse command issued.
    byte impulse;
    char pad1[3];
    // Current weapon id
    int weaponselect;
    int weaponsubtype;

    int random_seed; // For shared random functions
    short mousedx;   // mouse accum in x from create move
    short mousedy;   // mouse accum in y from create move

    // Client only, tracks whether we've predicted this command at least once
    bool hasbeenpredicted;
    char pad2[3];
} __attribute__((packed));

class CVerifiedUserCmd
{
public:
    CUserCmd m_cmd;
    CRC32_t m_crc;
} __attribute__((packed));

inline CRC32_t GetChecksum(CUserCmd *cmd)
{
    CRC32_t crc;

    CRC32_Init(&crc);
    CRC32_ProcessBuffer(&crc, &cmd->command_number, sizeof(cmd->command_number));
    CRC32_ProcessBuffer(&crc, &cmd->tick_count, sizeof(cmd->tick_count));
    CRC32_ProcessBuffer(&crc, &cmd->viewangles, sizeof(cmd->viewangles));
    CRC32_ProcessBuffer(&crc, &cmd->forwardmove, sizeof(cmd->forwardmove));
    CRC32_ProcessBuffer(&crc, &cmd->sidemove, sizeof(cmd->sidemove));
    CRC32_ProcessBuffer(&crc, &cmd->upmove, sizeof(cmd->upmove));
    CRC32_ProcessBuffer(&crc, &cmd->buttons, sizeof(cmd->buttons));
    CRC32_ProcessBuffer(&crc, &cmd->impulse, sizeof(cmd->impulse));
    CRC32_ProcessBuffer(&crc, &cmd->weaponselect, sizeof(cmd->weaponselect));
    CRC32_ProcessBuffer(&crc, &cmd->weaponsubtype, sizeof(cmd->weaponsubtype));
    CRC32_ProcessBuffer(&crc, &cmd->random_seed, sizeof(cmd->random_seed));
    CRC32_ProcessBuffer(&crc, &cmd->mousedx, sizeof(cmd->mousedx));
    CRC32_ProcessBuffer(&crc, &cmd->mousedy, sizeof(cmd->mousedy));
    CRC32_Final(&crc);

    return crc;
}

inline CUserCmd *GetCmds(void *iinput)
{
    return *(CUserCmd **) ((uintptr_t) iinput + 0xfc);
}

inline CVerifiedUserCmd *GetVerifiedCmds(void *iinput)
{
    return *(CVerifiedUserCmd **) ((uintptr_t) iinput + 0x100);
}
