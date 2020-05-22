#pragma once

#include "SteamID.h"

#include <chrono>
#include <map>
#include <optional>

namespace tf2_bot_detector
{
	enum class PlayerAttributes
	{
		Cheater,
		Suspicious,
		Exploiter,
		Racist,

		COUNT,
	};

	struct PlayerAttributesList
	{
		constexpr PlayerAttributesList() = default;

		bool HasAttribute(PlayerAttributes attribute) const;
		bool SetAttribute(PlayerAttributes attribute, bool set = true);

	private:
		// For now... just implemented with bools for simplicity
		bool m_Cheater : 1 = false;
		bool m_Suspicious : 1 = false;
		bool m_Exploiter : 1 = false;
		bool m_Racist : 1 = false;
	};

	struct PlayerListData
	{
		PlayerListData(const SteamID& id);

		SteamID GetSteamID() const { return m_SteamID; }

		PlayerAttributesList m_Attributes;

		struct LastSeen
		{
			std::chrono::system_clock::time_point m_Time;
			std::string m_PlayerName;
		};
		std::optional<LastSeen> m_LastSeen;

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
		std::invocable<T, PlayerListData&>;
		{ x(std::declval<PlayerListData>()) } -> std::same_as<ModifyPlayerAction>;
	};

	class PlayerListJSON final
	{
	public:
		PlayerListJSON();

		void LoadFile();
		void SaveFile() const;

		const PlayerListData* FindPlayerData(const SteamID& id) const;
		const PlayerAttributesList* FindPlayerAttributes(const SteamID& id) const;

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

	private:
		std::map<SteamID, PlayerListData> m_Players;
	};
}

template<typename CharT, typename Traits>
std::basic_ostream<CharT, Traits>& operator<<(std::basic_ostream<CharT, Traits>& os, tf2_bot_detector::PlayerAttributes type)
{
	using tf2_bot_detector::PlayerAttributes;

	switch (type)
	{
	case PlayerAttributes::Cheater:     return os << "PlayerAttributes::Cheater";
	case PlayerAttributes::Exploiter:   return os << "PlayerAttributes::Exploiter";
	case PlayerAttributes::Racist:      return os << "PlayerAttributes::Racist";
	case PlayerAttributes::Suspicious:  return os << "PlayerAttributes::Suspicious";

	default:
		assert(!"Unknown PlayerAttributes");
		return os << "<UNKNOWN>";
	}
}
