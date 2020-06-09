/*
 * PreDataUpdate.cpp
 *
 *  Created on: Jul 27, 2018
 *      Author: bencat07
 */
#include "HookedMethods.hpp"
#include "SeedPrediction.hpp"
#include "PreDataUpdate.hpp"

namespace hooked_methods
{
static void *PreData_Original = nullptr;
void PreDataUpdate(void *_this, int ok)
{
    hacks::tf2::seedprediction::handleFireBullets((C_TEFireBullets *) _this);
    ((bool (*)(C_TEFireBullets *, int)) PreData_Original)((C_TEFireBullets *) _this, ok);
}
static void tryPatchLocalPlayerPreData()
{
    // Patching C_TEFireBullets
    void **vtable = *(void ***) (C_TEFireBullets::GTEFireBullets());
    if (vtable[offsets::PreDataUpdate()] != PreDataUpdate)
    {
        logging::Info("0x%08X, 0x%08X", unsigned(C_TEFireBullets::GTEFireBullets()), unsigned(vtable[offsets::PreDataUpdate()]));
        PreData_Original = vtable[offsets::PreDataUpdate()];
        void *page       = (void *) ((uintptr_t) vtable & ~0xFFF);
        mprotect(page, 0xFFF, PROT_READ | PROT_WRITE | PROT_EXEC);
        vtable[offsets::PreDataUpdate()] = (void *) PreDataUpdate;
        mprotect(page, 0xFFF, PROT_READ | PROT_EXEC);
    }
}
void CreateMove()
{
    if (hacks::tf2::seedprediction::predon() && CE_GOOD(LOCAL_E))
        tryPatchLocalPlayerPreData();
}
} // namespace hooked_methods
