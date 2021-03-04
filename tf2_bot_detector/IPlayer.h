#pragma once

#include "Clock.h"
#include "SteamID.h"
#include "TFConstants.h"

#include <mh/error/expected.hpp>

#include <any>
#include <cstdint>
#include <optional>
#include <ostream>
#include <typeindex>

namespace tf2_bot_detector
{
	struct LobbyMember;
	enum class PlayerStatusState : uint8_t;
	enum class TFTeam : uint8_t;
	class IWorldState;

	namespace SteamAPI
	{
		struct PlayerSummary;
		struct PlayerBans;
		struct PlayerInventoryInfo;
	}

	namespace LogsTFAPI
	{
		struct PlayerLogsInfo;
	}

	struct PlayerScores
	{
		uint16_t m_Kills = 0;
		uint16_t m_Deaths = 0;
		uint16_t m_LocalKills = 0;
		uint16_t m_LocalDeaths = 0;
	};

	class IPlayer : public std::enable_shared_from_this<IPlayer>
	{
	public:
		virtual ~IPlayer() = default;

		virtual IWorldState& GetWorld() { return const_cast<IWorldState&>(std::as_const(*this).GetWorld()); }
		virtual const IWorldState& GetWorld() const = 0;

		virtual const LobbyMember* GetLobbyMember() const = 0;

		// Player name exactly as it is ingame, no evil characters collapsed/removed
		virtual std::string GetNameUnsafe() const = 0;

		// Player name with evil characters collapsed/removed
		std::string GetNameSafe() const;

		virtual SteamID GetSteamID() const = 0;
		virtual const mh::expected<SteamAPI::PlayerSummary>& GetPlayerSummary() const = 0;
		virtual const mh::expected<SteamAPI::PlayerBans>& GetPlayerBans() const = 0;
		virtual mh::expected<duration_t> GetTF2Playtime() const = 0;
		virtual bool IsFriend() const = 0;
		virtual std::optional<UserID_t> GetUserID() const = 0;

		virtual PlayerStatusState GetConnectionState() const = 0;
		virtual time_point_t GetConnectionTime() const = 0;
		virtual duration_t GetConnectedTime() const = 0;

		virtual TFTeam GetTeam() const = 0;
		virtual const PlayerScores& GetScores() const = 0;
		virtual uint16_t GetPing() const = 0;

		virtual time_point_t GetLastStatusUpdateTime() const = 0;
		duration_t GetTimeSinceLastStatusUpdate() const;

		// The estimated creation time of the account. Determined via linear interpolation
		// between ages of surrounding public steam profiles.
		virtual std::optional<time_point_t> GetEstimatedAccountCreationTime() const = 0;
		std::optional<duration_t> GetEstimatedAccountAge() const;

		virtual const mh::expected<LogsTFAPI::PlayerLogsInfo>& GetLogsInfo() const = 0;

		virtual const mh::expected<SteamAPI::PlayerInventoryInfo>& GetInventoryInfo() const = 0;

		// The time that this player has been in the "active" state.
		virtual duration_t GetActiveTime() const = 0;

		operator SteamID() const { return GetSteamID(); }

		template<typename T> inline T* GetData()
		{
			return const_cast<T*>(std::as_const(*this).GetData<T>());
		}
		template<typename T, typename... TArgs> inline T& GetOrCreateData(TArgs&&... args)
		{
			auto& storage = GetOrCreateDataStorage(typeid(T));
			if (!storage.has_value())
				return storage.emplace<T>(std::forward<TArgs>(args)...);
			else
				return std::any_cast<T&>(storage);
		}
		template<typename T> inline const T* GetData() const
		{
			return std::any_cast<T>(FindDataStorage(typeid(T)));
		}
		template<typename T> inline bool SetData(T&& value)
		{
			GetOrCreateDataStorage(typeid(T)) = std::move(value);
		}

		virtual std::any& GetOrCreateDataStorage(const std::type_index& type) = 0;

	protected:
		virtual const std::any* FindDataStorage(const std::type_index& type) const = 0;
		virtual std::any* FindDataStorage(const std::type_index& type)
		{
			return const_cast<std::any*>(std::as_const(*this).FindDataStorage(type));
		}
	};
}

template<typename CharT, typename Traits>
std::basic_ostream<CharT, Traits>& operator<<(std::basic_ostream<CharT, Traits>& os, const tf2_bot_detector::IPlayer& player)
{
	return os << '"' << player.GetNameSafe() << "\" " << player.GetSteamID();
}

template<typename CharT, typename Traits>
std::basic_ostream<CharT, Traits>& operator<<(std::basic_ostream<CharT, Traits>& os, const tf2_bot_detector::IPlayer* player)
{
	if (!player)
		return os << "<NULL IPlayer*>";
	else
		return os << *player;
}
