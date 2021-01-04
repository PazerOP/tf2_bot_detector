#pragma once

#include "ConfigHelpers.h"
#include "SteamID.h"

#include <mh/coroutine/generator.hpp>
#include <nlohmann/json_fwd.hpp>

#include <bitset>
#include <chrono>
#include <filesystem>
#include <future>
#include <map>
#include <optional>

namespace tf2_bot_detector
{
	class Settings;

	enum class PlayerAttribute
	{
		Cheater,
		Suspicious,
		Exploiter,
		Racist,

		COUNT,
	};

	struct PlayerAttributesList final
	{
		using bits_t = std::bitset<size_t(PlayerAttribute::COUNT)>;

		constexpr PlayerAttributesList() = default;
		explicit PlayerAttributesList(const bits_t& bits) : m_Bits(bits) {}
		PlayerAttributesList(const std::initializer_list<PlayerAttribute>& attributes);
		PlayerAttributesList(PlayerAttribute attribute);

		static constexpr size_t size() { return size_t(PlayerAttribute::COUNT); }
		bool HasAttribute(PlayerAttribute attribute) const { return m_Bits.test(size_t(attribute)); }
		bool SetAttribute(PlayerAttribute attribute, bool set = true);

		friend PlayerAttributesList operator|(const PlayerAttributesList& lhs, const PlayerAttributesList& rhs)
		{
			return PlayerAttributesList(lhs.m_Bits | rhs.m_Bits);
		}
		friend PlayerAttributesList& operator|=(PlayerAttributesList& lhs, const PlayerAttributesList& rhs)
		{
			lhs.m_Bits |= rhs.m_Bits;
			return lhs;
		}
		friend PlayerAttributesList operator&(const PlayerAttributesList& lhs, const PlayerAttributesList& rhs)
		{
			return PlayerAttributesList(lhs.m_Bits & rhs.m_Bits);
		}
		friend PlayerAttributesList& operator&=(PlayerAttributesList& lhs, const PlayerAttributesList& rhs)
		{
			lhs.m_Bits &= rhs.m_Bits;
			return lhs;
		}

		constexpr bool operator==(const PlayerAttributesList&) const = default;

		bool empty() const { return m_Bits.none(); }
		explicit operator bool() const { return m_Bits.any(); }

	private:
		bits_t m_Bits;
	};

	inline PlayerAttributesList operator|(PlayerAttribute lhs, PlayerAttribute rhs)
	{
		return PlayerAttributesList({ lhs, rhs });
	}

	struct PlayerListData
	{
		PlayerListData(const SteamID& id);
		~PlayerListData();

		constexpr SteamID GetSteamID() const { return m_SteamID; }

		PlayerAttributesList m_Attributes;

		struct LastSeen
		{
			std::chrono::system_clock::time_point m_Time;
			std::string m_PlayerName;

			static std::optional<LastSeen> Latest(
				const std::optional<LastSeen>& lhs, const std::optional<LastSeen>& rhs);

			constexpr bool operator==(const LastSeen& other) const { return m_Time == other.m_Time; }
			constexpr auto operator<=>(const LastSeen& other) const
			{
				return m_Time.time_since_epoch().count() <=> other.m_Time.time_since_epoch().count();
			}
		};
		std::optional<LastSeen> m_LastSeen;

		std::vector<nlohmann::json> m_Proof;

		bool operator==(const PlayerListData&) const;

	private:
		SteamID m_SteamID;
	};

	enum class ModifyPlayerResult
	{
		NoChanges,
		FileSaved,
	};

	enum class ModifyPlayerAction
	{
		NoChanges,
		Modified,
	};

	template<typename T> concept ModifyPlayerCallback = requires(T x)
	{
#ifndef __INTELLISENSE__
		std::invocable<T, PlayerListData&>;
		{ x(std::declval<PlayerListData>()) } -> std::same_as<ModifyPlayerAction>;
#endif
	};

	using ConfigFileName = std::string;
	struct PlayerMarks final
	{
		struct Mark final
		{
			Mark(const PlayerAttributesList& attr, const ConfigFileName& fileName) :
				m_Attributes(attr), m_FileName(fileName)
			{
			}

			PlayerAttributesList m_Attributes;
			ConfigFileName m_FileName;
		};

		bool Has(const PlayerAttributesList& attr) const;

		bool empty() const { return m_Marks.empty(); }
		explicit operator bool() const { return !empty(); }
		bool operator!() const { return empty(); }

		auto begin() { return m_Marks.begin(); }
		auto end() { return m_Marks.end(); }
		auto begin() const { return m_Marks.begin(); }
		auto end() const { return m_Marks.end(); }
		std::vector<Mark> m_Marks;
	};

