#pragma once

#include <mh/text/string_insertion.hpp>

#include <stdexcept>
#include <string_view>

#include <Windows.h>

namespace tf2_bot_detector::Windows
{
	inline std::error_code GetLastErrorCode(DWORD lastError) { return std::error_code(lastError, std::system_category()); }
	inline std::error_code GetLastErrorCode() { return GetLastErrorCode(GetLastError()); }

	class GetLastErrorException final : public std::runtime_error
	{
		static std::string GetLastErrorString(HRESULT hr, const std::string_view& context, DWORD errorCode)
		{
			if (errorCode == 0)
				return {};

			std::string retVal;
			if (!context.empty())
				retVal << context << ": ";

			retVal << GetLastErrorCode(errorCode).message();
			return retVal;
		}

	public:
		GetLastErrorException(HRESULT hr, DWORD lastError) :
			GetLastErrorException(hr, lastError, std::string_view{}) {}
		GetLastErrorException(HRESULT hr, DWORD lastError, const std::string_view& msg) :
			std::runtime_error(GetLastErrorString(m_Result = hr, msg, m_ErrorCode = lastError)) {}

		HRESULT GetResult() const { return m_Result; }
		DWORD GetErrorCode() const { return m_ErrorCode; }

	private:
		HRESULT m_Result;
		DWORD m_ErrorCode;
	};


#define MH_STR(x) #x

#define CHECK_HR(x) \
	if (auto hr_temp_____ = (x); FAILED(hr_temp_____)) \
	{ \
		auto lastError_____ = ::GetLastError(); \
		throw ::tf2_bot_detector::Windows::GetLastErrorException(hr_temp_____, lastError_____, MH_STR(x)); \
	}

#define IUNKNOWN_QI_TYPE(type) \
	if (riid == IID_ ## type) \
	{ \
		*ppv = static_cast<type*>(this); \
		AddRef(); \
		return S_OK; \
	}
}
