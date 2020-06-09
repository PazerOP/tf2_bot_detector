/*
  Created by Jenny White on 29.04.18.
  Copyright (c) 2018 nullworks. All rights reserved.
*/

#include "HookedMethods.hpp"

namespace hooked_methods
{

DEFINE_HOOKED_METHOD(GetUserCmd, CUserCmd *, IInput *this_, int sequence_number)
{
    // We need to overwrite this if crithack is on
    if (criticals::isEnabled())
        return &GetCmds(this_)[sequence_number % VERIFIED_CMD_SIZE];
    else
        return original::GetUserCmd(this_, sequence_number);
}
} // namespace hooked_methods
