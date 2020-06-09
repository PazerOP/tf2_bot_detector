/*
 * CatBot.hpp
 *
 *  Created on: Dec 30, 2017
 *      Author: nullifiedcat
 */

#pragma once

#include "common.hpp"

namespace hacks::shared::catbot
{
extern Timer timer_votekicks;
bool should_ignore_player(CachedEntity *player);
void update();
void init();
void level_init();
extern settings::Boolean catbotmode;
extern settings::Boolean anti_motd;

#if ENABLE_IPC
void update_ipc_data(ipc::user_data_s &data);
#endif
} // namespace hacks::shared::catbot
