/*
 * hooks.cpp
 *
 *  Created on: Oct 4, 2016
 *      Author: nullifiedcat
 */

#include "common.hpp"

#include <stdlib.h>
#include <string.h>

namespace hooks
{

unsigned CountMethods(method_table_t table)
{
    unsigned int i = 0;
    while (table[i++])
        ;
    return i;
}

std::vector<VMTHook *> hooks;

void ReleaseAllHooks()
{
    for (auto hook : hooks)
    {
        hook->Release();
    }
}

table_ref_t GetVMT(ptr_t inst, uint32_t offset)
{
    return *reinterpret_cast<table_ptr_t>((uint32_t) inst + offset);
}

bool VMTHook::IsHooked(ptr_t inst)
{
    return GetVMT(inst, 0) == &vtable_hooked[2];
}

VMTHook::VMTHook()
{
    static_assert(ptr_size == 4, "Pointer size must be DWORD.");
    hooks.push_back(this);
};

VMTHook::~VMTHook()
{
    Release();
}

void VMTHook::Set(ptr_t inst, uint32_t offset)
{
    Release();
    vtable_ptr      = &GetVMT(inst, offset);
    vtable_original = *vtable_ptr;
    int mc          = CountMethods(vtable_original);
    logging::Info("Hooking vtable 0x%08x with %d methods", vtable_original, mc);
    vtable_hooked = static_cast<method_table_t>(calloc(mc + 2, sizeof(ptr_t)));
    memcpy(&vtable_hooked[0], &vtable_original[-2], sizeof(ptr_t) * (mc + 2));
}

void VMTHook::Release()
{
    if (vtable_ptr && *vtable_ptr == &vtable_hooked[2])
    {
        logging::Info("Un-hooking 0x%08x (vtable @ 0x%08x)", vtable_ptr, *vtable_ptr);
        *vtable_ptr = vtable_original;
        free(vtable_hooked);
        vtable_ptr      = nullptr;
        vtable_hooked   = nullptr;
        vtable_original = nullptr;
    }
}

void *VMTHook::GetMethod(uint32_t idx) const
{
    return vtable_original[idx];
}

void VMTHook::HookMethod(ptr_t func, uint32_t idx, ptr_t *backup)
{
    logging::Info("Hooking method %d of vtable 0x%08x, replacing 0x%08x with 0x%08x", idx, vtable_original, GetMethod(idx), func);
    if (backup)
        *backup = vtable_hooked[2 + idx];
    vtable_hooked[2 + idx] = func;
}

void VMTHook::Apply()
{
    *vtable_ptr = &vtable_hooked[2];
}

VMTHook input{};
VMTHook localbaseent{};
VMTHook steamfriends{};
VMTHook baseclientstate{};
VMTHook baseclientstate8{};
VMTHook clientmode{};
VMTHook panel{};
VMTHook prediction{};
VMTHook client{};
VMTHook inventory{};
VMTHook chathud{};
VMTHook engine{};
VMTHook demoplayer{};
VMTHook netchannel{};
VMTHook firebullets{};
VMTHook clientdll{};
VMTHook matsurface{};
VMTHook studiorender{};
VMTHook modelrender{};
VMTHook clientmode4{};
VMTHook materialsystem{};
VMTHook soundclient{};
VMTHook enginevgui{};
VMTHook vstd{};
VMTHook eventmanager2{};
} // namespace hooks
