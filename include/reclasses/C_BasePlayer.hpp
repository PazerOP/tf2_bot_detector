#pragma once

#include "reclasses.hpp"

namespace re
{

class C_BasePlayer : public C_BaseEntity
{
public:
    inline static CTFPlayerShared &shared_(IClientEntity *self)
    {
        return *((CTFPlayerShared *) (unsigned(self) + 6092));
    }
    inline static Vector GetEyePosition(IClientEntity *self)
    {
        typedef Vector (*fn_t)(IClientEntity *);
        return vfunc<fn_t>(self, offsets::PlatformOffset(194, offsets::undefined, 194), 0)(self);
    }
};
} // namespace re
