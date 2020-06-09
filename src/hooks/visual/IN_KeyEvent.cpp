/*
  Created by Jenny White on 29.04.18.
  Copyright (c) 2018 nullworks. All rights reserved.
*/

#include "HookedMethods.hpp"

namespace hooked_methods
{

DEFINE_HOOKED_METHOD(IN_KeyEvent, int, void *this_, int eventcode, ButtonCode_t keynum, const char *binding)
{
    return original::IN_KeyEvent(this_, eventcode, keynum, binding);
}
} // namespace hooked_methods
