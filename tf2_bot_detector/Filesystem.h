#pragma once

#include <cstddef>
#include <filesystem>
#include <iostream>
#include <memory>
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

		virtual std::filesystem::path ResolvePath(const std::filesystem::path& path, PathUsage usage) const = 0;

		//virtual std::fstream OpenFile(const std::filesystem::path& path) = 0;
		virtual std::vector<std::byte> ReadFile(std::filesystem::path path) const = 0;
		virtual void WriteFile(std::filesystem::path path, const void* begin, const void* end) const = 0;

		void WriteFile(const std::filesystem::path& path, const std::string_view& data)
		{
			return WriteFile(path, data.data(), data.data() + data.size());
		}
	};
}
