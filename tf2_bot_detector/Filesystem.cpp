#include "Filesystem.h"
#include "Log.h"
#include "Platform/Platform.h"

#include <mh/text/format.hpp>
#include <mh/text/string_insertion.hpp>

#include <fstream>

using namespace tf2_bot_detector;

namespace
{
	class Filesystem final : public IFilesystem
	{
	public:
		Filesystem();

		struct FileDeleter
		{
			void operator()(FILE* file) const
			{
				fclose(file);
			}
		};

		std::filesystem::path ResolvePath(const std::filesystem::path& path, PathUsage usage) const override;
		std::vector<std::byte> ReadFile(std::filesystem::path path) const override;
		void WriteFile(std::filesystem::path path, const void* begin, const void* end) const override;

	private:
		static constexpr char NON_PORTABLE_MARKER[] = ".non_portable";
		std::vector<std::filesystem::path> m_SearchPaths;
		bool m_IsPortable;

		bool CheckIsPortable() const;
		std::filesystem::path CreateAppDataPath() const;
		std::filesystem::path ChooseMutableDataPath() const;
		std::vector<std::filesystem::path> CreateSearchPaths() const;

		const std::filesystem::path m_ExeDir = Platform::GetCurrentExeDir();
		const std::filesystem::path m_WorkingDir = std::filesystem::current_path();
		const std::filesystem::path m_AppDataDir = Platform::GetAppDataDir() / "TF2 Bot Detector";
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
		m_SearchPaths.insert(m_SearchPaths.begin(), m_AppDataDir);
		m_IsPortable = false;
	}
	else
	{
		m_IsPortable = true;
	}

	{
		std::string initMsg = "Filesystem initialized. Search paths:";
		for (const auto& searchPath : m_SearchPaths)
			initMsg << "\n\t" << searchPath;

		DebugLog(std::move(initMsg));
	}
}
catch (const std::exception& e)
{
	LogFatalException(MH_SOURCE_LOCATION_CURRENT(), e, "Failed to initialize filesystem");
}

std::filesystem::path Filesystem::ResolvePath(const std::filesystem::path& path, PathUsage usage) const
{
	if (path.is_absolute())
		return path;

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
		if (!m_IsPortable)
			return m_AppDataDir / path;

		return m_WorkingDir / path;
	}
	else
	{
		throw std::invalid_argument(mh::format("{}: Unknown PathUsage value {}", __FUNCTION__, int(usage)));
	}
}

std::vector<std::byte> Filesystem::ReadFile(std::filesystem::path path) const
{
	path = ResolvePath(path, PathUsage::Read);
	if (path.empty())
		return {};

	std::ifstream file(path, std::ios::binary);
	if (!file.good())
		throw std::runtime_error(mh::format("{}: Failed to open file {}", __FUNCTION__, path));

	file.seekg(0, std::ios::end);
	if (!file.good())
		throw std::runtime_error(mh::format("{}: Failed to seek to end of {}", __FUNCTION__, path));

	const auto length = file.tellg();
	file.seekg(0, std::ios::beg);
	if (!file.good())
		throw std::runtime_error(mh::format("{}: Failed to seek to beginning of {}", __FUNCTION__, path));

	std::vector<std::byte> retVal;
	retVal.resize(length);
	file.read(reinterpret_cast<char*>(retVal.data()), length);
	if (!file.good())
		throw std::runtime_error(mh::format("{}: Failed to read file {}", __FUNCTION__, path));

	return retVal;
}

void Filesystem::WriteFile(std::filesystem::path path, const void* begin, const void* end) const
{
	path = ResolvePath(path, PathUsage::Write);

	std::ofstream file(path, std::ios::binary | std::ios::trunc);
	if (!file.good())
		throw std::runtime_error(mh::format("{}: Failed to open file {}", __FUNCTION__, path));

	const auto bytes = uintptr_t(end) - uintptr_t(begin);
	file.write(reinterpret_cast<const char*>(begin), bytes);
	if (!file.good())
		throw std::runtime_error(mh::format("{}: Failed to write {} bytes to {}", __FUNCTION__, bytes, path));
}

std::vector<std::filesystem::path> Filesystem::CreateSearchPaths() const try
{
	std::vector<std::filesystem::path> retVal;

	const auto exeDir = Platform::GetCurrentExeDir();
	retVal.insert(retVal.begin(), exeDir);

	if (auto workingDir = std::filesystem::current_path(); workingDir != exeDir)
		retVal.insert(retVal.begin(), std::move(workingDir));

	if (!ResolvePath(std::filesystem::path("cfg") / NON_PORTABLE_MARKER, PathUsage::Read).empty())
		retVal.insert(retVal.begin(), Platform::GetAppDataDir() / "TF2 Bot Detector");

	return retVal;
}
catch (const std::exception& e)
{
	LogFatalException(MH_SOURCE_LOCATION_CURRENT(), e);
}
