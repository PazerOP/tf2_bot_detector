#pragma once

#include "Log.h"

#include <nlohmann/json.hpp>

#include <optional>
#include <string_view>

namespace tf2_bot_detector
{
	namespace detail
	{
		inline constexpr bool JSON_TRY_GET_TO_LOGGING = false;

		template<typename T> struct is_clearable_container : std::bool_constant<false> {};

		template<typename T, typename TAlloc>
		struct is_clearable_container<std::vector<T, TAlloc>> : std::bool_constant<true> {};
		template<typename CharT, typename Traits, typename TAlloc>
		struct is_clearable_container<std::basic_string<CharT, Traits, TAlloc>> : std::bool_constant<true> {};

		template<typename T> inline constexpr bool is_clearable_container_v = is_clearable_container<T>::value;
	}

	template<typename T, typename = std::enable_if_t<!std::is_const_v<T>>>
	void json_reset_value(T& value)
	{
		if constexpr (std::is_array_v<T>)
		{
			for (auto& val : value)
				json_reset_value(val);
		}
		else if constexpr (detail::is_clearable_container_v<T>)
		{
			value.clear();
		}
		else
		{
			value = T{};
		}
	}

	// Tries to get_to() a json value. On error/missing property, <value> is default-initialized.
	template<typename T, typename TFactory = decltype(json_reset_value<T>),
		typename = std::enable_if_t<std::is_invocable_v<TFactory, std::add_lvalue_reference_t<T>>>>
	bool try_get_to_defaulted(const nlohmann::json& j, T& value,
		const std::string_view& name, TFactory&& defaultValFunc = json_reset_value<T>)
	{
		static_assert(std::is_invocable_v<TFactory, std::add_lvalue_reference_t<T>>);

		try
		{
			if (auto found = j.find(name); found != j.end())
			{
				found->get_to(value);
				return true;
			}
			else if constexpr (detail::JSON_TRY_GET_TO_LOGGING)
			{
				DebugLog(MH_SOURCE_LOCATION_CURRENT(), "Failed to find "s << std::quoted(name));
			}
		}
		catch (const std::exception& e)
		{
			LogError(std::string(__FUNCTION__) << ": Exception when getting " << name << ": " << e.what());
		}

		defaultValFunc(value);
		return false;
	}

	template<typename T>
	bool try_get_to_defaulted(const nlohmann::json& j, T& value,
		const std::string_view& name, const T& defaultVal)
	{
		return try_get_to_defaulted(j, value, name, [&](T& v) { v = defaultVal; });
	}
	template<typename T>
	bool try_get_to_defaulted(const nlohmann::json& j, T& value,
		const std::string_view& name, T&& defaultVal)
	{
		return try_get_to_defaulted(j, value, name, [&](T& v) { v = std::move(defaultVal); });
	}

#if 0
	template<typename T>
	bool try_get_to_defaulted(const nlohmann::json& j, T& value, const std::string_view& name)
	{
		return try_get_to_default<T>(j, name, value, json_reset_value<T>);
	}
#endif

	// Tries to get_to() a json value. On error/missing property, no further modification of <value> takes place.
	template<typename T>
	bool try_get_to_noinit(const nlohmann::json& j, T& value, const std::string_view& name)
	{
		return try_get_to_defaulted<T>(j, value, name, [](T&) {});
	}

	// Tries to load keys, starting at the first value, then continuing through the initializer_list until we
	// finally successfully load a value.
	template<typename T, typename TFactory = decltype(json_reset_value<T>),
		typename = std::enable_if_t<std::is_invocable_v<TFactory, std::add_lvalue_reference_t<T>>>>
	bool try_get_to_defaulted(const nlohmann::json& j, T& value,
		const std::initializer_list<std::string_view>& names, TFactory&& defaultValFunc = json_reset_value<T>)
	{
		for (const auto& name : names)
		{
			if (try_get_to_noinit<T>(j, value, name))
				return true;
		}

		defaultValFunc(value);
		return false;
	}

	template<typename T>
	bool try_get_to_defaulted(const nlohmann::json& j, T& value,
		const std::initializer_list<std::string_view>& names, const T& defaultVal)
	{
		return try_get_to_defaulted(j, value, names, [&](T& v) { v = defaultVal; });
	}
	template<typename T>
	bool try_get_to_defaulted(const nlohmann::json& j, T& value,
		const std::initializer_list<std::string_view>& names, T&& defaultVal)
	{
		return try_get_to_defaulted(j, value, names, [&](T& v) { v = std::move(defaultVal); });
	}

	template<typename T>
	bool try_get_to_noinit(const nlohmann::json& j, T& value, const std::initializer_list<std::string_view>& names)
	{
		return try_get_to_defaulted<T>(j, value, names, [](T&) {});
	}

	template<typename T>
	bool try_get_to_defaulted(const nlohmann::json& j, std::optional<T>& value, const std::string_view& name)
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
