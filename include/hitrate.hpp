/*
 * hitrate.hpp
 *
 *  Created on: Aug 16, 2017
 *      Author: nullifiedcat
 */

#pragma once

namespace hitrate
{

extern int count_shots;
extern int count_hits;
extern int count_hits_head;

void AimbotShot(int idx, bool target_body);
void Update();
} // namespace hitrate
