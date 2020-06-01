#include "Windows.h"
#include "Log.h"

#include <mh/text/string_insertion.hpp>

#include <memory>

#include <wrl/client.h>
#include <Windows.h>
#include <ShObjIdl_core.h>

namespace
{
	class GetLastErrorException final : public std::runtime_error
	{
		static std::string GetLastErrorString(HRESULT hr, const std::string_view& context, DWORD& errorCode)
		{
			errorCode = GetLastError();
			if (errorCode == 0)
				return {};

			std::string retVal;
			if (!context.empty())
				retVal << context << ": ";

			LPSTR buf = nullptr;
			const size_t size = FormatMessageA(
				FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
				nullptr, errorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&buf, 0, nullptr);

			try
			{
				retVal << std::string_view(buf, size);
			}
			catch (...)
			{
				LocalFree(buf);
				throw;
			}

			LocalFree(buf);

			return retVal;
		}

	public:
		GetLastErrorException(HRESULT hr) : GetLastErrorException(hr, std::string_view{}) {}
		GetLastErrorException(HRESULT hr, const std::string_view& msg) : std::runtime_error(GetLastErrorString(hr, msg, m_ErrorCode)) {}

		HRESULT GetResult() const { return m_Result; }
		DWORD GetErrorCode() const { return m_ErrorCode; }

	private:
		HRESULT m_Result;
		DWORD m_ErrorCode;
	};

	static bool Failed(HRESULT hr) { return FAILED(hr); }
	static bool Succeeded(HRESULT hr) { return SUCCEEDED(hr); }

#define MH_STR(x) #x

#define CHECK_HR(x) \
	if (auto hr_temp_____ = (x); Failed(hr_temp_____)) \
	{ \
		throw GetLastErrorException(hr_temp_____, MH_STR(x)); \
	}

	struct CoTaskMemFreeFn
	{
		void operator()(void* p) { CoTaskMemFree(p); }
	};
}

std::filesystem::path tf2_bot_detector::BrowseForFolderDialog()
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
