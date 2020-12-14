#pragma once
#include "ConfigHelpers.h"

#include <cppcoro/generator.hpp>
#include <mh/reflection/enum.hpp>
#include <nlohmann/json_fwd.hpp>

#include <filesystem>
#include <optional>
#include <vector>

namespace tf2_bot_detector
{
	class IPlayer;
	enum class PlayerAttribute;
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

	struct AvatarMatch
	{
		std::string m_AvatarHash;

		bool Match(const std::string_view& avatarHash) const;
	};

	void to_json(nlohmann::json& j, const AvatarMatch& d);
	void from_json(const nlohmann::json& j, AvatarMatch& d);

	struct NicknameMatch
	{
		std::string m_Nickname;

		bool Match(const std::string_view& Nickname) const;
	};

	void to_json(nlohmann::json& j, const NicknameMatch& d);
	void from_json(const nlohmann::json& j, NicknameMatch& d);

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
			std::vector<AvatarMatch> m_AvatarMatches;
			std::vector<NicknameMatch> m_NicknameMatches;
		} m_Triggers;

		struct Actions
		{
			std::vector<PlayerAttribute> m_Mark;
			std::vector<PlayerAttribute> m_Unmark;
		} m_Actions;
	};

	class ModerationRules
	{
	public:
		ModerationRules(const Settings& settings);

		bool LoadFiles();
		bool SaveFile() const;

		cppcoro::generator<const ModerationRule&> GetRules() const;
		size_t GetRuleCount() const { return m_CFGGroup.size(); }

	private:
		using RuleList_t = std::vector<ModerationRule>;
		struct RuleFile final : SharedConfigFileBase
		{
			void ValidateSchema(const ConfigSchemaInfo& schema) const override;
			void Deserialize(const nlohmann::json& json) override;
			void Serialize(nlohmann::json& json) const override;

			size_t size() const { return m_Rules.size(); }

			RuleList_t m_Rules;
		};

		static constexpr int RULES_SCHEMA_VERSION = 3;

		struct ConfigFileGroup final : ConfigFileGroupBase<RuleFile, RuleList_t>
		{
			using ConfigFileGroupBase::ConfigFileGroupBase;
			void CombineEntries(RuleList_t& list, const RuleFile& file) const override;
			std::string GetBaseFileName() const override { return "rules"; }

		} m_CFGGroup;
	};
}

MH_ENUM_REFLECT_BEGIN(tf2_bot_detector::TriggerMatchMode)
	MH_ENUM_REFLECT_VALUE(MatchAll)
	MH_ENUM_REFLECT_VALUE(MatchAny)
MH_ENUM_REFLECT_END()

MH_ENUM_REFLECT_BEGIN(tf2_bot_detector::TextMatchMode)
	MH_ENUM_REFLECT_VALUE(Equal)
	MH_ENUM_REFLECT_VALUE(Contains)
	MH_ENUM_REFLECT_VALUE(StartsWith)
	MH_ENUM_REFLECT_VALUE(EndsWith)
	MH_ENUM_REFLECT_VALUE(Regex)
	MH_ENUM_REFLECT_VALUE(Word)
MH_ENUM_REFLECT_END()
