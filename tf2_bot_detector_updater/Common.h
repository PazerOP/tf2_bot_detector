#pragma once

#include "ReleaseChannel.h"

#include <mh/reflection/enum.hpp>

#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace tf2_bot_detector::Updater
{
	enum class UpdateType
	{
		Unknown = 0,

#ifdef _WIN32
		MSIX,
		// MSI -- maybe one day...
#endif

		Portable,
	};

	struct CmdLineArgs
	{
		bool m_PauseOnError = true;

		UpdateType m_UpdateType = UpdateType::Unknown;
		ReleaseChannel m_ReleaseChannel = ReleaseChannel::None;

		std::filesystem::path m_SourcePath;
		std::filesystem::path m_DestPath;
	};

	extern CmdLineArgs s_CmdLineArgs;
}

MH_ENUM_REFLECT_BEGIN(tf2_bot_detector::Updater::UpdateType)
	MH_ENUM_REFLECT_VALUE(Unknown)
	MH_ENUM_REFLECT_VALUE(Portable)

#ifdef _WIN32
	MH_ENUM_REFLECT_VALUE(MSIX)
#endif

MH_ENUM_REFLECT_END()