	class PlayerListJSON final
	{
	public:
		PlayerListJSON(const Settings& settings);

		bool LoadFiles();
		void SaveFiles() const;

		mh::generator<std::pair<const ConfigFileName&, const PlayerListData&>>
			FindPlayerData(const SteamID& id) const;
		mh::generator<std::pair<const ConfigFileName&, const PlayerAttributesList&>>
			FindPlayerAttributes(const SteamID& id) const;
		PlayerMarks GetPlayerAttributes(const SteamID& id) const;
		PlayerMarks HasPlayerAttributes(const SteamID& id, const PlayerAttributesList& attributes) const;

		ModifyPlayerResult ModifyPlayer(const SteamID& id,
			const std::function<ModifyPlayerAction(PlayerListData& data)>& func);

		size_t GetPlayerCount() const { return m_CFGGroup.size(); }

	private:
		const Settings* m_Settings = nullptr;

		ModifyPlayerAction OnPlayerDataChanged(PlayerListData& data);

		using PlayerMap_t = std::map<SteamID, PlayerListData>;

		struct PlayerListFile final : public SharedConfigFileBase
		{
			void ValidateSchema(const ConfigSchemaInfo& schema) const override;
			void Deserialize(const nlohmann::json& json) override;
			void Serialize(nlohmann::json& json) const override;

			size_t size() const { return m_Players.size(); }

			PlayerListData& GetOrAddPlayer(const SteamID& id);

			PlayerMap_t m_Players;
		};

		static constexpr int PLAYERLIST_SCHEMA_VERSION = 3;

		struct ConfigFileGroup final : ConfigFileGroupBase<PlayerListFile, std::vector<std::pair<ConfigFileName, PlayerMap_t>>>
		{
			using BaseClass = ConfigFileGroupBase;

			using ConfigFileGroupBase::ConfigFileGroupBase;
			void CombineEntries(BaseClass::collection_type& map, const PlayerListFile& file) const override;
			std::string GetBaseFileName() const override { return "playerlist"; }

		} m_CFGGroup;
	};

	void to_json(nlohmann::json& j, const PlayerAttribute& d);
	void from_json(const nlohmann::json& j, PlayerAttribute& d);
}

MH_ENUM_REFLECT_BEGIN(tf2_bot_detector::PlayerAttribute)
	MH_ENUM_REFLECT_VALUE(Cheater)
	MH_ENUM_REFLECT_VALUE(Exploiter)
	MH_ENUM_REFLECT_VALUE(Racist)
	MH_ENUM_REFLECT_VALUE(Suspicious)
MH_ENUM_REFLECT_END()

template<typename CharT>
struct mh::formatter<tf2_bot_detector::PlayerAttributesList, CharT>
{
	constexpr auto parse(basic_format_parse_context<CharT>& ctx) const noexcept { return ctx.begin(); }

	template<typename FormatContext>
	auto format(const tf2_bot_detector::PlayerAttributesList& list, FormatContext& ctx)
	{
		bool printed = false;
		auto it = ctx.out();
		for (size_t i = 0; i < list.size(); i++)
		{
			const auto thisAttr = tf2_bot_detector::PlayerAttribute(i);
			if (!list.HasAttribute(thisAttr))
				continue;

			if (printed)
				it = mh::format_to(it, MH_FMT_STRING(", "));

			it = mh::format_to(it, MH_FMT_STRING("{}"), mh::enum_fmt(thisAttr));
			printed = true;
		}

		return it;
	}
};

template<typename CharT>
struct mh::formatter<tf2_bot_detector::PlayerMarks::Mark, CharT>
{
	constexpr auto parse(basic_format_parse_context<CharT>& ctx) const noexcept { return ctx.begin(); }

	template<typename FormatContext>
	auto format(const tf2_bot_detector::PlayerMarks::Mark& mark, FormatContext& ctx)
	{
		return mh::format_to(ctx.out(), MH_FMT_STRING("{} ({})"), std::quoted(mark.m_FileName), mark.m_Attributes);
	}
};

template<typename CharT>
struct mh::formatter<tf2_bot_detector::PlayerMarks, CharT>
{
	constexpr auto parse(basic_format_parse_context<CharT>& ctx) const noexcept { return ctx.begin(); }

	template<typename FormatContext>
	auto format(const tf2_bot_detector::PlayerMarks& marks, FormatContext& ctx) const
	{
		auto it = ctx.out();

		for (auto& mark : marks)
			it = mh::format_to(it, MH_FMT_STRING("\n\t - {}"), mark);

		return it;
	}
};
