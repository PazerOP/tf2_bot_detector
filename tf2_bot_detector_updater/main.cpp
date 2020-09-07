#include "Common.h"
#include "Platform/Platform.h"
#include "ReleaseChannel.h"

#include <mh/algorithm/multi_compare.hpp>
#include <mh/text/format.hpp>

#include <iomanip>
#include <iostream>
#include <string_view>

#include "Update_Portable.h"
#ifdef _WIN32
#include "Update_MSIX.h"
#endif

using namespace tf2_bot_detector;
using namespace tf2_bot_detector::Updater;

CmdLineArgs tf2_bot_detector::Updater::s_CmdLineArgs;

[[nodiscard]] static int ValidateParameters()
{
	if (s_CmdLineArgs.m_UpdateType == UpdateType::Unknown)
	{
		std::cerr << "Update type not specified. Use --update-type <type> with one of the following values:\n"
			<< std::endl;

		for (const auto& value : mh::enum_type<UpdateType>::VALUES)
		{
			if (value.value() == UpdateType::Unknown)
				continue;

			std::cerr << '\t' << value.value_name() << '\n';
		}

		std::cerr << std::flush;
		return 1;
	}

	if (mh::any_eq(s_CmdLineArgs.m_UpdateType, UpdateType::MSIX))
	{
		if (s_CmdLineArgs.m_ReleaseChannel == ReleaseChannel::None)
		{
			std::cerr << mh::format("Release channel not specified. Use --release-channel <{:v}|{:v}|{:v}>",
				mh::enum_fmt(ReleaseChannel::Public),
				mh::enum_fmt(ReleaseChannel::Preview),
				mh::enum_fmt(ReleaseChannel::Nightly))
				<< std::endl;
			return 1;
		}
	}

	if (s_CmdLineArgs.m_UpdateType == UpdateType::Portable)
	{
		if (s_CmdLineArgs.m_SourcePath.empty())
		{
			std::cerr << "Source path not specified. Use --source-path <directory>" << std::endl;
			return 1;
		}
		if (s_CmdLineArgs.m_DestPath.empty())
		{
			std::cerr << "Destination path not specified. Use --dest-path <directory>" << std::endl;
			return 1;
		}
	}

	return 0;
}

[[nodiscard]] static int MainHelper(int argc, char** argv) try
{
	for (int i = 1; i < argc; i++)
	{
		const bool hasNextArg = i < (argc - 1);
		const std::string_view arg(argv[i]);
		if (arg == "--update-type")
		{
			if (!hasNextArg)
			{
				std::cerr << "Missing argument for " << std::quoted(arg) << std::endl;
				return 1;
			}

			s_CmdLineArgs.m_UpdateType = mh::enum_type<UpdateType>::find_value(argv[i + 1]);
		}
		else if (arg == "--release-channel")
		{
			if (!hasNextArg)
			{
				std::cerr << "Missing argument for " << std::quoted(arg) << std::endl;
				return 1;
			}

			s_CmdLineArgs.m_ReleaseChannel = mh::enum_type<ReleaseChannel>::find_value(argv[i + 1]);
		}
		else if (arg == "--source-path")
		{
			if (!hasNextArg)
			{
				std::cerr << "Missing argument for " << std::quoted(arg) << std::endl;
				return 1;
			}

			s_CmdLineArgs.m_SourcePath = argv[i + 1];
		}
		else if (arg == "--dest-path")
		{
			if (!hasNextArg)
			{
				std::cerr << "Missing argument for " << std::quoted(arg) << std::endl;
				return 1;
			}

			s_CmdLineArgs.m_DestPath = argv[i + 1];
		}
		else if (arg == "--wait-pid")
		{
			if (!hasNextArg)
			{
				std::cerr << "Missing argument for " << std::quoted(arg) << std::endl;
				return 1;
			}

			int pid;
			if (sscanf_s(argv[i + 1], "%i", &pid) != 1)
			{
				std::cerr << "Unable to parse argument to " << std::quoted(arg)
					<< ' ' << std::quoted(argv[i + 1]) << " as an integer." << std::endl;
				return 1;
			}

			Platform::WaitForPIDToExit(pid);
		}
		else if (arg == "--no-pause-on-error")
		{
			s_CmdLineArgs.m_PauseOnError = false;
		}
	}

	if (auto result = ValidateParameters(); result != 0)
		return result;

	switch (s_CmdLineArgs.m_UpdateType)
	{
#ifdef _WIN32
	case UpdateType::MSIX:
		return Update_MSIX();
#endif
	case UpdateType::Portable:
		return Update_Portable();

	default:
		std::cerr << mh::format("Unhandled UpdateType {}", mh::enum_fmt(s_CmdLineArgs.m_UpdateType)) << std::endl;
		return 1;
	}
}
catch (const std::exception& e)
{
	std::cerr << "Unhandled exception: " << typeid(e).name() << ": " << e.what();
	return 1;
}

int main(int argc, char** argv)
{
	std::cerr << std::endl;
	auto result = MainHelper(argc, argv);
	std::cerr << std::endl << std::endl;

	if (result != 0 && s_CmdLineArgs.m_PauseOnError)
		system("pause");

	return result;
}

