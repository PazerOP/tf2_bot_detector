#pragma once

#include "reclasses.hpp"
#include "copypasted/CSignature.h"
#include "e8call.hpp"

namespace re
{

class C_BaseAnimating
{
public:
    inline static void InvalidateBoneCache(IClientEntity *self)
    {
        typedef int (*InvalidateBoneCache_t)(IClientEntity *);
        static uintptr_t addr                            = gSignatures.GetClientSignature("55 8B 0D ? ? ? ? 89 E5 8B 45 ? 8D 51");
        static InvalidateBoneCache_t InvalidateBoneCache = InvalidateBoneCache_t(addr);
        InvalidateBoneCache(self);
    }
    inline static bool Interpolate(IClientEntity *self, float time)
    {
        typedef bool (*fn_t)(IClientEntity *, float);
        return vfunc<fn_t>(self, offsets::PlatformOffset(143, offsets::undefined, 143), 0)(self, time);
    }
};
} // namespace re
