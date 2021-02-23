#pragma once

#include "Log.h"

#include <mh/reflection/enum.hpp>
#include <nlohmann/json.hpp>

#include <optional>
#include <string_view>

namespace std
{
	template<typename T>
	void to_json(nlohmann::json& j, const optional<T>& d)
	{
		if (d.has_value())
			j = *d;
		else
			j = nullptr;
	}

	template<typename T>
	void from_json(const nlohmann::json& j, optional<T>& d)
	{
		if (j.is_null())
			d.reset();
		else
			j.get_to<T>(d.emplace());
	}

	inline void to_json(nlohmann::json& j, const std::filesystem::path& path)
	{
		j = path.string();
	}
	inline void from_json(const nlohmann::json& j, std::filesystem::path& path)
	{
		path = (std::string)j;
	}
}

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
				DebugLog(MH_SOURCE_LOCATION_CURRENT(), "Failed to find {}", std::quoted(name));
			}
		}
		catch (...)
		{
			LogException(MH_SOURCE_LOCATION_CURRENT(), "Exception when getting {}", std::quoted(name));
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

	/// <summary>
	/// Tries to get a value with the given name from json, otherwise sets the value to the class default.
	/// </summary>
	/// <typeparam name="TClass">Type of the class holding the given member variable.</typeparam>
	/// <typeparam name="TMember">The type of the member variable.</typeparam>
	/// <param name="j">The json to read data from.</param>
	/// <param name="obj">The object containing the member variable that will be written to.</param>
	/// <param name="memberVar">A pointer to a member variable that will be written to, and also used to read a default
	/// value from a default-initialized instance of TClass.</param>
	/// <param name="name">The name of the json variable to read.</param>
	/// <returns>true if the value was successfully deserialized from the json, false if there was an exception or the
	/// json did not contain a value with that name.</returns>
	template<typename TClass, typename TMember>
	bool try_get_to_defaulted(const nlohmann::json& j, TClass& obj, TMember(TClass::* memberVar), const std::string_view& name)
	{
		constexpr TClass DEFAULTS;

		return try_get_to_defaulted(j, obj.*memberVar, name, DEFAULTS.*memberVar);
	}
	template<typename TClass, typename TMember>
	bool try_get_to_defaulted(const nlohmann::json& j, TClass& obj, TMember(TClass::* memberVar),
		const std::initializer_list<std::string_view>& names)
	{
		constexpr TClass DEFAULTS;

		return try_get_to_defaulted(j, obj.*memberVar, names, DEFAULTS.*memberVar);
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
				if (!found->is_null())
					found->get_to(value.emplace());
				else
					value.reset();

				return true;
			}
		}
		catch (...)
		{
			LogException(MH_SOURCE_LOCATION_CURRENT(), "Exception when getting {}", std::quoted(name));
		}

		value.reset();
		return false;
	}
}

MH_ENUM_REFLECT_BEGIN(nlohmann::json::value_t)
	MH_ENUM_REFLECT_VALUE(null)
	MH_ENUM_REFLECT_VALUE(object)
	MH_ENUM_REFLECT_VALUE(array)
	MH_ENUM_REFLECT_VALUE(string)
	MH_ENUM_REFLECT_VALUE(boolean)
	MH_ENUM_REFLECT_VALUE(number_integer)
	MH_ENUM_REFLECT_VALUE(number_unsigned)
	MH_ENUM_REFLECT_VALUE(number_float)
	MH_ENUM_REFLECT_VALUE(binary)
	MH_ENUM_REFLECT_VALUE(discarded)
MH_ENUM_REFLECT_END()
