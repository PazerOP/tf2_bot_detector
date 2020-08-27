#include "Version.h"

using namespace tf2_bot_detector;

std::optional<Version> Version::Parse(const char* str)
{
	Version v{};
	const auto count = std::sscanf(str, "%hi.%hi.%hi.%hi", &v.m_Major, &v.m_Minor, &v.m_Patch, &v.m_Build);
	if (count < 2)
		return std::nullopt;

	return v;
}
