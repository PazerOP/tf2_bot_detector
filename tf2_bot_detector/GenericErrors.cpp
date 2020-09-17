#include "GenericErrors.h"

using namespace tf2_bot_detector;

namespace
{
	class ErrorCategoryType final : public std::error_category
	{
	public:
		const char* name() const noexcept override { return "TF2 Bot Detector (Generic)"; }
		std::string message(int condition) const override
		{
			switch (ErrorCode(condition))
			{
			case ErrorCode::Success:
				return "Success";
			case ErrorCode::InternetConnectivityDisabled:
				return "Internet connectivity has been disabled by the user in Settings.";
			}

			return mh::format("Unknown error condition {}", condition);
		}
	};

	const ErrorCategoryType& ErrorCategory()
	{
		static const ErrorCategoryType s_Value;
		return s_Value;
	}
}

std::error_condition std::make_error_condition(tf2_bot_detector::ErrorCode e)
{
	return std::error_condition(static_cast<int>(e), ErrorCategory());
}
