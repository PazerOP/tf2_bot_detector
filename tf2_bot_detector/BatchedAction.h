#pragma once

#include "Clock.h"
#include "Log.h"

#include <mutex>
#include <unordered_set>

namespace tf2_bot_detector
{
	template<typename TState, typename TItem, typename TResponse>
	class BatchedAction
	{
	public:
		using state_type = TState;
		using queue_collection_type = std::unordered_set<TItem>;
		using response_type = TResponse;
		using response_future_type = mh::task<response_type>;

		BatchedAction() = default;
		BatchedAction(const TState& state) : m_State(state) {}
		BatchedAction(TState&& state) : m_State(std::move(state)) {}

		bool IsQueued(const TItem& item) const
		{
			std::lock_guard lock(m_Mutex);
			return m_Queued.contains(item);
		}

		void Queue(TItem&& item)
		{
			std::lock_guard lock(m_Mutex);
			m_Queued.insert(std::move(item));
		}
		void Queue(const TItem& item)
		{
			std::lock_guard lock(m_Mutex);
			m_Queued.insert(item);
		}

		void Update()
		{
			std::invoke([&]
				{
					if (m_Queued.empty())
						return;

					if (m_ResponseFuture.valid())
						return;

					const auto curTime = clock_t::now();
					if (curTime < (m_LastUpdate + MIN_INTERVAL))
						return;

					m_LastUpdate = curTime;

					m_ResponseFuture = SendRequest(m_State, m_Queued);
				});

			if (m_ResponseFuture.is_ready())
			{
				std::lock_guard lock(m_Mutex);
				try
				{
					const auto& response = m_ResponseFuture.get();

					try
					{
						OnDataReady(m_State, response, m_Queued);
					}
					catch (const std::exception& e)
					{
						LogException(MH_SOURCE_LOCATION_CURRENT(), e, "Failed to process batched action");
					}
				}
				catch (const std::exception& e)
				{
					LogException(MH_SOURCE_LOCATION_CURRENT(), e, "Failed to get batched action future");
				}

				m_ResponseFuture = {};
			}
		}

	protected:
		virtual response_future_type SendRequest(state_type& state, queue_collection_type& collection) = 0;
		virtual void OnDataReady(state_type& state, const response_type& response, queue_collection_type& collection) = 0;

	private:
		static constexpr duration_t MIN_INTERVAL = std::chrono::seconds(5);
		state_type m_State{};
		std::recursive_mutex m_Mutex;
		queue_collection_type m_Queued;
		response_future_type m_ResponseFuture;
		time_point_t m_LastUpdate{};
	};
}
