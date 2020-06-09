//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//
//=====================================================================================//

#pragma once

#ifdef _WIN32
#pragma once
#endif

class KeyValues;

//------------------------------------------------------------------------------
// Simple base class for screen space post-processing effects
//------------------------------------------------------------------------------
class IScreenSpaceEffect
{
public:
    virtual void Init()     = 0;
    virtual void Shutdown() = 0;

    virtual void SetParameters(KeyValues *params) = 0;

    virtual void Render(int x, int y, int w, int h) = 0;

    virtual void Enable(bool bEnable) = 0;
    virtual bool IsEnabled()          = 0;
};

//------------------------------------------------------------------------------
// Interface class for managing screen space post-processing effects
//------------------------------------------------------------------------------
class IScreenSpaceEffectManager
{
public:
    virtual void InitScreenSpaceEffects()     = 0;
    virtual void ShutdownScreenSpaceEffects() = 0;

    virtual IScreenSpaceEffect *GetScreenSpaceEffect(const char *pEffectName) = 0;

    virtual void SetScreenSpaceEffectParams(const char *pEffectName, KeyValues *params)     = 0;
    virtual void SetScreenSpaceEffectParams(IScreenSpaceEffect *pEffect, KeyValues *params) = 0;

    virtual void EnableScreenSpaceEffect(const char *pEffectName)     = 0;
    virtual void EnableScreenSpaceEffect(IScreenSpaceEffect *pEffect) = 0;

    virtual void DisableScreenSpaceEffect(const char *pEffectName)     = 0;
    virtual void DisableScreenSpaceEffect(IScreenSpaceEffect *pEffect) = 0;

    virtual void DisableAllScreenSpaceEffects() = 0;

    virtual void RenderEffects(int x, int y, int w, int h) = 0;
};

extern IScreenSpaceEffectManager *g_pScreenSpaceEffects;

//-------------------------------------------------------------------------------------
// Registration class for adding screen space effects to the
// IScreenSpaceEffectManager
//-------------------------------------------------------------------------------------
class CScreenSpaceEffectRegistration
{
public:
    CScreenSpaceEffectRegistration(const char *pName, IScreenSpaceEffect *pEffect);

    const char *m_pEffectName;
    IScreenSpaceEffect *m_pEffect;

    CScreenSpaceEffectRegistration *m_pNext;

    // NEVER USE - CATHOOK
    static CScreenSpaceEffectRegistration *s_pHead;
};

// CATHOOK
extern CScreenSpaceEffectRegistration **g_ppScreenSpaceRegistrationHead;

#define ADD_SCREENSPACE_EFFECT(CEffect, pEffectName) \
    CEffect pEffectName##_effect;                    \
    CScreenSpaceEffectRegistration pEffectName##_reg(#pEffectName, &pEffectName##_effect);
