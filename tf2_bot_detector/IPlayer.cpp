#include "IPlayer.h"
#include "WorldState.h"

using namespace tf2_bot_detector;

duration_t IPlayer::GetTimeSinceLastStatusUpdate() const
{
	return GetWorld().GetCurrentTime() - GetLastStatusUpdateTime();
}
