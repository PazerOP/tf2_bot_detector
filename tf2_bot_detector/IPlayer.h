#pragma once

#include "Clock.h"
#include "SteamID.h"
#include "TFConstants.h"

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
	class WorldState;

	namespace SteamAPI
	{
		struct PlayerSummary;
	}

	struct PlayerScores
	{
		uint16_t m_Kills = 0;
		uint16_t m_Deaths = 0;
	};

	class IPlayer
	{
	public:
		virtual ~IPlayer() = default;

		virtual WorldState& GetWorld() { return const_cast<WorldState&>(std::as_const(*this).GetWorld()); }
		virtual const WorldState& GetWorld() const = 0;

		virtual const LobbyMember* GetLobbyMember() const = 0;

		virtual std::string_view GetName() const = 0;
		virtual SteamID GetSteamID() const = 0;
		virtual const SteamAPI::PlayerSummary* GetPlayerSummary() const = 0;
		virtual std::optional<UserID_t> GetUserID() const = 0;

		virtual PlayerStatusState GetConnectionState() const = 0;
		virtual time_point_t GetConnectionTime() const = 0;
		virtual duration_t GetConnectedTime() const = 0;

		virtual TFTeam GetTeam() const = 0;
		virtual const PlayerScores& GetScores() const = 0;
		virtual uint16_t GetPing() const = 0;

		virtual time_point_t GetLastStatusUpdateTime() const = 0;
		duration_t GetTimeSinceLastStatusUpdate() const;

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
#if 0
		template<typename T> const T* GetPlayerData(const SteamID& id) const
		{
			if (auto foundPlayer = m_CurrentPlayerData.find(id); foundPlayer != m_CurrentPlayerData.end())
			{
				if (auto foundData = foundPlayer->second.m_UserData.find(typeid(T)); foundData != foundPlayer->second.m_UserData.end())
					return &std::any_cast<T>(foundData->second);
			}

			return nullptr;
		}
		template<typename T> bool SetPlayerData(const SteamID& id, T&& value)
		{
			if (auto foundPlayer = m_CurrentPlayerData.find(id); foundPlayer != m_CurrentPlayerData.end())
				foundPlayer->second.m_UserData[typeid(T)] = std::move(value);
		}
#else
		template<typename T> inline const T* GetData() const
		{
			return std::any_cast<T>(FindDataStorage(typeid(T)));
		}
		template<typename T> inline bool SetData(T&& value)
		{
			GetOrCreateDataStorage(typeid(T)) = std::move(value);
		}
#endif

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
	return os << '"' << player.GetName() << "\" " << player.GetSteamID();
}

template<typename CharT, typename Traits>
std::basic_ostream<CharT, Traits>& operator<<(std::basic_ostream<CharT, Traits>& os, const tf2_bot_detector::IPlayer* player)
{
	if (!player)
		return os << "<NULL IPlayer*>";
	else
		return os << *player;
}
