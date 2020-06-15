#pragma once

#include "Log.h"

#include <mh/text/string_insertion.hpp>

#include <chrono>
#include <future>
#include <thread>
#include <variant>

namespace tf2_bot_detector
{
	namespace detail
	{
		inline static const auto MAIN_THREAD_ID = std::this_thread::get_id();
		static bool is_main_thread() { return std::this_thread::get_id() == detail::MAIN_THREAD_ID; }
	}

	template<typename T>
	class AsyncObject final
	{
		static constexpr bool SHOW_STALLS = true;

		struct StallWarning final
		{
			using clock_t = std::chrono::high_resolution_clock;

			StallWarning(const char* funcName)
			{
				if constexpr (SHOW_STALLS)
				{
					m_FuncName = funcName;
					m_StartTime = clock_t::now();
				}
			}
			~StallWarning()
			{
				if constexpr (SHOW_STALLS)
				{
					auto delta = clock_t::now() - m_StartTime;
					using namespace std::chrono_literals;
					using namespace std::string_literals;

					if (delta > 10ms)
					{
						Log(""s << m_FuncName << ": stall on main thread of " <<
							std::chrono::duration_cast<std::chrono::milliseconds>(delta).count() << "ms");
					}
				}
			}

		private:
			const char* m_FuncName;
			clock_t::time_point m_StartTime;
		};

		using fut_t = std::future<T>;
		using sfut_t = std::shared_future<T>;
	public:
		AsyncObject() = default;
		AsyncObject(fut_t&& future) : m_Object(std::move(future)) {}
		AsyncObject(sfut_t&& future) : m_Object(std::move(future)) {}
		AsyncObject(T&& object) : m_Object(std::move(object)) {}

		bool is_valid() const
		{
			if (std::holds_alternative<std::monostate>(m_Object))
				return false;
			else if (std::holds_alternative<T>(m_Object))
				return true;
			else if (auto value = std::get_if<fut_t>(&m_Object))
				return value->valid();
			else
				return std::get<sfut_t>(m_Object).valid();
		}

		/// <summary>
		/// Checks if a result is ready (successful or not).
		/// </summary>
		bool is_ready() const
		{
			using namespace std::chrono_literals;
			return is_valid() && wait_for(0s) == std::future_status::ready;
		}

		void wait() const
		{
			const StallWarning stallWarning(__FUNCTION__);

			if (std::holds_alternative<T>(m_Object))
				return;
			else if (auto value = std::get_if<sfut_t>(&m_Object))
				return value->wait();
			else
				return std::get<fut_t>(m_Object).wait();
		}
		template<typename Rep, typename Period>
		std::future_status wait_for(std::chrono::duration<Rep, Period> duration) const
		{
			const StallWarning stallWarning(__FUNCTION__);

			if (std::holds_alternative<T>(m_Object))
				return std::future_status::ready;
			else if (auto value = std::get_if<sfut_t>(&m_Object))
				return value->wait_for(duration);
			else
				return std::get<fut_t>(m_Object).wait_for(duration);
		}
		template<typename Clock, typename Dur>
		std::future_status wait_for(std::chrono::time_point<Clock, Dur> time) const
		{
			const StallWarning stallWarning(__FUNCTION__);

			if (std::holds_alternative<T>(m_Object))
				return std::future_status::ready;
			else if (auto value = std::get_if<sfut_t>(&m_Object))
				return value->wait_until(time);
			else
				return std::get<fut_t>(m_Object).wait_until(time);
		}

		T& get()
		{
			const StallWarning stallWarning(__FUNCTION__);

			if (auto value = std::get_if<T>(&m_Object))
			{
				return *value;
			}
			else if (auto value = std::get_if<sfut_t>(&m_Object))
			{
				m_Object = std::move(value->get());
				return get();
			}
			else
			{
				m_Object = std::move(std::get<fut_t>(m_Object).get());
				return get();
			}
		}
		const T& get() const
		{
			const StallWarning stallWarning(__FUNCTION__);

			if (auto value = std::get_if<T>(&m_Object))
			{
				return *value;
			}
			else if (auto value = std::get_if<sfut_t>(&m_Object))
			{
				m_Object = std::move(value->get());
				return get();
			}
			else
			{
				m_Object = std::move(std::get<fut_t>(m_Object).get());
				return get();
			}
		}

		T& operator*() { return get(); }
		const T& operator*() const { return get(); }

		T* operator->() { return &get(); }
		const T* operator->() const { return &get(); }

	private:
		auto& get_future() { return std::get<std::shared_future<T>>(m_Object); }
		auto& get_future() const { return std::get<std::shared_future<T>>(m_Object); }

		mutable std::variant<std::monostate, T, std::future<T>, std::shared_future<T>> m_Object;
	};
}
