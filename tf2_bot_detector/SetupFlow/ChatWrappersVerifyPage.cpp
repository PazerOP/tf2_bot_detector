#include "ChatWrappersVerifyPage.h"
#include "ChatWrappersGeneratorPage.h"
#include "Config/ChatWrappers.h"
#include "Config/Settings.h"
#include "ImGui_TF2BotDetector.h"

#include <mh/future.hpp>
#include <mh/text/string_insertion.hpp>
#include <mh/text/stringops.hpp>
#include <srcon/async_client.h>

using namespace std::string_literals;
using namespace tf2_bot_detector;

bool ChatWrappersVerifyPage::ValidateSettings(const Settings& settings) const
{
	return m_Validation.valid();
}

auto ChatWrappersVerifyPage::OnDraw(const DrawState& ds) -> OnDrawResult
{
	if (!m_Validation.valid())
	{
		if (m_Message.empty())
		{
			m_Message = "Validating chat wrappers...";
			m_MessageColor = { 1, 1, 1, 1 };
		}

		const auto curTime = clock_t::now();
		if (curTime >= m_NextValidationTime)
		{
			m_Validation = ds.m_Settings->m_Unsaved.m_RCONClient->send_command_async(
				"exec "s << ChatWrappersGeneratorPage::VERIFY_CFG_FILE_NAME, false);
			m_NextValidationTime = curTime + VALIDATION_INTERVAL;
		}
	}
	else if (mh::is_future_ready(m_Validation))
	{
		try
		{
			auto val = m_Validation.get();
			val = mh::trim(std::move(val));

			const auto developerPrefix = "execing "s << ChatWrappersGeneratorPage::VERIFY_CFG_FILE_NAME;
			if (val.starts_with(developerPrefix))
			{
				val.erase(0, developerPrefix.size());
				val = mh::trim(std::move(val));
			}

			if (val != m_ExpectedToken)
			{
				m_Message = ""s <<
					"Failed to validate chat wrappers: \n"
					"\tExpected token : " << std::quoted(m_ExpectedToken) << "\n"
					"\tActual token     " << std::quoted(val);

				m_MessageColor = { 1, 0.5, 0, 1 };
				m_Validation = {};
			}
		}
		catch (const std::exception& e)
		{
			m_Message = "Failed to validate chat wrappers: "s << typeid(e).name() << ": " << e.what();
			m_MessageColor = { 1, 0.25, 0.25, 1 };
			m_Validation = {};
		}
	}

	ImGui::TextColoredUnformatted(m_MessageColor, m_Message);

	ImGui::NewLine();
	if (AutoLaunchTF2Checkbox(ds.m_Settings->m_AutoLaunchTF2))
		ds.m_Settings->SaveFile();

	if (mh::is_future_ready(m_Validation))
		return OnDrawResult::EndDrawing;

	return OnDrawResult::ContinueDrawing;
}

void ChatWrappersVerifyPage::Init(const Settings& settings)
{
	m_Validation = {};
	m_ExpectedToken = ChatWrappersGeneratorPage::GetChatWrapperStringToken(settings.m_Unsaved.m_ChatMsgWrappersToken);
}

bool ChatWrappersVerifyPage::CanCommit() const
{
	return true;
}

void ChatWrappersVerifyPage::Commit(Settings& settings)
{
}
