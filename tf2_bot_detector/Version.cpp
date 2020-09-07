#include "Version.h"

#include <mh/text/fmtstr.hpp>
#include <nlohmann/json.hpp>

using namespace tf2_bot_detector;

std::optional<Version> Version::Parse(const char* str)
{
	Version v{};
	const auto count = sscanf_s(str, "%hi.%hi.%hi.%hi", &v.m_Major, &v.m_Minor, &v.m_Patch, &v.m_Build);
	if (count < 2)
		return std::nullopt;

	return v;
}

void tf2_bot_detector::to_json(nlohmann::json& j, const Version& d)
{
	j = mh::fmtstr<128>("{}", d).view();
}
void tf2_bot_detector::from_json(const nlohmann::json& j, Version& d)
{
	d = Version::Parse(j.get<std::string>().c_str()).value();
}
