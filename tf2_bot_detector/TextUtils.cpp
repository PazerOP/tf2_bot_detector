#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING 1
#define _SILENCE_CXX20_CODECVT_FACETS_DEPRECATION_WARNING 1

#include "TextUtils.h"
#include "Log.h"

#include <mh/text/string_insertion.hpp>

#include <codecvt>
#include <fstream>

using namespace std::string_literals;

std::u16string tf2_bot_detector::ToU16(const std::u8string_view& input)
{
	std::wstring_convert<std::codecvt<char16_t, char, std::mbstate_t>, char16_t> converter;
	return converter.from_bytes(reinterpret_cast<const char*>(input.data()),
		reinterpret_cast<const char*>(input.data()) + input.size());
}

std::u16string tf2_bot_detector::ToU16(const char* input, const char* input_end)
{
	if (input_end)
		return ToU16(std::string_view(input, input_end - input));
	else
		return ToU16(std::string_view(input));
}

std::u16string tf2_bot_detector::ToU16(const std::string_view& input)
{
	return ToU16(std::u8string_view(reinterpret_cast<const char8_t*>(input.data()), input.size()));
}

std::u16string tf2_bot_detector::ToU16(const std::wstring_view& input)
{
	return std::u16string(std::u16string_view(reinterpret_cast<const char16_t*>(input.data()), input.size()));
}

std::u8string tf2_bot_detector::ToU8(const std::string_view& input)
{
	return std::u8string(std::u8string_view(reinterpret_cast<const char8_t*>(input.data()), input.size()));
}

std::u8string tf2_bot_detector::ToU8(const std::u16string_view& input)
{
	std::wstring_convert<std::codecvt<char16_t, char, std::mbstate_t>, char16_t> converter;
	return ToU8(converter.to_bytes(input.data(), input.data() + input.size()));
}

std::u8string tf2_bot_detector::ToU8(const std::wstring_view& input)
{
	return ToU8(std::u16string_view(reinterpret_cast<const char16_t*>(input.data()), input.size()));
}

std::string tf2_bot_detector::ToMB(const std::u8string_view& input)
{
	return std::string(std::string_view(reinterpret_cast<const char*>(input.data()), input.size()));
}

std::string tf2_bot_detector::ToMB(const std::u16string_view& input)
{
	return ToMB(ToU8(input));
}

std::string tf2_bot_detector::ToMB(const std::wstring_view& input)
{
	return ToMB(ToU16(input));
}

std::wstring tf2_bot_detector::ToWC(const std::string_view& input)
{
	std::wstring_convert<std::codecvt<wchar_t,char, std::mbstate_t>> converter;
	return converter.from_bytes(input.data(), input.data() + input.size());
}

std::u16string tf2_bot_detector::ReadWideFile(const std::filesystem::path& filename)
{
	//DebugLog("ReadWideFile("s << filename << ')');

	std::u16string wideFileData;
	{
		std::ifstream file(filename, std::ios::binary);
		if (!file.good())
			return {};

		file.seekg(0, std::ios::end);
		if (!file.good())
			return {};

		const auto length = static_cast<size_t>(file.tellg());

		// Skip BOM
		file.seekg(2, std::ios::beg);
		if (!file.good())
			return {};

		wideFileData.resize(length / 2 - 1);

		file.read(reinterpret_cast<char*>(wideFileData.data()), length);
		if (file.bad())
			return {};
	}

	return wideFileData;
}

void tf2_bot_detector::WriteWideFile(const std::filesystem::path& filename, const std::u16string_view& text)
{
	std::ofstream file(filename, std::ios::binary);
	file << '\xFF' << '\xFE'; // BOM - UTF16LE

	file.write(reinterpret_cast<const char*>(text.data()), text.size() * sizeof(text[0]));
}

std::string tf2_bot_detector::CollapseNewlines(const std::string_view& input)
{
	std::string retVal;

	// collapse groups of newlines in the message into red "(\n x <count>)" text
	bool firstLine = true;
	for (size_t i = 0; i < input.size(); )
	{
		size_t nonNewlineEnd = std::min(input.find('\n', i), input.size());
		retVal.append(input.substr(i, nonNewlineEnd - i));

		size_t newlineEnd = std::min(input.find_first_not_of('\n', nonNewlineEnd), input.size());

		if (newlineEnd > nonNewlineEnd)
		{
			const auto newlineCount = (newlineEnd - nonNewlineEnd);

			const auto smallGroupMsgLength = newlineCount * (std::size("\\n") - 1);

			char buf[64];
			const auto largeGroupMsgLength = sprintf_s(buf, "(\\n x %zu)", newlineCount);

			if (smallGroupMsgLength >= largeGroupMsgLength)
			{
				retVal.append(buf);
			}
			else
			{
				for (size_t n = 0; n < newlineCount; n++)
					retVal += "\\n";
			}
		}

		i = newlineEnd;
		firstLine = false;
	}

	return retVal;
}
