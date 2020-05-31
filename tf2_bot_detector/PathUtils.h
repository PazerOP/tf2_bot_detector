#pragma once

#include <filesystem>
#include <string_view>

namespace tf2_bot_detector
{
	struct TFDirectoryValidator
	{
		TFDirectoryValidator(std::filesystem::path path);

		enum class Result
		{
			Valid,

			Empty,            // Path is empty
			DoesNotExist,     // Directory doesn't exist
			NotADirectory,    // Not a directory
			InvalidContents,  // Contents don't match what we expect
			FilesystemError,  // Some sort of std::filesystem::filesystem_error exception
		} m_Result;

		std::filesystem::path m_Path;
		std::string m_Message;

		operator bool() const { return m_Result == Result::Valid; }

	private:
		[[nodiscard]] bool ValidateDirectory(const std::string_view& relative);
		[[nodiscard]] bool ValidateFile(const std::string_view& relative);
	};
}
