#pragma once

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
		AllSpec,
		TeamSpec,
		Coach,

		COUNT,
	};

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
		using wrapper_pair_t = std::pair<wrapper_t, wrapper_t>;

		struct Type
		{
			wrapper_pair_t m_Full;
			wrapper_pair_t m_Name;
			wrapper_pair_t m_Message;
		};

		Type m_Types[(int)ChatCategory::COUNT];
	};

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
	case ChatCategory::AllSpec:   return os << "ChatCategory::AllSpec";
	case ChatCategory::TeamSpec:  return os << "ChatCategory::TeamSpec";
	case ChatCategory::Coach:     return os << "ChatCategory::Coach";

	default:
		assert(!"Unknown ChatCategory");
		return os << "ChatCategory(" << +std::underlying_type_t<ChatCategory>(cat) << ')';
	}
}
