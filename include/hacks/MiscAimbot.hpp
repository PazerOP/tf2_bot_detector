#pragma once
#include "common.hpp"
namespace hacks::tf2::misc_aimbot
{
std::pair<CachedEntity *, Vector> FindBestEnt(bool teammate, bool Predict, bool zcheck, bool fov_check, float range = 1500.0f);
}
