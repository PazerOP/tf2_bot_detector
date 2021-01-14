#include "Update_Portable.h"
#include "Platform/Platform.h"
#include "Common.h"

#include <mh/reflection/enum.hpp>
#include <mh/text/format.hpp>
#include <mh/text/formatters/error_code.hpp>

#include <exception>
#include <filesystem>
#include <iostream>

int tf2_bot_detector::Updater::Update_Portable() try
{
	auto sysDepsResult = Platform::UpdateSystemDependencies();

	std::cerr << "Attempting to install portable version from "
		<< s_CmdLineArgs.m_SourcePath << " to " << s_CmdLineArgs.m_DestPath
		<< "..." << std::endl;

	using copy_options = std::filesystem::copy_options;
	std::filesystem::copy(s_CmdLineArgs.m_SourcePath, s_CmdLineArgs.m_DestPath,
		copy_options::recursive | copy_options::overwrite_existing);

	std::cerr << "Finished copying files. Attempting to start tool..." << std::endl;

	// FIXME linux
	if (sysDepsResult != UpdateSystemDependenciesResult::RebootRequired)
		Platform::Processes::Launch(s_CmdLineArgs.m_DestPath / "tf2_bot_detector.exe");

	if (sysDepsResult == UpdateSystemDependenciesResult::RebootRequired)
		Platform::RebootComputer();

	return 0;
}
catch (const std::exception& e)
{
	std::cerr << mh::format("Unhandled exception ({}) in {}: {}",
		typeid(e).name(), __FUNCTION__, e.what()) << std::endl;
	return 3;
}
