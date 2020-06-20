#pragma once

#include <nlohmann/json.hpp>

#include <optional>
#include <string_view>

namespace tf2_bot_detector
{
	template<typename T>
	bool try_get_to(const nlohmann::json& j, const std::string_view& name, T& value)
	{
		try
		{
			if (auto found = j.find(name); found != j.end())
			{
				found->get_to(value);
				return true;
			}
		}
		catch (const std::exception& e)
		{
			LogError(std::string(__FUNCTION__) << ": Exception when getting " << name << ": " << e.what());
		}

		return false;
	}

	template<typename T>
	bool try_get_to(const nlohmann::json& j, const std::string_view& name, std::optional<T>& value)
	{
		try
		{
			if (auto found = j.find(name); found != j.end())
			{
				found->get_to(value.emplace());
				return true;
			}
		}
		catch (const std::exception& e)
		{
			LogError(std::string(__FUNCTION__) << ": Exception when getting " << name << ": " << e.what());
		}

		value.reset();
		return false;
	}
}
