/*
 * vfunc.h
 *
 *  Created on: Jan 8, 2017
 *      Author: nullifiedcat
 */

#pragma once

template <typename F> inline F vfunc(void *thisptr, uintptr_t idx, uintptr_t offset = 0)
{
    void **vmt = *reinterpret_cast<void ***>(uintptr_t(thisptr) + offset);
    return reinterpret_cast<F>((vmt)[idx]);
}