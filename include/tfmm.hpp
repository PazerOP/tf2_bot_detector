/*
 * tfmm.hpp
 *
 *  Created on: Dec 7, 2017
 *      Author: nullifiedcat
 */

#pragma once

namespace tfmm
{

void startQueue();
void startQueueStandby();
void leaveQueue();
void disconnectAndAbandon();
void abandon();
bool isMMBanned();
int getQueue();
} // namespace tfmm
