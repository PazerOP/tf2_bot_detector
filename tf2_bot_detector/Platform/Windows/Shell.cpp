#include "../Platform.h"
#include "Log.h"
#include "TextUtils.h"

#include <mh/text/string_insertion.hpp>

#include <filesystem>
#include <memory>

#include "WindowsHelpers.h"
#include <wrl/client.h>
#include <Windows.h>
#include <ShObjIdl_core.h>
#include <ShlObj.h>

using namespace std::string_literals;
using namespace tf2_bot_detector::Windows;

namespace
{
	struct CoTaskMemFreeFn
	{
		void operator()(void* p) { CoTaskMemFree(p); }
	};
}

std::filesystem::path tf2_bot_detector::Shell::BrowseForFolderDialog()
{
	try
	{
		using Microsoft::WRL::ComPtr;
		ComPtr<IFileDialog> dialog;
		CHECK_HR(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, __uuidof(IFileDialog), (LPVOID*)dialog.ReleaseAndGetAddressOf()));

		// Configure options
		{
			FILEOPENDIALOGOPTIONS options;
			CHECK_HR(dialog->GetOptions(&options));

			options |= FOS_NOCHANGEDIR;
			options |= FOS_PICKFOLDERS;
			options |= FOS_FORCEFILESYSTEM;

			CHECK_HR(dialog->SetOptions(options));
		}

		{
			auto hr = dialog->Show(nullptr);
			if (hr == HRESULT_FROM_WIN32(ERROR_CANCELLED))
				return {};
		}

		ComPtr<IShellItem> psiResult;
		CHECK_HR(dialog->GetResult(psiResult.ReleaseAndGetAddressOf()));

		std::unique_ptr<std::remove_pointer_t<PWSTR>, CoTaskMemFreeFn> filePath;
		{
			PWSTR pszFilePath = nullptr;
			CHECK_HR(psiResult->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath));
			filePath.reset(pszFilePath);
		}

		return std::filesystem::path(filePath.get());
	}
	catch (const GetLastErrorException& e)
	{
		LogError(std::string(__FUNCTION__ "(): ") << e.what());
		return {};
	}
}

void tf2_bot_detector::Shell::OpenURL(const char* url)
{
	DebugLog("Shell opening "s << std::quoted(url));
	ShellExecuteA(NULL, "open", url, nullptr, nullptr, SW_SHOWNORMAL);
}

void tf2_bot_detector::Shell::ExploreToAndSelect(std::filesystem::path path)
{
	try
	{
		if (!path.is_absolute())
			path = std::filesystem::absolute(path);

		struct Free
		{
			void operator()(LPITEMIDLIST p) const { ILFree(p); }
		};
		using itemid_ptr = std::unique_ptr<std::remove_pointer_t<LPITEMIDLIST>, Free>;

		auto folder = std::filesystem::path(path).remove_filename();

		//itemid_ptr folderID(ILCreateFromPathW(folder.wstring().c_str()));
		itemid_ptr fileID(ILCreateFromPathW(path.wstring().c_str()));

		CHECK_HR(SHOpenFolderAndSelectItems(fileID.get(), 0, nullptr, 0));
	}
	catch (const std::exception& e)
	{
		LogError(std::string(__FUNCTION__) << "(): path = " << path << ", " << e.what());
	}
}

std::vector<std::string> tf2_bot_detector::Shell::SplitCommandLineArgs(const std::string_view& cmdline)
{
	auto cmdLineW = tf2_bot_detector::ToWC(cmdline);

	struct Free
	{
		void operator()(LPWSTR* str) const { LocalFree(str); }
	};

	using argptr_t = std::unique_ptr<std::remove_pointer_t<LPWSTR>*, Free>;

	int argc;
	argptr_t argvW(CommandLineToArgvW(cmdLineW.c_str(), &argc));

	std::vector<std::string> args;
	for (int i = 0; i < argc; i++)
		args.push_back(ToMB(argvW.get()[i]));

	return args;
}
