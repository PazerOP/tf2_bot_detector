#pragma once

#include <mh/reflection/enum.hpp>

#include <filesystem>

namespace tf2_bot_detector
{
	inline namespace Platform
	{
		void WaitForPIDToExit(int pid);

		enum class Arch
		{
			x86 = 86,
			x64 = 64,
		};

		Arch GetArch();

		namespace Processes
		{
			void Launch(const std::filesystem::path& executable, const std::string_view& arguments = {});
			int LaunchAndWait(const std::filesystem::path& executable, const std::string_view& arguments = {});
		}

		void RebootComputer();

		enum class UpdateSystemDependenciesResult
		{
			Success,

			RebootRequired,
		};
		UpdateSystemDependenciesResult UpdateSystemDependencies();
	}
}

MH_ENUM_REFLECT_BEGIN(tf2_bot_detector::Platform::Arch)
	MH_ENUM_REFLECT_VALUE(x86)
	MH_ENUM_REFLECT_VALUE(x64)
MH_ENUM_REFLECT_END()
