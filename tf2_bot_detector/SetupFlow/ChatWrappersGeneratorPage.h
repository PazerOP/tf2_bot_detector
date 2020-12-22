#pragma once

#include "Config/ChatWrappers.h"
#include "ISetupFlowPage.h"

#include <mh/concurrency/future.hpp>
#include <mh/error/status.hpp>

#include <future>
#include <optional>

namespace tf2_bot_detector
{
	class ChatWrappersGeneratorPage final : public ISetupFlowPage
	{
	public:
		ValidateSettingsResult ValidateSettings(const Settings& settings) const override;
		OnDrawResult OnDraw(const DrawState& ds) override;
		void Init(const InitState& is) override;

		bool CanCommit() const override;
		void Commit(const CommitState& cs) override;

		bool WantsSetupText() const override { return false; }
		bool WantsContinueButton() const override { return false; }

		SetupFlowPage GetPage() const override { return SetupFlowPage::ChatWrappersGenerate; }

		static std::string GetChatWrapperStringToken(uint32_t token);
		static constexpr char VERIFY_CFG_FILE_NAME[] = "__tf2bd_chat_wrappers_verify.cfg";

	private:
		mh::status_reader<ChatWrappersProgress> m_Progress;

		mh::future<ChatWrappers> m_ChatWrappersGenerated;
		bool m_WasInitiallyClosed = true;
		std::optional<ChatWrappers> m_ChatWrappersLoaded;
	};
}
