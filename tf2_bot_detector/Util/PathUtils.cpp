#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING 1

#include "PathUtils.h"
#include "Log.h"

#include <mh/text/string_insertion.hpp>
#include <vdf_parser.hpp>

#include <iomanip>
#include <string>

using namespace std::string_literals;
using namespace tf2_bot_detector;

[[nodiscard]] static bool ValidateDirectory(DirectoryValidatorResult& result, const std::string_view& relative)
{
	using Result = DirectoryValidatorResult::Result;

	const auto fullPath = result.m_Path / relative;
	if (!std::filesystem::exists(fullPath))
	{
		result.m_Message = "Expected folder "s << std::quoted(relative) << " does not exist";
		result.m_Result = Result::InvalidContents;
		return false;
	}

	auto canonical = std::filesystem::canonical(fullPath);
	if (!std::filesystem::is_directory(canonical))
	{
		result.m_Message = "Expected folder "s << std::quoted(relative) << " is not a folder";
		result.m_Result = Result::InvalidContents;
		return false;
	}

	return true;
}

[[nodiscard]] static bool ValidateFile(DirectoryValidatorResult& result, const std::string_view& relative)
{
	using Result = DirectoryValidatorResult::Result;

	const auto fullPath = result.m_Path / relative;
	if (!std::filesystem::exists(fullPath))
	{
		result.m_Message = "Expected file "s << std::quoted(relative) << " does not exist";
		result.m_Result = Result::InvalidContents;
		return false;
	}

	auto canonical = std::filesystem::canonical(fullPath);
	if (!std::filesystem::is_regular_file(canonical))
	{
		result.m_Message = "Expected file "s << std::quoted(relative) << " is not a file";
		result.m_Result = Result::InvalidContents;
		return false;
	}

	return true;
}

[[nodiscard]] static bool BasicDirChecks(DirectoryValidatorResult& result)
{
	using Result = DirectoryValidatorResult::Result;

	if (!std::filesystem::exists(result.m_Path))
	{
		result.m_Result = Result::DoesNotExist;
		result.m_Message = "Path does not exist";
		return false;
	}

	result.m_Path = std::filesystem::canonical(result.m_Path);
	if (!std::filesystem::is_directory(result.m_Path))
	{
		result.m_Result = Result::NotADirectory;
		result.m_Message = "Not a folder";
		return false;
	}

	return true;
}


DirectoryValidatorResult tf2_bot_detector::ValidateTFDir(std::filesystem::path path)
{
	DirectoryValidatorResult result(std::move(path));

	using Result = DirectoryValidatorResult::Result;

	try
	{
		if (!BasicDirChecks(result))
			return result;

		if (!ValidateFile(result, "../hl2.exe") ||
			!ValidateFile(result, "tf2_misc_dir.vpk") ||
			!ValidateFile(result, "tf2_sound_misc_dir.vpk") ||
			!ValidateFile(result, "tf2_textures_dir.vpk") ||
			!ValidateDirectory(result, "custom"))
		{
			return result;
		}
	}
	catch (const std::filesystem::filesystem_error& e)
	{
		result.m_Result = Result::FilesystemError;
		result.m_Message = e.what();
	}

	return result;
}

DirectoryValidatorResult tf2_bot_detector::ValidateSteamDir(std::filesystem::path path)
{
	DirectoryValidatorResult result(std::move(path));

	using Result = DirectoryValidatorResult::Result;

	try
	{
		if (!BasicDirChecks(result))
			return result;

		if (!ValidateFile(result, "steam.exe") ||
			!ValidateFile(result, "GameOverlayUI.exe") ||
			!ValidateFile(result, "streaming_client.exe") ||
			!ValidateDirectory(result, "steamapps") ||
			!ValidateDirectory(result, "config"))
		{
			return result;
		}
	}
	catch (const std::filesystem::filesystem_error& e)
	{
		result.m_Result = Result::FilesystemError;
		result.m_Message = e.what();
	}

	return result;
}

cppcoro::generator<std::filesystem::path> tf2_bot_detector::GetSteamLibraryFolders(const std::filesystem::path& steamDir)
{
	co_yield steamDir / "steamapps";

	const auto libraryFoldersFilename = steamDir / "steamapps/libraryfolders.vdf";
	std::ifstream file(libraryFoldersFilename);
	if (!file.good())
	{
		DebugLogWarning(std::string(__FUNCTION__) << ": Failed to read " << libraryFoldersFilename);
		co_return;
	}

	auto vdf = tyti::vdf::read(file);
	for (const auto& attrib : vdf.attribs)
	{
		if (!std::all_of(attrib.first.begin(), attrib.first.end(), [](char c) { return isdigit(c); }))
			continue;

		co_yield std::filesystem::path(attrib.second) / "steamapps";
	}
}

std::filesystem::path tf2_bot_detector::FindTFDir(const std::filesystem::path& steamDir)
{
	for (const auto& libraryFolder : GetSteamLibraryFolders(steamDir))
	{
		auto tfDir = libraryFolder / "common/Team Fortress 2/tf";
		if (!ValidateTFDir(tfDir))
			continue;

		return tfDir;
	}

	LogError("Failed to find tf directory from "s << steamDir);
	return {};
}
