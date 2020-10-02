#include "Update_MSIX.h"
#include "Common.h"

#include <mh/source_location.hpp>
#include <mh/text/codecvt.hpp>
#include <mh/text/format.hpp>

#include <iostream>

#include <Windows.h>
#undef max
#undef min

#include <winrt/Windows.ApplicationModel.h>
#include <winrt/Windows.Management.Deployment.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>

#pragma comment(lib, "windowsapp")

using namespace tf2_bot_detector::Updater;
using namespace winrt::Windows::ApplicationModel;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Foundation::Collections;
using namespace winrt::Windows::Management::Deployment;

int tf2_bot_detector::Updater::Update_MSIX() try
{
	std::cerr << "Attempting to install via API..." << std::endl;

	const auto mainBundleURL = mh::change_encoding<wchar_t>(s_CmdLineArgs.m_SourcePath);

	std::wcerr << "Main bundle URL: " << mainBundleURL;
	const Uri uri = Uri(winrt::param::hstring(mainBundleURL));

	IVector<Uri> deps{ winrt::single_threaded_vector<Uri>() };

	unsigned bits = 86;
	{
		SYSTEM_INFO sysInfo;
		GetNativeSystemInfo(&sysInfo);
		if (sysInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64)
			bits = 64;
	}

	const auto depURL = mh::format(L"https://tf2bd-util.pazer.us/AppInstaller/vcredist.x{}.msix", bits);
	std::wcerr << "Dependency URL: " << depURL << std::endl;
	deps.Append(Uri(depURL));

	PackageManager mgr;

	std::cerr << "Starting async update..." << std::endl;
	auto task = mgr.AddPackageAsync(uri, deps, DeploymentOptions::ForceApplicationShutdown);

	std::cerr << "Waiting for results..." << std::endl;
	auto waitStatus = task.wait_for(TimeSpan::max());
	if (waitStatus != AsyncStatus::Completed)
	{
		std::wcerr << "Error encountered during async update: "
			<< task.ErrorCode() << ": " << task.get().ErrorText().c_str();
		return false;
	}
	else if (waitStatus == AsyncStatus::Completed)
	{
		std::cerr << "Update complete" << std::endl;
	}
	else
	{
		std::cerr << "Unknown update result" << std::endl;
	}

	std::cerr << "Reopening tool..." << std::endl;
	ShellExecuteA(nullptr, "open", "tf2bd:", nullptr, nullptr, SW_SHOWNORMAL);

	return 0;
}
catch (const std::exception& e)
{
	std::cerr << mh::format("Unhandled exception ({}) in {}: {}",
		typeid(e).name(), __FUNCTION__, e.what()) << std::endl;
	return 2;
}
