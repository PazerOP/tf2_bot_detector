#pragma once

#include <mh/reflection/enum.hpp>

namespace tf2_bot_detector
{
	enum class ReleaseChannel
	{
		None = -1, // Don't auto update

		Public,
		Preview,
		Nightly,
	};
}

MH_ENUM_REFLECT_BEGIN(tf2_bot_detector::ReleaseChannel)
	MH_ENUM_REFLECT_VALUE(None)
	MH_ENUM_REFLECT_VALUE(Public)
	MH_ENUM_REFLECT_VALUE(Preview)
	MH_ENUM_REFLECT_VALUE(Nightly)
MH_ENUM_REFLECT_END()
