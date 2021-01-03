#include "Update_Portable.h"
#include "Platform/Platform.h"
#include "Common.h"

#include <mh/reflection/enum.hpp>
#include <mh/text/format.hpp>
#include <mh/text/formatters/error_code.hpp>

#include <exception>
#include <filesystem>
#include <iostream>

#ifdef _WIN32
#define CPPHTTPLIB_OPENSSL_SUPPORT 1
#include <httplib.h>
#include <Windows.h>
#include <fstream>

MH_ENUM_REFLECT_BEGIN(httplib::Error)
	MH_ENUM_REFLECT_VALUE(Success)
	MH_ENUM_REFLECT_VALUE(Unknown)
	MH_ENUM_REFLECT_VALUE(Connection)
	MH_ENUM_REFLECT_VALUE(BindIPAddress)
	MH_ENUM_REFLECT_VALUE(Read)
	MH_ENUM_REFLECT_VALUE(Write)
	MH_ENUM_REFLECT_VALUE(ExceedRedirectCount)
	MH_ENUM_REFLECT_VALUE(Canceled)
	MH_ENUM_REFLECT_VALUE(SSLConnection)
	MH_ENUM_REFLECT_VALUE(SSLLoadingCerts)
	MH_ENUM_REFLECT_VALUE(SSLServerVerification)
	MH_ENUM_REFLECT_VALUE(UnsupportedMultipartBoundaryChars)
MH_ENUM_REFLECT_END()

static bool UpdateVCRedist() try
{
	auto arch = tf2_bot_detector::Platform::GetArch();
	const auto filename = mh::format(MH_FMT_STRING("vc_redist.{:v}.exe"), mh::enum_fmt(arch));
	std::cerr << mh::format(MH_FMT_STRING("Downloading latest vcredist from https://aka.ms/vs/16/release/{}..."), filename) << std::endl;

	httplib::SSLClient downloader("aka.ms", 443);
	downloader.set_follow_location(true);

	httplib::Headers headers =
	{
		{ "User-Agent", "curl/7.58.0" }
	};

	httplib::Result result = downloader.Get(mh::format(MH_FMT_STRING("/vs/16/release/{}"), filename).c_str(), headers);

	if (!result)
		throw std::runtime_error(mh::format(MH_FMT_STRING("Failed to download latest vcredist: {}"), mh::enum_fmt(result.error())));

	auto savePath = std::filesystem::temp_directory_path() / "TF2 Bot Detector" / "Updater_VCRedist";
	std::cerr << "Creating " << savePath << "..." << std::endl;
	std::filesystem::create_directories(savePath);

	savePath /= filename;
	std::cerr << "Saving latest vcredist to " << savePath << "..." << std::endl;
	{
		std::ofstream file;
		file.exceptions(std::ios::badbit | std::ios::failbit);
		file.open(savePath, std::ios::binary);
		file.write(result->body.c_str(), result->body.size());
	}

	std::cerr << "Installing latest vcredist..." << std::endl;
	auto vcRedistResult = tf2_bot_detector::Platform::Processes::LaunchAndWait(savePath, "/install /passive /norestart");
	const auto vcRedistErrorCode = std::error_code(vcRedistResult, std::system_category());

	switch (vcRedistResult)
	{
	case ERROR_SUCCESS:
		std::cerr << "vcredist install successful." << std::endl;
		break;

	case ERROR_PRODUCT_VERSION:
		std::cerr << "vcredist already installed." << std::endl;
		break;

	case ERROR_SUCCESS_REBOOT_REQUIRED:
	{
		const auto result = MessageBoxA(nullptr,
			"TF2 Bot Detector Updater has successfully installed the latest Microsoft Visual C++ Redistributable. Some programs (including TF2 Bot Detector) might not work properly until you restart your computer.\n\nWould you like to restart your computer now?",
			"Reboot Recommended", MB_YESNO | MB_ICONINFORMATION);

		return result == IDYES;
	}

	default:
		MessageBoxA(nullptr,
			mh::format("TF2 Bot Detector attempted to install the latest Microsoft Visual C++ Redistributable, but there was an error:\n\n{}\n\nTF2 Bot Detector might not work correctly. If possible, please report this error on the TF2 Bot Detector discord server.", vcRedistErrorCode).c_str(), "Unknown Error", MB_OK | MB_ICONWARNING);
		break;
	}

	std::cerr << "Finished installing latest vcredist.\n\n" << std::flush;
	return false;
}
catch (const std::exception& e)
{
	std::cerr << mh::format(MH_FMT_STRING("Unhandled exception ({}) in {}: {}"),
		typeid(e).name(), __FUNCTION__, e.what()) << std::endl;
	throw;
}
#endif

int tf2_bot_detector::Updater::Update_Portable() try
{
	bool rebootRequired = false;
#ifdef _WIN32
	rebootRequired = UpdateVCRedist();
#endif

	std::cerr << "Attempting to install portable version from "
		<< s_CmdLineArgs.m_SourcePath << " to " << s_CmdLineArgs.m_DestPath
		<< "..." << std::endl;

	using copy_options = std::filesystem::copy_options;
	std::filesystem::copy(s_CmdLineArgs.m_SourcePath, s_CmdLineArgs.m_DestPath,
		copy_options::recursive | copy_options::overwrite_existing);

	std::cerr << "Finished copying files. Attempting to start tool..." << std::endl;

	// FIXME linux
	if (!rebootRequired)
		Platform::Processes::Launch(s_CmdLineArgs.m_DestPath / "tf2_bot_detector.exe");

	if (rebootRequired)
		Platform::RebootComputer();

	return 0;
}
catch (const std::exception& e)
{
	std::cerr << mh::format("Unhandled exception ({}) in {}: {}",
		typeid(e).name(), __FUNCTION__, e.what()) << std::endl;
	return 3;
}
