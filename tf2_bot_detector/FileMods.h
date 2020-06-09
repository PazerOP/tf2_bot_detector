#pragma once

#include <filesystem>
#include <string>
#include <utility>

namespace tf2_bot_detector
{
	std::pair<std::string, std::string> RandomizeChatWrappers(const std::filesystem::path& tfdir, size_t wrapChars = 16);
}
