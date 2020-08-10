#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace tf2_bot_detector
{
	std::u16string ToU16(const std::u8string_view& input);
	std::u16string ToU16(const char* input, const char* input_end = nullptr);
	std::u16string ToU16(const std::string_view& input);
	std::u16string ToU16(const std::wstring_view& input);
	std::u8string ToU8(const std::string_view& input);
	std::u8string ToU8(const std::u16string_view& input);
	std::u8string ToU8(const std::wstring_view& input);
	std::string ToMB(const std::u8string_view& input);
	std::string ToMB(const std::u16string_view& input);
	std::string ToMB(const std::wstring_view& input);
	std::wstring ToWC(const std::string_view& input);

	std::u16string ReadWideFile(const std::filesystem::path& filename);
	void WriteWideFile(const std::filesystem::path& filename, const std::u16string_view& text);

	std::string CollapseNewlines(const std::string_view& input);
}
