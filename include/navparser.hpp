#pragma once

#include <memory>
#include "mathlib/vector.h"

class CNavFile;
class CNavArea;

namespace nav
{

enum init_status : uint8_t
{
    off = 0,
    unavailable,
    initing,
    on
};

// Call prepare first and check its return value
extern std::unique_ptr<CNavFile> navfile;

// Current path priority
extern int curr_priority;
// Check if ready to recieve another NavTo (to avoid overwriting of
// instructions)
extern bool ReadyForCommands;
// Ignore. For level init only
extern std::atomic<init_status> status;

// Nav to vector
bool navTo(const Vector &destination, int priority = 5, bool should_repath = true, bool nav_to_local = true, bool is_repath = false);
// Find closest to vector area
CNavArea *findClosestNavSquare(const Vector &vec);
// Check and init navparser
bool prepare();
// Clear current path
void clearInstructions();
// Check if area is safe from stickies and sentries
bool isSafe(CNavArea *area);

} // namespace nav
