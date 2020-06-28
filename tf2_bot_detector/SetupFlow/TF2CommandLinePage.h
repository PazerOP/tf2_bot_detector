#pragma once

#include "Clock.h"
#include "ISetupFlowPage.h"

#include <future>
#include <string>
#include <vector>

namespace tf2_bot_detector
{
	class TF2CommandLinePage final : public ISetupFlowPage
	{
	public:
		bool ValidateSettings(const Settings& settings) const override;
		OnDrawResult OnDraw(const DrawState& ds) override;

		void Init(const Settings& settings) override;
		bool CanCommit() const override { return true; }
		void Commit(Settings& settings) override;

		bool WantsSetupText() const override { return false; }
		bool WantsContinueButton() const override { return false; }

	private:
		std::vector<std::string> m_CommandLineArgs;
		std::shared_future<std::vector<std::string>> m_CommandLineArgsFuture;
		bool m_Ready = false;

		time_point_t m_LastCLUpdate{};
		static constexpr duration_t CL_UPDATE_INTERVAL = std::chrono::seconds(1);

		void TryUpdateCmdlineArgs();
		bool HasUseRconCmdLineFlag() const;

		std::string m_RCONPassword;
		uint16_t m_RCONPort;
	};
}
