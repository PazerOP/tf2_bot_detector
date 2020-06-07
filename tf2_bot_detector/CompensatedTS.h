#pragma once

#include "Clock.h"

#include <optional>

namespace tf2_bot_detector
{
	// A compensated timestamp. Allows time to appear to progress normally
	// (with all the sub-second precision offered by clock_t) despite the uneven
	// pacing of the output from the log file.
	struct CompensatedTS
	{
	public:
		void InvalidateRecorded() { m_Recorded.reset(); }
		bool IsRecordedValid() const { return m_Recorded.has_value(); }
		void SetRecorded(time_point_t recorded);

		void Snapshot();
		bool IsSnapshotValid() const { return m_Snapshot.has_value(); }
		time_point_t GetSnapshot() const;

	private:
		std::optional<time_point_t> m_Recorded;  // real time at instant of recording
		time_point_t m_Parsed{};                 // real time when the recorded value was read
		std::optional<time_point_t> m_Snapshot;  // compensated time when Snapshot() was called
		std::optional<time_point_t> m_PreviousSnapshot;

		mutable bool m_SnapshotUsed = false;
	};
}
