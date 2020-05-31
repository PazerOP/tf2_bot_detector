#pragma once

#include <mh/coroutine/generator.hpp>
#include <nlohmann/json_fwd.hpp>

#include <filesystem>
#include <optional>
#include <vector>

namespace tf2_bot_detector
{
	class IPlayer;
	enum class PlayerAttributes;
	class Settings;

	enum class TriggerMatchMode
	{
		MatchAll,
		MatchAny,
	};

	void to_json(nlohmann::json& j, const TriggerMatchMode& d);
	void from_json(const nlohmann::json& j, TriggerMatchMode& d);

	enum class TextMatchMode
	{
		Equal,
		Contains,
		StartsWith,
		EndsWith,
		Regex,
		Word,
	};

	void to_json(nlohmann::json& j, const TextMatchMode& d);
	void from_json(const nlohmann::json& j, TextMatchMode& d);

	struct TextMatch
	{
		TextMatchMode m_Mode;
		std::vector<std::string> m_Patterns;
		bool m_CaseSensitive = false;

		bool Match(const std::string_view& text) const;
	};

	struct ModerationRule
	{
		std::string m_Description;

		bool Match(const IPlayer& player) const;
		bool Match(const IPlayer& player, const std::string_view& chatMsg) const;

		struct Triggers
		{
			TriggerMatchMode m_Mode = TriggerMatchMode::MatchAll;

			std::optional<TextMatch> m_UsernameTextMatch;
			std::optional<TextMatch> m_ChatMsgTextMatch;
		} m_Triggers;

		struct Actions
		{
			std::vector<PlayerAttributes> m_Mark;
			std::vector<PlayerAttributes> m_Unmark;
		} m_Actions;
	};

	class ModerationRules
	{
	public:
		ModerationRules(const Settings& settings);

		bool Load();
		bool Save() const;

		mh::generator<const ModerationRule*> GetRules() const;

	private:
		const Settings* m_Settings = nullptr;
		bool IsOfficial() const;

		using RuleList_t = std::vector<ModerationRule>;
		bool LoadFile(const std::filesystem::path& filename, RuleList_t& rules) const;

		RuleList_t& GetMutableList() { return const_cast<RuleList_t&>(std::as_const(*this).GetMutableList()); }
		const RuleList_t& GetMutableList() const { return IsOfficial() ? m_OfficialRules : m_UserRules; }

		RuleList_t m_OfficialRules;
		RuleList_t m_UserRules;
		RuleList_t m_OtherRules;
	};
}
