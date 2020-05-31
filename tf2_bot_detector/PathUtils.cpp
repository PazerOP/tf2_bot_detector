#include "PathUtils.h"

#include <mh/text/string_insertion.hpp>

#include <iomanip>
#include <string>

using namespace std::string_literals;
using namespace tf2_bot_detector;

TFDirectoryValidator::TFDirectoryValidator(std::filesystem::path path) :
	m_Path(path)
{
	try
	{
		[&]
		{
			if (!std::filesystem::exists(path))
			{
				m_Result = Result::DoesNotExist;
				m_Message = "Path does not exist";
				return;
			}

			path = std::filesystem::canonical(path);
			if (!std::filesystem::is_directory(path))
			{
				m_Result = Result::NotADirectory;
				m_Message = "Not a folder";
				return;
			}

			if (!ValidateFile("../hl2.exe") ||
				!ValidateFile("tf2_misc_dir.vpk") ||
				!ValidateFile("tf2_sound_misc_dir.vpk") ||
				!ValidateFile("tf2_textures_dir.vpk") ||
				!ValidateDirectory("custom"))
			{
				return;
			}

			m_Result = Result::Valid;
		}();
	}
	catch (const std::filesystem::filesystem_error& e)
	{
		m_Result = Result::FilesystemError;
		m_Message = e.what();
	}
}

bool TFDirectoryValidator::ValidateDirectory(const std::string_view& relative)
{
	const auto fullPath = m_Path / relative;
	if (!std::filesystem::exists(fullPath))
	{
		m_Message = "Expected folder "s << std::quoted(relative) << " does not exist";
		m_Result = Result::InvalidContents;
		return false;
	}

	auto canonical = std::filesystem::canonical(fullPath);
	if (!std::filesystem::is_directory(canonical))
	{
		m_Message = "Expected folder "s << std::quoted(relative) << " is not a folder";
		m_Result = Result::InvalidContents;
		return false;
	}

	return true;
}

bool TFDirectoryValidator::ValidateFile(const std::string_view& relative)
{
	const auto fullPath = m_Path / relative;
	if (!std::filesystem::exists(fullPath))
	{
		m_Message = "Expected file "s << std::quoted(relative) << " does not exist";
		m_Result = Result::InvalidContents;
		return false;
	}

	auto canonical = std::filesystem::canonical(fullPath);
	if (!std::filesystem::is_regular_file(canonical))
	{
		m_Message = "Expected file "s << std::quoted(relative) << " is not a file";
		m_Result = Result::InvalidContents;
		return false;
	}

	return true;
}
