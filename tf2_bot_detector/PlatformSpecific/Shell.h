#pragma once

#include <filesystem>

namespace tf2_bot_detector
{
	std::filesystem::path BrowseForFolderDialog();

	void OpenURL(const char* url);
}
