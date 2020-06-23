#pragma once

#include <cppcoro/generator.hpp>

#include <filesystem>
#include <string_view>

namespace tf2_bot_detector
{
	struct [[nodiscard]] DirectoryValidatorResult
	{
		DirectoryValidatorResult(std::filesystem::path path) : m_Path(std::move(path)) {}

		enum class Result
		{
			Valid,

			Empty,            // Path is empty
			DoesNotExist,     // Directory doesn't exist
			NotADirectory,    // Not a directory
			InvalidContents,  // Contents don't match what we expect
			FilesystemError,  // Some sort of std::filesystem::filesystem_error exception
		} m_Result = Result::Valid;

		operator bool() const { return m_Result == Result::Valid; }

		std::filesystem::path m_Path;
		std::string m_Message;
	};

	DirectoryValidatorResult ValidateTFDir(std::filesystem::path path);
	DirectoryValidatorResult ValidateSteamDir(std::filesystem::path path);

	cppcoro::generator<std::filesystem::path> GetSteamLibraryFolders(const std::filesystem::path& steamDir);
	std::filesystem::path FindTFDir(const std::filesystem::path& steamDir);
}
