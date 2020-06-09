//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//
//=============================================================================//
#pragma once

#ifdef _WIN32
#pragma once
#endif

constexpr int IN_ATTACK    = (1 << 0);
constexpr int IN_JUMP      = (1 << 1);
constexpr int IN_DUCK      = (1 << 2);
constexpr int IN_FORWARD   = (1 << 3);
constexpr int IN_BACK      = (1 << 4);
constexpr int IN_USE       = (1 << 5);
constexpr int IN_CANCEL    = (1 << 6);
constexpr int IN_LEFT      = (1 << 7);
constexpr int IN_RIGHT     = (1 << 8);
constexpr int IN_MOVELEFT  = (1 << 9);
constexpr int IN_MOVERIGHT = (1 << 10);
constexpr int IN_ATTACK2   = (1 << 11);
constexpr int IN_RUN       = (1 << 12);
constexpr int IN_RELOAD    = (1 << 13);
constexpr int IN_ALT1      = (1 << 14);
constexpr int IN_ALT2      = (1 << 15);
constexpr int IN_SCORE     = (1 << 16); // Used by client.dll for when scoreboard is held down
constexpr int IN_SPEED     = (1 << 17); // Player is holding the speed key
constexpr int IN_WALK      = (1 << 18); // Player holding walk key
constexpr int IN_ZOOM      = (1 << 19); // Zoom key for HUD zoom
constexpr int IN_WEAPON1   = (1 << 20); // weapon defines these bits
constexpr int IN_WEAPON2   = (1 << 21); // weapon defines these bits
constexpr int IN_BULLRUSH  = (1 << 22);
constexpr int IN_GRENADE1  = (1 << 23); // grenade 1
constexpr int IN_GRENADE2  = (1 << 24); // grenade 2
constexpr int IN_ATTACK3   = (1 << 25);
