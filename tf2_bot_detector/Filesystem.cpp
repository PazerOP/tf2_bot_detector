#include "Filesystem.h"
#include "Log.h"
#include "Platform/Platform.h"

#include <mh/text/format.hpp>
#include <mh/text/string_insertion.hpp>
#include <mh/utility.hpp>

#include <fstream>

using namespace tf2_bot_detector;

namespace
{
	class Filesystem final : public IFilesystem
	{
	public:
		Filesystem();

		mh::generator<std::filesystem::path> GetSearchPaths() const override;

		std::filesystem::path ResolvePath(const std::filesystem::path& path, PathUsage usage) const override;
		std::string ReadFile(std::filesystem::path path) const override;
		void WriteFile(std::filesystem::path path, const void* begin, const void* end) const override;

		std::filesystem::path GetMutableDataDir() const override;
		std::filesystem::path GetRealMutableDataDir() const override;
		std::filesystem::path GetRealTempDataDir() const override;

	private:
		static constexpr char NON_PORTABLE_MARKER[] = ".non_portable";
		static constexpr char APPDATA_SUBFOLDER[] = "TF2 Bot Detector";
		std::vector<std::filesystem::path> m_SearchPaths;
		bool m_IsPortable;

		const std::filesystem::path m_ExeDir = Platform::GetCurrentExeDir();
		const std::filesystem::path m_WorkingDir = std::filesystem::current_path();
		const std::filesystem::path m_AppDataDir = Platform::GetAppDataDir() / APPDATA_SUBFOLDER;
		//std::filesystem::path m_MutableDataDir = ChooseMutableDataPath();
	};
}

IFilesystem& IFilesystem::Get()
{
	static Filesystem s_Filesystem;
	return s_Filesystem;
}

Filesystem::Filesystem() try
{
	DebugLog("Initializing filesystem...");

	m_SearchPaths.insert(m_SearchPaths.begin(), m_ExeDir);

	if (m_WorkingDir != m_ExeDir)
		m_SearchPaths.insert(m_SearchPaths.begin(), m_WorkingDir);

	if (!ResolvePath(std::filesystem::path("cfg") / NON_PORTABLE_MARKER, PathUsage::Read).empty())
	{
		DebugLog("Installation detected as non-portable.");
		m_SearchPaths.insert(m_SearchPaths.begin(), m_AppDataDir);
		m_IsPortable = false;

		// If we crash, we want our working directory to be somewhere we can write to.
		if (std::filesystem::create_directories(m_AppDataDir))
			DebugLog("Created {}", m_AppDataDir);

		std::filesystem::current_path(m_AppDataDir);
		DebugLog("Set working directory to {}", m_AppDataDir);
	}
	else
	{
		DebugLog("Installation detected as portable.");
		m_IsPortable = true;
	}

	{
		std::string initMsg = "Filesystem initialized. Search paths:";
		for (const auto& searchPath : m_SearchPaths)
			initMsg << "\n\t" << searchPath;

		DebugLog(std::move(initMsg));
	}
}
catch (...)
{
	LogFatalException("Failed to initialize filesystem");
}

mh::generator<std::filesystem::path> Filesystem::GetSearchPaths() const
{
	for (auto searchPath : m_SearchPaths)
		co_yield searchPath;
}

std::filesystem::path Filesystem::ResolvePath(const std::filesystem::path& path, PathUsage usage) const
{
	if (path.is_absolute())
		return path;

	auto retVal = std::invoke([&]() -> std::filesystem::path
	{
		if (usage == PathUsage::Read)
		{
			for (const auto& searchPath : m_SearchPaths)
			{
				auto fullPath = searchPath / path;
				if (std::filesystem::exists(fullPath))
				{
					assert(fullPath.is_absolute());
					return fullPath;
				}
			}

			DebugLogWarning("Unable to find {} in any search path", path);
			return {};
		}
		else if (usage == PathUsage::Write)
		{
			return GetRealMutableDataDir() / path;
		}
		else
		{
			throw std::invalid_argument(mh::format("{}: Unknown PathUsage value {}", __FUNCTION__, int(usage)));
		}
	});

	DebugLog("ResolvePath({}, {}) -> {}", path, mh::enum_fmt(usage), retVal);
	return std::move(retVal);
}

std::string Filesystem::ReadFile(std::filesystem::path path) const try
{
	path = ResolvePath(path, PathUsage::Read);
	if (path.empty())
		throw std::filesystem::filesystem_error("ResolvePath returned an empty path.", path, make_error_code(std::errc::no_such_file_or_directory));

	std::ifstream file;
	file.exceptions(std::ios::badbit | std::ios::failbit);
	file.open(path, std::ios::binary);

	file.seekg(0, std::ios::end);

	const auto length = file.tellg();
	file.seekg(0, std::ios::beg);

	std::string retVal;
	retVal.resize(size_t(length));
	file.read(retVal.data(), length);

	return retVal;
}
catch (...)
{
	LogException(MH_SOURCE_LOCATION_CURRENT(), "Filename: {}", path);
	throw;
}

void Filesystem::WriteFile(std::filesystem::path path, const void* begin, const void* end) const try
{
	path = ResolvePath(path, PathUsage::Write);

	// Create any missing directories
	if (auto folderPath = mh::copy(path).remove_filename(); std::filesystem::create_directories(folderPath))
		DebugLog("Created one or more directories in the path {}", folderPath);

	std::ofstream file;
	file.exceptions(std::ios::badbit | std::ios::failbit);
	file.open(path, std::ios::binary | std::ios::trunc);

	const auto bytes = uintptr_t(end) - uintptr_t(begin);
	file.write(reinterpret_cast<const char*>(begin), bytes);
}
catch (...)
{
	LogException("Filename: {}", path);
	throw;
}

std::filesystem::path Filesystem::GetMutableDataDir() const
{
	if (m_IsPortable)
		return m_WorkingDir;
	else
		return m_AppDataDir;
}

std::filesystem::path Filesystem::GetRealMutableDataDir() const
{
	if (m_IsPortable)
		return m_WorkingDir;
	else
		return Platform::GetRealAppDataDir() / APPDATA_SUBFOLDER;
}

std::filesystem::path Filesystem::GetRealTempDataDir() const
{
	if (m_IsPortable)
		return m_WorkingDir / "temp";
	else
		return Platform::GetRealTempDataDir() / "TF2 Bot Detector";
}
