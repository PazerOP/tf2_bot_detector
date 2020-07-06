#pragma once

#include <nlohmann/json_fwd.hpp>

#include <array>
#include <cassert>
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

	struct ChatWrappers
	{
		explicit ChatWrappers(size_t wrapChars = 16);

		using wrapper_t = std::string;
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
	ChatWrappers RandomizeChatWrappers(const std::filesystem::path& tfdir, size_t wrapChars = 16);
}

template<typename CharT, typename Traits>
std::basic_ostream<CharT, Traits>& operator<<(std::basic_ostream<CharT, Traits>& os, tf2_bot_detector::ChatCategory cat)
{
	using tf2_bot_detector::ChatCategory;
	switch (cat)
	{
	case ChatCategory::All:       return os << "ChatCategory::All";
	case ChatCategory::AllDead:   return os << "ChatCategory::AllDead";
	case ChatCategory::Team:      return os << "ChatCategory::Team";
	case ChatCategory::TeamDead:  return os << "ChatCategory::TeamDead";
	case ChatCategory::Spec:      return os << "ChatCategory::Spec";
	case ChatCategory::SpecTeam:  return os << "ChatCategory::SpecTeam";
	case ChatCategory::Coach:     return os << "ChatCategory::Coach";

	default:
		assert(!"Unknown ChatCategory");
		return os << "ChatCategory(" << +std::underlying_type_t<ChatCategory>(cat) << ')';
	}
}
