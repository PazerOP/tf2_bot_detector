#include "IPlayer.h"
#include "WorldState.h"
#include "Util/TextUtils.h"

using namespace tf2_bot_detector;

duration_t IPlayer::GetTimeSinceLastStatusUpdate() const
{
	return GetWorld().GetCurrentTime() - GetLastStatusUpdateTime();
}

std::string IPlayer::GetNameSafe() const
{
	return CollapseNewlines(GetNameUnsafe());
}
