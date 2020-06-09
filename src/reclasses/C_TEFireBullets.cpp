/*
 * C_TEFireBullets.cpp
 *
 *  Created on: Jul 27, 2018
 *      Author: bencat07
 */
#include "reclasses.hpp"
#include "C_TEFireBullets.hpp"

C_TEFireBullets *C_TEFireBullets::GTEFireBullets()
{
    static uintptr_t fireaddr = (gSignatures.GetClientSignature("55 B8 ? ? ? ? 89 E5 5D C3 8D B6 00 00 "
                                                                "00 00 55 89 E5 56 53 83 EC ? C7 45") +
                                 0x2);
    C_TEFireBullets *fire     = *(C_TEFireBullets **) fireaddr;
    return fire;
}
int C_TEFireBullets::m_iSeed()
{
    return int(unsigned(this) + 0x34);
}
int C_TEFireBullets::m_iWeaponID()
{
    return int(unsigned(this) + 0x2c);
}
int C_TEFireBullets::m_iPlayer()
{
    return int(unsigned(this) + 0x10);
}
float C_TEFireBullets::m_flSpread()
{
    return float(unsigned(this) + 0x38);
}
