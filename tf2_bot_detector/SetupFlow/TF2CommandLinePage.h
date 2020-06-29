#pragma once

#include "Clock.h"
#include "ISetupFlowPage.h"

#include <srcon/async_client.h>

#include <array>
#include <future>
#include <optional>
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
		static constexpr duration_t CL_UPDATE_INTERVAL = std::chrono::seconds(1);

		struct RCONClientData
		{
			RCONClientData(std::string pwd, uint16_t port);
			std::unique_ptr<srcon::async_client> m_Client;
			std::shared_future<std::string> m_Future;

			bool m_Success = false;
			[[nodiscard]] bool Update();

		private:
			std::array<float, 4> m_MessageColor;
			std::string m_Message;
		};

		struct Data
		{
			std::optional<bool> m_LastUpdateValidationSuccess;

			std::vector<std::string> m_CommandLineArgs;
			std::shared_future<std::vector<std::string>> m_CommandLineArgsFuture;
			bool m_Ready = false;

			time_point_t m_LastCLUpdate{};

			std::string m_RCONPassword;
			uint16_t m_RCONPort;
			bool m_RCONSuccess = false;
			std::optional<RCONClientData> m_TestRCONClient;

			void TryUpdateCmdlineArgs();
			bool HasUseRconCmdLineFlag() const;

		} m_Data;
	};
}
