#pragma once

#include "Clock.h"

#include <cppcoro/generator.hpp>

#include <memory>
#include <vector>

namespace tf2_bot_detector
{
	class IAction;
	class RCONActionManager;

	class IActionGenerator
	{
	public:
		virtual ~IActionGenerator() = default;

		virtual bool Execute(RCONActionManager& manager) = 0;
	};

	class IPeriodicActionGenerator : public IActionGenerator
	{
	public:
		virtual ~IPeriodicActionGenerator() = default;

		virtual duration_t GetInterval() const = 0;
		virtual duration_t GetInitialDelay() const { return {}; }

		bool Execute(RCONActionManager& manager) override final;

	protected:
		[[nodiscard]] virtual bool ExecuteImpl(RCONActionManager& manager) = 0;

	private:
		time_point_t m_LastRunTime{};
	};

	class StatusUpdateActionGenerator final : public IPeriodicActionGenerator
	{
	public:
		duration_t GetInterval() const override;

	protected:
		bool ExecuteImpl(RCONActionManager& manager) override;

	private:
		bool m_NextShort = false;
		bool m_NextPing = false;
	};

	class ConfigActionGenerator final : public IPeriodicActionGenerator
	{
	public:
		duration_t GetInterval() const override;

	protected:
		bool ExecuteImpl(RCONActionManager& manager) override;
	};

	class LobbyDebugActionGenerator final : public IPeriodicActionGenerator
	{
	public:
		duration_t GetInterval() const override { return std::chrono::seconds(1); }

	protected:
		bool ExecuteImpl(RCONActionManager& manager) override;
	};
}
