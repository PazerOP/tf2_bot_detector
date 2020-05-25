#pragma once

#include <filesystem>

namespace tf2_bot_detector
{
	enum class ValidateTFDirectoryResult
	{
		Valid,

		DoesNotExist,     // Directory doesn't exist
		NotADirectory,    // Not a directory
		InvalidContents,  // Contents don't match what we expect
	};
	[[nodiscard]] ValidateTFDirectoryResult ValidateTFDirectory(std::filesystem::path path);
}
