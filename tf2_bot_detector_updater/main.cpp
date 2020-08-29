#include "Common.h"
#include "ReleaseChannel.h"

#include <mh/text/format.hpp>

#include <iostream>
#include <string_view>

#ifdef _WIN32
#include "Update_MSIX.h"
#endif

using namespace tf2_bot_detector;
using namespace tf2_bot_detector::Updater;

CmdLineArgs tf2_bot_detector::Updater::s_CmdLineArgs;

static int MainHelper(int argc, char** argv) try
{
	for (int i = 1; i < argc; i++)
	{
		const bool hasNextArg = i < (argc - 1);
		const std::string_view arg(argv[i]);
		if (arg == "--update-type")
		{
			if (!hasNextArg)
			{
				std::cerr << "Missing argument for " << arg << std::endl;
				return 1;
			}

			s_CmdLineArgs.m_UpdateType = mh::enum_type<UpdateType>::find_value(argv[i + 1]);
		}
		else if (arg == "--release-channel")
		{
			if (!hasNextArg)
			{
				std::cerr << "Missing argument for " << arg << std::endl;
				return 1;
			}

			s_CmdLineArgs.m_ReleaseChannel = mh::enum_type<ReleaseChannel>::find_value(argv[i + 1]);
		}
	}

	if (s_CmdLineArgs.m_ReleaseChannel == ReleaseChannel::None)
	{
		std::cerr << mh::format("Release channel not specified. Use --release-channel <{:v}|{:v}|{:v}>",
			ReleaseChannel::Public, ReleaseChannel::Preview, ReleaseChannel::Nightly);
		return 1;
	}

	if (s_CmdLineArgs.m_UpdateType == UpdateType::Unknown)
	{
		std::cerr << "Update type not specified. Use --update-type <type> with one of the following values:\n";

		for (const auto& value : mh::enum_type<UpdateType>::VALUES)
		{
			if (value.value() == UpdateType::Unknown)
				continue;

			std::cerr << '\t' << value.value_name() << '\n';
		}

		std::cerr << std::flush;
		return 1;
	}

	switch (s_CmdLineArgs.m_UpdateType)
	{
#ifdef _WIN32
	case UpdateType::MSIX:
		return Update_MSIX();
#endif
	}

	return 0;
}
catch (const std::exception& e)
{
	std::cerr << "Unhandled exception: " << typeid(e).name() << ": " << e.what();
	return 1;
}

int main(int argc, char** argv)
{
	std::cerr << std::endl;
	MainHelper(argc, argv);
	std::cerr << std::endl << std::endl;
}

