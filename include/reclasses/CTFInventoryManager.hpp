/*
 * CTFInventoryManager.cpp
 *
 *  Created on: Apr 26, 2018
 *      Author: bencat07
 */
#pragma once

#include "common.hpp"
namespace re
{

class CEconItem
{
public:
    unsigned long long uniqueid();
};

class CEconItemView
{
public:
    unsigned long long UUID();
};

class CTFPlayerInventory
{
public:
    CTFPlayerInventory() = delete;

public:
    CEconItemView *GetFirstItemOfItemDef(int id);
};

class CTFInventoryManager
{
public:
    CTFInventoryManager() = delete;
    static CTFInventoryManager *GTFInventoryManager();
    CTFPlayerInventory *GTFPlayerInventory();

public:
    bool EquipItemInLoadout(int, int, unsigned long long);
};
} // namespace re
