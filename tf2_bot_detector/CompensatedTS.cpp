#include "CompensatedTS.h"

#include <algorithm>
#include <cassert>

using namespace std::chrono_literals;
using namespace tf2_bot_detector;

void CompensatedTS::SetRecorded(time_point_t recorded)
{
	assert(recorded.time_since_epoch() > 0s);
	m_Recorded = recorded;

	const auto now = clock_t::now();
	if (m_Snapshot && (now - *m_Snapshot) >= 1s)
		m_Snapshot.reset();

	if ((now - recorded) > 1250ms)
		m_Parsed = now;
	else
		m_Parsed = recorded;
}

void CompensatedTS::Snapshot()
{
	const auto now = clock_t::now();
	const auto extra = now - m_Parsed;
	[[maybe_unused]] const auto extraSeconds = to_seconds(extra);
	const time_point_t adjustedTS = m_Recorded.value() + extra;

	const auto error = adjustedTS - now;
	const auto errorSeconds = to_seconds(extra);
	assert(error <= 1s);

	auto newSnapshot = std::min(adjustedTS, now);

#if 0
	if (m_SnapshotUsed && m_PreviousSnapshot && newSnapshot < *m_PreviousSnapshot)
	{
		// going backwards in time!!!
		const auto delta = to_seconds(newSnapshot - *m_PreviousSnapshot);
		assert(delta >= -0.01);
		newSnapshot = *m_PreviousSnapshot;
	}
#endif

	m_Snapshot = newSnapshot;
	m_PreviousSnapshot = newSnapshot;
	m_SnapshotUsed = false;
}

time_point_t CompensatedTS::GetSnapshot() const
{
	if (m_Snapshot.has_value())
	{
		m_SnapshotUsed = true;
		return *m_Snapshot;
	}
	else
	{
		return {};
	}
}
