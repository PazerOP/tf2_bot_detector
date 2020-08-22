#pragma once

#include <cppcoro/generator.hpp>

#include <cstddef>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace tf2_bot_detector
{
	enum class PathUsage
	{
		Read,
		Write,
	};

	class IFilesystem
	{
	public:
		virtual ~IFilesystem() = default;

		static IFilesystem& Get();

		virtual cppcoro::generator<std::filesystem::path> GetSearchPaths() const = 0;

		virtual std::filesystem::path ResolvePath(const std::filesystem::path& path, PathUsage usage) const = 0;

		virtual std::filesystem::path GetMutableDataDir() const = 0;

		//virtual std::fstream OpenFile(const std::filesystem::path& path) = 0;
		virtual std::string ReadFile(std::filesystem::path path) const = 0;
		virtual void WriteFile(std::filesystem::path path, const void* begin, const void* end) const = 0;

		void WriteFile(const std::filesystem::path& path, const std::string_view& data) const
		{
			return WriteFile(path, data.data(), data.data() + data.size());
		}

		std::filesystem::path GetLogsDir() const
		{
			return GetMutableDataDir() / "logs";
		}
		std::filesystem::path GetConfigDir() const
		{
			return GetMutableDataDir() / "cfg";
		}

		bool Exists(const std::filesystem::path& path) const
		{
			return !ResolvePath(path, PathUsage::Read).empty();
		}
	};
}

template<typename CharT, typename Traits>
std::basic_ostream<CharT, Traits>& operator<<(std::basic_ostream<CharT, Traits>& os, tf2_bot_detector::PathUsage usage)
{
	using namespace tf2_bot_detector;

#undef OS_CASE
#define OS_CASE(x) case x : return os << #x

	switch (usage)
	{
		OS_CASE(PathUsage::Read);
		OS_CASE(PathUsage::Write);

	default:
		return os << "PathUsage(" << +std::underlying_type_t<PathUsage>(usage) << ')';
	}

#undef OS_CASE
}
