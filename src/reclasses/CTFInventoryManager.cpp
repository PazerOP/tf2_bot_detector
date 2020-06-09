/*
 * CTFInventoryManager.cpp
 *
 *  Created on: Apr 26, 2018
 *      Author: bencat07
 */
#include "common.hpp"
#include "e8call.hpp"
using namespace re;

CTFInventoryManager *CTFInventoryManager::GTFInventoryManager()
{
    typedef CTFInventoryManager *(*GTFInventoryManager_t)();
    static uintptr_t address                            = (unsigned) e8call((void *) (gSignatures.GetClientSignature("E8 ? ? ? ? 0F B6 55 0C") + 1));
    static GTFInventoryManager_t GTFInventoryManager_fn = GTFInventoryManager_t(address);
    return GTFInventoryManager_fn();
}
bool CTFInventoryManager::EquipItemInLoadout(int classid, int slot, unsigned long long uniqueid)
{
    typedef bool (*fn_t)(void *, int, int, unsigned long long);
    return vfunc<fn_t>(this, offsets::PlatformOffset(20, offsets::undefined, 20), 0)(this, classid, slot, uniqueid);
}
unsigned long long int CEconItem::uniqueid()
{
    return *((unsigned long long int *) this + 36);
}
CTFPlayerInventory *CTFInventoryManager::GTFPlayerInventory()
{
    return (CTFPlayerInventory *) (this + 268);
    /*typedef CTFPlayerInventory *(*fn_t)(void *);
    return vfunc<fn_t>(this, offsets::PlatformOffset(21, offsets::undefined, 22), 0)(this);*/
}

CEconItemView *CTFPlayerInventory::GetFirstItemOfItemDef(int id)
{
    typedef CEconItemView *(*GetFirstItemOfItemDef_t)(int16_t, void *);
    static uintptr_t address                                = (unsigned) e8call((void *) (gSignatures.GetClientSignature("E8 ? ? ? ? 85 C0 74 35 8B 55 C0") + 1));
    static GetFirstItemOfItemDef_t GetFirstItemOfItemDef_fn = GetFirstItemOfItemDef_t(address);
    return GetFirstItemOfItemDef_fn(id, this);
}

unsigned long long CEconItemView::UUID()
{
    unsigned long long value = *(unsigned long long *) ((char *) this + 56);
    auto a                   = value >> 32;
    auto b                   = value << 32;
    return b | a;
}

static CatCommand equip_debug("equip_debug", "Debug auto equip stuff", []() {
    auto invmng    = CTFInventoryManager::GTFInventoryManager();
    auto inv       = invmng->GTFPlayerInventory();
    auto item_view = inv->GetFirstItemOfItemDef(56);
    if (item_view)
    {
        logging::Info("%llu %llu", item_view->UUID());
        logging::Info("Equip item: %d", invmng->EquipItemInLoadout(tf_sniper, 0, item_view->UUID()));
    }
});
