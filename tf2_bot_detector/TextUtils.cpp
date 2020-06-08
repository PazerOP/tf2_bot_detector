#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING 1
#define _SILENCE_CXX20_CODECVT_FACETS_DEPRECATION_WARNING 1

#include "TextUtils.h"

#include <codecvt>
#include <fstream>

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

std::u8string tf2_bot_detector::ReadWideFile(const std::filesystem::path& filename)
{
	std::u16string wideFileData;
	{
		std::ifstream file(filename, std::ios::binary);
		file.seekg(0, std::ios::end);
		const auto length = file.tellg();

		// Skip BOM
		file.seekg(2, std::ios::beg);
		wideFileData.resize(length / 2 - 1);

		file.read(reinterpret_cast<char*>(wideFileData.data()), length);
	}
	const auto end = wideFileData.data() + wideFileData.size() - 100;

	return ToU8(wideFileData);
}

void tf2_bot_detector::WriteWideFile(const std::filesystem::path& filename, const std::u8string_view& text)
{
	std::ofstream file(filename, std::ios::binary);
	//file << '\xFE' << '\xFF'; // BOM - UTF16LE
	file << '\xFF' << '\xFE'; // BOM - UTF16LE

	const auto converted = ToU16(text);
	file.write(reinterpret_cast<const char*>(converted.data()), converted.size() * sizeof(converted[0]));
}
