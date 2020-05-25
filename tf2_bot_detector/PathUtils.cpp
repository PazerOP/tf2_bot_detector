#include "PathUtils.h"

using namespace tf2_bot_detector;

namespace tf2_bot_detector
{
	static bool is_file(std::filesystem::path path)
	{
		if (!std::filesystem::exists(path))
			return false;

		path = std::filesystem::canonical(path);
		return std::filesystem::is_regular_file(path);
	}
	static bool is_directory(std::filesystem::path path)
	{
		if (!std::filesystem::exists(path))
			return false;

		path = std::filesystem::canonical(path);
		return std::filesystem::is_directory(path);
	}
}

ValidateTFDirectoryResult tf2_bot_detector::ValidateTFDirectory(std::filesystem::path path)
{
	if (!std::filesystem::exists(path))
		return ValidateTFDirectoryResult::DoesNotExist;

	path = std::filesystem::canonical(path);
	if (!std::filesystem::is_directory(path))
		return ValidateTFDirectoryResult::NotADirectory;

	if (!is_file(path / "../hl2.exe"))
		return ValidateTFDirectoryResult::InvalidContents;
	if (!is_file(path / "tf2_misc_dir.vpk") || !is_file(path / "tf2_sound_misc_dir.vpk") || !is_file(path / "tf2_textures_dir.vpk"))
		return ValidateTFDirectoryResult::InvalidContents;
	if (!tf2_bot_detector::is_directory(path / "custom"))
		return ValidateTFDirectoryResult::InvalidContents;

	return ValidateTFDirectoryResult::Valid;
}
