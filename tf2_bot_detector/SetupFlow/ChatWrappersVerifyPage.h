#pragma once

#include "Clock.h"
#include "ISetupFlowPage.h"

#include <array>
#include <future>

namespace tf2_bot_detector
{
	class ChatWrappersVerifyPage final : public ISetupFlowPage
	{
	public:
		bool ValidateSettings(const Settings& settings) const override;
		OnDrawResult OnDraw(const DrawState& ds) override;
		void Init(const Settings& settings) override;

		bool CanCommit() const override;
		void Commit(Settings& settings);

		bool WantsSetupText() const override { return false; }
		bool WantsContinueButton() const override { return false; }

	private:
		std::string m_ExpectedToken;
		std::shared_future<std::string> m_Validation;
		std::string m_Message;
		std::array<float, 4> m_MessageColor{ 1, 1, 1, 1 };
		time_point_t m_NextValidationTime{};

		static constexpr duration_t VALIDATION_INTERVAL = std::chrono::seconds(1);
	};
}
