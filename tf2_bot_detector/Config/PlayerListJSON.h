#pragma once

#include "ConfigHelpers.h"
#include "SteamID.h"

#include <cppcoro/generator.hpp>
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

		bool empty() const { return m_Bits.none(); }
		operator bool() const { return m_Bits.any(); }

	private:
		bits_t m_Bits;
	};

	struct PlayerListData
	{
		PlayerListData(const SteamID& id);
		~PlayerListData();

		SteamID GetSteamID() const { return m_SteamID; }

		PlayerAttributesList m_Attributes;

		struct LastSeen
		{
			std::chrono::system_clock::time_point m_Time;
			std::string m_PlayerName;

			auto operator<=>(const LastSeen& other) const { return m_Time.time_since_epoch().count() <=> other.m_Time.time_since_epoch().count(); }
		};
		std::optional<LastSeen> m_LastSeen;

		std::vector<nlohmann::json> m_Proof;

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

	struct PlayerMarks final
	{
		struct Mark final
		{
			Mark(const PlayerAttributesList& attr, const ConfigFileInfo& file) :
				m_Attributes(attr), m_File(file)
			{
			}

			PlayerAttributesList m_Attributes;
			std::reference_wrapper<const ConfigFileInfo> m_File;
		};

		bool Has(const PlayerAttributesList& attr) const;

		operator bool() const { return !m_Marks.empty(); }

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
		void SaveFile() const;

		cppcoro::generator<std::pair<const ConfigFileInfo&, const PlayerListData&>>
			FindPlayerData(const SteamID& id) const;
		cppcoro::generator<std::pair<const ConfigFileInfo&, const PlayerAttributesList&>>
			FindPlayerAttributes(const SteamID& id) const;
		PlayerMarks GetPlayerAttributes(const SteamID& id) const;
		PlayerMarks HasPlayerAttributes(const SteamID& id, const PlayerAttributesList& attributes) const;

		template<typename TFunc>
		ModifyPlayerResult ModifyPlayer(const SteamID& id, TFunc&& func)
		{
			return ModifyPlayer(id,
				[](PlayerListData& data, void* userData) -> ModifyPlayerAction { return (*reinterpret_cast<TFunc*>(userData))(data); },
				reinterpret_cast<void*>(&func));
		}

		ModifyPlayerResult ModifyPlayer(const SteamID& id, ModifyPlayerAction(*func)(PlayerListData& data, void* userData),
			void* userData = nullptr);
		ModifyPlayerResult ModifyPlayer(const SteamID& id, ModifyPlayerAction(*func)(PlayerListData& data, const void* userData),
			const void* userData = nullptr);

		size_t GetPlayerCount() const { return m_CFGGroup.size(); }

	private:
		const Settings* m_Settings = nullptr;

		using PlayerMap_t = std::map<SteamID, PlayerListData>;

		struct PlayerListFile final : public SharedConfigFileBase
		{
			void ValidateSchema(const ConfigSchemaInfo& schema) const override;
			void Deserialize(const nlohmann::json& json) override;
			void Serialize(nlohmann::json& json) const override;

			size_t size() const { return m_Players.size(); }

			PlayerMap_t m_Players;
		};

		static constexpr int PLAYERLIST_SCHEMA_VERSION = 3;

		struct ConfigFileGroup final : ConfigFileGroupBase<PlayerListFile, std::vector<std::pair<ConfigFileInfo, PlayerMap_t>>>
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

template<typename CharT, typename Traits>
std::basic_ostream<CharT, Traits>& operator<<(std::basic_ostream<CharT, Traits>& os, tf2_bot_detector::PlayerAttribute type)
{
	using tf2_bot_detector::PlayerAttribute;

	switch (type)
	{
	case PlayerAttribute::Cheater:     return os << "Cheater";
	case PlayerAttribute::Exploiter:   return os << "Exploiter";
	case PlayerAttribute::Racist:      return os << "Racist";
	case PlayerAttribute::Suspicious:  return os << "Suspicious";

	default:
		assert(!"Unknown PlayerAttribute");
		return os << "<UNKNOWN>";
	}
}

template<typename CharT, typename Traits>
std::basic_ostream<CharT, Traits>& operator<<(std::basic_ostream<CharT, Traits>& os,
	const tf2_bot_detector::PlayerAttributesList& list)
{
	bool printed = false;
	for (size_t i = 0; i < list.size(); i++)
	{
		const auto thisAttr = tf2_bot_detector::PlayerAttribute(i);
		if (!list.HasAttribute(thisAttr))
			continue;

		if (printed)
			os << ", ";

		os << thisAttr;
		printed = true;
	}

	return os;
}

template<typename CharT, typename Traits>
std::basic_ostream<CharT, Traits>& operator<<(std::basic_ostream<CharT, Traits>& os,
	const tf2_bot_detector::PlayerMarks::Mark& mark)
{
	return os << std::quoted(mark.m_File.get().m_Title) << " (" << mark.m_Attributes << ')';
}
