#pragma once

#include <filesystem>
#include <string>

namespace tf2_bot_detector::Shell
{
	std::filesystem::path BrowseForFolderDialog();
	void ExploreToAndSelect(std::filesystem::path path);

	void OpenURL(const char* url);
	inline void OpenURL(const std::string& url) { return OpenURL(url.c_str()); }
}
