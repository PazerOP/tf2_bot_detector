#pragma once

#include <filesystem>

namespace tf2_bot_detector
{
	std::filesystem::path BrowseForFolderDialog();

	void OpenURL(const char* url);
	inline void OpenURL(const std::string& url) { return OpenURL(url.c_str()); }
}
