#include "IPlayer.h"
#include "WorldState.h"
#include "Util/TextUtils.h"

using namespace tf2_bot_detector;

duration_t IPlayer::GetTimeSinceLastStatusUpdate() const
{
	return GetWorld().GetCurrentTime() - GetLastStatusUpdateTime();
}

std::optional<duration_t> IPlayer::GetEstimatedAccountAge() const
{
	if (auto timePoint = GetEstimatedAccountCreationTime())
	{
		auto age = clock_t::now() - *timePoint;

		if (age.count() < 0)
		{
			DebugLogWarning("{}: account creation time age = {}, timePoint = {}", *this, age.count(),
				timePoint->time_since_epoch().count());
			return std::nullopt;
		}

		return age;
	}

	return std::nullopt;
}

std::string IPlayer::GetNameSafe() const
{
	return CollapseNewlines(GetNameUnsafe());
}
