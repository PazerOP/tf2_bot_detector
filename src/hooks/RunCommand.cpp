
#include "HookedMethods.hpp"

namespace hooked_methods
{
int last_tick   = 0;
int last_weapon = 0;

// Credits to blackfire for telling me to do this :)
DEFINE_HOOKED_METHOD(RunCommand, void, IPrediction *prediction, IClientEntity *entity, CUserCmd *usercmd, IMoveHelper *move)
{
    if (CE_GOOD(LOCAL_E) && CE_GOOD(LOCAL_W) && entity && entity->entindex() == g_pLocalPlayer->entity_idx && usercmd && usercmd->command_number)
    {
        original::RunCommand(prediction, entity, usercmd, move);
        criticals::fixBucket(RAW_ENT(LOCAL_W), usercmd);
    }
    else
        return original::RunCommand(prediction, entity, usercmd, move);
}
} // namespace hooked_methods
