/*
  Created by Jenny White on 29.04.18.
  Copyright (c) 2018 nullworks. All rights reserved.
*/

#include <EventLogging.hpp>
#include "HookedMethods.hpp"

namespace hooked_methods
{

DEFINE_HOOKED_METHOD(FireGameEvent, void, void *this_, IGameEvent *event)
{
    const char *name = event->GetName();
    if (name)
    {
#if ENABLE_VISUALS
        if (event_logging::isEnabled())
        {
            if (!strcmp(name, "player_connect_client") || !strcmp(name, "player_disconnect") || !strcmp(name, "player_team"))
            {
                return;
            }
        }
//		hacks::tf2::killstreak::fire_event(event);
#endif
    }
    original::FireGameEvent(this_, event);
}
} // namespace hooked_methods