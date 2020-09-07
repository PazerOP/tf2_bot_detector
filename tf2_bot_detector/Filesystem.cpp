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

		cppcoro::generator<std::filesystem::path> GetSearchPaths() const override;

		std::filesystem::path ResolvePath(const std::filesystem::path& path, PathUsage usage) const override;
		std::string ReadFile(std::filesystem::path path) const override;
		void WriteFile(std::filesystem::path path, const void* begin, const void* end) const override;

		std::filesystem::path GetMutableDataDir() const override;
		std::filesystem::path GetRealMutableDataDir() const override;

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
	DebugLog(MH_SOURCE_LOCATION_CURRENT(), "Initializing filesystem...");

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
catch (const std::exception& e)
{
	LogFatalException(MH_SOURCE_LOCATION_CURRENT(), e, "Failed to initialize filesystem");
}

cppcoro::generator<std::filesystem::path> Filesystem::GetSearchPaths() const
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
			if (!m_IsPortable)
				return m_AppDataDir / path;

			return m_WorkingDir / path;
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
		return {};

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
catch (const std::exception& e)
{
	LogException(MH_SOURCE_LOCATION_CURRENT(), e, "Filename: {}", path);
	throw;
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
