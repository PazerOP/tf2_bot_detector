#pragma once
#include "timer.hpp"
#include <map>
#include "public/mathlib/vector.h"
#include <optional>

namespace soundcache
{
std::optional<Vector> GetSoundLocation(int entid);
void cache_sound(const Vector *Origin, int source);
} // namespace soundcache
