/*
 * AutoJoin.hpp
 *
 *  Created on: Jul 28, 2017
 *      Author: nullifiedcat
 */

#pragma once
#include "common.hpp"
namespace hacks::shared::autojoin
{
void updateSearch();
void onShutdown();
#if !ENABLE_VISUALS
extern Timer queue_time;
#endif
} // namespace hacks::shared::autojoin
