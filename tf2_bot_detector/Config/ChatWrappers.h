#pragma once

#include <mh/reflection/enum.hpp>
#include <nlohmann/json_fwd.hpp>

#include <array>
#include <atomic>
#include <cassert>
#include <compare>
#include <filesystem>
#include <ostream>
#include <string>
#include <utility>

namespace tf2_bot_detector
{
	enum class ChatCategory
	{
		All,
		AllDead,
		Team,
		TeamDead,
		Spec,
		SpecTeam,
		Coach,

		COUNT,
	};

	void to_json(nlohmann::json& j, const ChatCategory& d);
	void from_json(const nlohmann::json& j, ChatCategory& d);

	inline constexpr bool IsDead(ChatCategory category)
	{
		return category == ChatCategory::AllDead || category == ChatCategory::TeamDead;
	}
	inline constexpr bool IsTeam(ChatCategory category)
	{
		return category == ChatCategory::Team || category == ChatCategory::TeamDead;
	}

	struct ChatFmtStrLengths
	{
		ChatFmtStrLengths() = default;

		ChatFmtStrLengths Max(const ChatFmtStrLengths& other) const;

		struct Type
		{
			constexpr Type() = default;
			Type(const std::string_view& str);
			constexpr Type(size_t pre, size_t sep, size_t post) :
				m_Prefix(pre), m_Separator(sep), m_Suffix(post)
			{
			}

			size_t m_Prefix{};
			size_t m_Separator{};
			size_t m_Suffix{};

			Type Max(const Type& other) const;
			size_t GetAvailableChars() const;
			size_t GetMaxWrapperLength() const;
		};

		std::array<Type, (size_t)ChatCategory::COUNT> m_Types;
	};

	struct ChatWrappers
	{
		ChatWrappers() = default;
		explicit ChatWrappers(const ChatFmtStrLengths& availableChars);

		//using wrapper_t = std::string;
		struct wrapper_t
		{
			operator const std::string& () const { return m_Narrow; }
			operator std::string& () { return m_Narrow; }
			std::string m_Narrow;
			std::u16string m_Wide;

			bool operator==(const wrapper_t& rhs) const { return m_Wide == rhs.m_Wide; }
			bool operator<(const wrapper_t& rhs) const { return m_Wide < rhs.m_Wide; }
		};

		struct WrapperPair
		{
			wrapper_t m_Start;
			wrapper_t m_End;
		};
		using wrapper_pair_t = WrapperPair;

		struct Type
		{
			wrapper_pair_t m_Full;
			wrapper_pair_t m_Name;
			wrapper_pair_t m_Message;

			ChatFmtStrLengths::Type GetLengths() const;
		};

		std::array<Type, (size_t)ChatCategory::COUNT> m_Types;
	};

	void to_json(nlohmann::json& j, const ChatWrappers::WrapperPair& d);
	void from_json(const nlohmann::json& j, ChatWrappers::WrapperPair& d);

	void to_json(nlohmann::json& j, const ChatWrappers::Type& d);
	void from_json(const nlohmann::json& j, ChatWrappers::Type& d);

	void to_json(nlohmann::json& j, const ChatWrappers& d);
	void from_json(const nlohmann::json& j, ChatWrappers& d);

	static constexpr char TF2BD_CHAT_WRAPPERS_DIR[] = "aaaaaaaaaa_loadfirst_tf2_bot_detector";

	struct ChatWrappersProgress
	{
		std::atomic<uint32_t> m_Value{};
		std::atomic<uint32_t> m_MaxValue{};
	};
	ChatWrappers RandomizeChatWrappers(const std::filesystem::path& tfdir,
		ChatWrappersProgress* progress = nullptr);
}

MH_ENUM_REFLECT_BEGIN(tf2_bot_detector::ChatCategory)
	MH_ENUM_REFLECT_VALUE(All)
	MH_ENUM_REFLECT_VALUE(AllDead)
	MH_ENUM_REFLECT_VALUE(Team)
	MH_ENUM_REFLECT_VALUE(TeamDead)
	MH_ENUM_REFLECT_VALUE(Spec)
	MH_ENUM_REFLECT_VALUE(SpecTeam)
	MH_ENUM_REFLECT_VALUE(Coach)
MH_ENUM_REFLECT_END()
