#pragma once

#include "Clock.h"

namespace tf2_bot_detector
{
	class IAction;
	class IActionManager;

	class IActionGenerator
	{
	public:
		virtual ~IActionGenerator() = default;

		virtual bool Execute(IActionManager& manager) = 0;
	};

	class IPeriodicActionGenerator : public IActionGenerator
	{
	public:
		virtual ~IPeriodicActionGenerator() = default;

		virtual duration_t GetInterval() const = 0;
		virtual duration_t GetInitialDelay() const { return {}; }

		bool Execute(IActionManager& manager) override final;

	protected:
		[[nodiscard]] virtual bool ExecuteImpl(IActionManager& manager) = 0;

	private:
		time_point_t m_LastRunTime{};
	};

	class StatusUpdateActionGenerator final : public IPeriodicActionGenerator
	{
	public:
		duration_t GetInterval() const override;

	protected:
		bool ExecuteImpl(IActionManager& manager) override;

	private:
		bool m_NextShort = false;
		bool m_NextPing = false;
	};

	class ConfigActionGenerator final : public IPeriodicActionGenerator
	{
	public:
		duration_t GetInterval() const override;

	protected:
		bool ExecuteImpl(IActionManager& manager) override;
	};

	class LobbyDebugActionGenerator final : public IPeriodicActionGenerator
	{
	public:
		duration_t GetInterval() const override { return std::chrono::seconds(1); }

	protected:
		bool ExecuteImpl(IActionManager& manager) override;
	};
}
