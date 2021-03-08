#ifdef TF2BD_ENABLE_DISCORD_INTEGRATION
#include "DiscordRichPresence.h"
#include "Config/DRPInfo.h"
#include "Config/Settings.h"
#include "ConsoleLog/ConsoleLineListener.h"
#include "ConsoleLog/ConsoleLines.h"
#include "ConsoleLog/NetworkStatus.h"
#include "GameData/MatchmakingQueue.h"
#include "GameData/TFClassType.h"
#include "Log.h"
#include "WorldEventListener.h"
#include "WorldState.h"

#include <mh/concurrency/thread_sentinel.hpp>
#include <mh/text/charconv_helper.hpp>
#include <mh/text/fmtstr.hpp>
#include <mh/text/format.hpp>
#include <mh/text/indenting_ostream.hpp>
#include <mh/text/string_insertion.hpp>
#include <discord-game-sdk/core.h>
#include <cryptopp/sha.h>

#include <array>
#include <cassert>
#include <compare>

#undef min
#undef max

using namespace std::chrono_literals;
using namespace std::string_literals;
using namespace tf2_bot_detector;

namespace mh
{
	template<typename T>
	class property
	{
	public:
		constexpr property() = default;
		constexpr explicit property(const T& value) : m_Value(value) {}
		constexpr explicit property(T&& value) : m_Value(std::move(value)) {}

		constexpr const T& get() const { return m_Value; }
		constexpr operator const T& () const { return get(); }

		constexpr property& operator=(const property& rhs) { return set(rhs); }
		constexpr property& operator=(const T& rhs) { return set(rhs); }
		constexpr property& operator=(T&& rhs) { return set(std::move(rhs)); }

		constexpr property& set(const T& newValue)
		{
			if (newValue != m_Value && OnChange(newValue))
				m_Value = newValue;

			return *this;
		}
		constexpr property& set(T&& newValue)
		{
			if (newValue != m_Value && OnChange(newValue))
				m_Value = std::move(newValue);

			return *this;
		}

		constexpr auto operator<=>(const property&) const = default;

	protected:
		virtual bool OnChange(const T& newValue) const { return true; }

	private:
		T m_Value;
	};

	template<typename T> constexpr auto operator<=>(const property<T>& lhs, const T& rhs) { return lhs.get() <=> rhs; }
	template<typename T> constexpr auto operator<=>(const T& lhs, const property<T>& rhs) { return lhs <=> rhs.get(); }
	template<typename T> constexpr auto operator==(const property<T>& lhs, const T& rhs) { return lhs.get() == rhs; }
	template<typename T> constexpr bool operator==(const T& lhs, const property<T>& rhs) { return lhs == rhs.get(); }
}

static constexpr const char DEFAULT_LARGE_IMAGE_KEY[] = "tf2_1x1";

static constexpr LogMessageColor DISCORD_LOG_COLOR{ 117 / 255.0f, 136 / 255.0f, 215 / 255.0f };

static bool s_DiscordDebugLogEnabled = true;

template<typename... TArgs>
static auto DiscordDebugLog(const std::string_view& fmtStr, const TArgs&... args) ->
	decltype(mh::format(fmtStr, args...), void())
{
	if (!s_DiscordDebugLogEnabled)
		return;

	DebugLog(DISCORD_LOG_COLOR, "DRP: {}", mh::format(fmtStr, args...));
}

template<typename... TArgs>
static auto DiscordDebugLog(const mh::source_location& location,
	const std::string_view& fmtStr = {}, const TArgs&... args) ->
	decltype(mh::format(fmtStr, args...), void())
{
	if (!s_DiscordDebugLogEnabled)
		return;

	if (fmtStr.empty())
		DebugLog(DISCORD_LOG_COLOR, location);
	else
		DebugLog(DISCORD_LOG_COLOR, location, "DRP: {}", mh::format(fmtStr, args...));
}

static void DiscordLogHookFunc(discord::LogLevel level, const char* logMsg)
{
	DebugLog(DISCORD_LOG_COLOR, logMsg);
}

namespace
{
	enum class ConnectionState
	{
		Disconnected,  // We're not connected to a server in the first place.
		Local,         // This is a local (loopback) server.
		Nonlocal,      // This is a non-local server.
	};
}

MH_ENUM_REFLECT_BEGIN(discord::Result)
	MH_ENUM_REFLECT_VALUE(Ok)
	MH_ENUM_REFLECT_VALUE(ServiceUnavailable)
	MH_ENUM_REFLECT_VALUE(InvalidVersion)
	MH_ENUM_REFLECT_VALUE(LockFailed)
	MH_ENUM_REFLECT_VALUE(InternalError)
	MH_ENUM_REFLECT_VALUE(InvalidPayload)
	MH_ENUM_REFLECT_VALUE(InvalidCommand)
	MH_ENUM_REFLECT_VALUE(InvalidPermissions)
	MH_ENUM_REFLECT_VALUE(NotFetched)
	MH_ENUM_REFLECT_VALUE(NotFound)
	MH_ENUM_REFLECT_VALUE(Conflict)
	MH_ENUM_REFLECT_VALUE(InvalidSecret)
	MH_ENUM_REFLECT_VALUE(InvalidJoinSecret)
	MH_ENUM_REFLECT_VALUE(NoEligibleActivity)
	MH_ENUM_REFLECT_VALUE(InvalidInvite)
	MH_ENUM_REFLECT_VALUE(NotAuthenticated)
	MH_ENUM_REFLECT_VALUE(InvalidAccessToken)
	MH_ENUM_REFLECT_VALUE(ApplicationMismatch)
	MH_ENUM_REFLECT_VALUE(InvalidDataUrl)
	MH_ENUM_REFLECT_VALUE(InvalidBase64)
	MH_ENUM_REFLECT_VALUE(NotFiltered)
	MH_ENUM_REFLECT_VALUE(LobbyFull)
	MH_ENUM_REFLECT_VALUE(InvalidLobbySecret)
	MH_ENUM_REFLECT_VALUE(InvalidFilename)
	MH_ENUM_REFLECT_VALUE(InvalidFileSize)
	MH_ENUM_REFLECT_VALUE(InvalidEntitlement)
	MH_ENUM_REFLECT_VALUE(NotInstalled)
	MH_ENUM_REFLECT_VALUE(NotRunning)
	MH_ENUM_REFLECT_VALUE(InsufficientBuffer)
	MH_ENUM_REFLECT_VALUE(PurchaseCanceled)
	MH_ENUM_REFLECT_VALUE(InvalidGuild)
	MH_ENUM_REFLECT_VALUE(InvalidEvent)
	MH_ENUM_REFLECT_VALUE(InvalidChannel)
	MH_ENUM_REFLECT_VALUE(InvalidOrigin)
	MH_ENUM_REFLECT_VALUE(RateLimited)
	MH_ENUM_REFLECT_VALUE(OAuth2Error)
	MH_ENUM_REFLECT_VALUE(SelectChannelTimeout)
	MH_ENUM_REFLECT_VALUE(GetGuildTimeout)
	MH_ENUM_REFLECT_VALUE(SelectVoiceForceRequired)
	MH_ENUM_REFLECT_VALUE(CaptureShortcutAlreadyListening)
	MH_ENUM_REFLECT_VALUE(UnauthorizedForAchievement)
	MH_ENUM_REFLECT_VALUE(InvalidGiftCode)
	MH_ENUM_REFLECT_VALUE(PurchaseError)
	MH_ENUM_REFLECT_VALUE(TransactionAborted)
MH_ENUM_REFLECT_END()

MH_ENUM_REFLECT_BEGIN(discord::ActivityType)
	MH_ENUM_REFLECT_VALUE(Playing)
	MH_ENUM_REFLECT_VALUE(Streaming)
	MH_ENUM_REFLECT_VALUE(Listening)
	MH_ENUM_REFLECT_VALUE(Watching)
MH_ENUM_REFLECT_END()

MH_ENUM_REFLECT_BEGIN(ConnectionState)
	MH_ENUM_REFLECT_VALUE(Disconnected)
	MH_ENUM_REFLECT_VALUE(Local)
	MH_ENUM_REFLECT_VALUE(Nonlocal)
MH_ENUM_REFLECT_END()

static std::strong_ordering strcmp3(const char* lhs, const char* rhs)
{
	const auto result = strcmp(lhs, rhs);
	if (result < 0)
		return std::strong_ordering::less;
	else if (result == 0)
		return std::strong_ordering::equal;
	else //if (result > 0)
		return std::strong_ordering::greater;
}

static std::strong_ordering operator<=>(const discord::ActivityTimestamps& lhs, const discord::ActivityTimestamps& rhs)
{
	if (auto result = lhs.GetStart() <=> rhs.GetStart(); result != 0)
		return result;
	if (auto result = lhs.GetEnd() <=> rhs.GetEnd(); result != 0)
		return result;

	return std::strong_ordering::equal;
}

static std::strong_ordering operator<=>(const discord::ActivityAssets& lhs, const discord::ActivityAssets& rhs)
{
	if (auto result = strcmp3(lhs.GetLargeImage(), rhs.GetLargeImage()); result != 0)
		return result;
	if (auto result = strcmp3(lhs.GetLargeText(), rhs.GetLargeText()); result != 0)
		return result;
	if (auto result = strcmp3(lhs.GetSmallImage(), rhs.GetSmallImage()); result != 0)
		return result;
	if (auto result = strcmp3(lhs.GetSmallText(), rhs.GetSmallText()); result != 0)
		return result;

	return std::strong_ordering::equal;
}

static std::strong_ordering operator<=>(const discord::PartySize& lhs, const discord::PartySize& rhs)
{
	if (auto result = lhs.GetCurrentSize() <=> rhs.GetCurrentSize(); result != 0)
		return result;
	if (auto result = lhs.GetMaxSize() <=> rhs.GetMaxSize(); result != 0)
		return result;

	return std::strong_ordering::equal;
}

static std::strong_ordering operator<=>(const discord::ActivityParty& lhs, const discord::ActivityParty& rhs)
{
	if (auto result = strcmp3(lhs.GetId(), rhs.GetId()); result != 0)
		return result;
	if (auto result = lhs.GetSize() <=> rhs.GetSize(); result != 0)
		return result;

	return std::strong_ordering::equal;
}

static std::strong_ordering operator<=>(const discord::ActivitySecrets& lhs, const discord::ActivitySecrets& rhs)
{
	if (auto result = strcmp3(lhs.GetMatch(), rhs.GetMatch()); result != 0)
		return result;
	if (auto result = strcmp3(lhs.GetJoin(), rhs.GetJoin()); result != 0)
		return result;
	if (auto result = strcmp3(lhs.GetSpectate(), rhs.GetSpectate()); result != 0)
		return result;

	return std::strong_ordering::equal;
}

static std::strong_ordering operator<=>(const discord::Activity& lhs, const discord::Activity& rhs)
{
	if (auto result = lhs.GetType() <=> rhs.GetType(); result != 0)
		return result;
	if (auto result = lhs.GetApplicationId() <=> rhs.GetApplicationId(); result != 0)
		return result;
	if (auto result = strcmp3(lhs.GetName(), rhs.GetName()); result != 0)
		return result;
	if (auto result = strcmp3(lhs.GetState(), rhs.GetState()); result != 0)
		return result;
	if (auto result = strcmp3(lhs.GetDetails(), rhs.GetDetails()); result != 0)
		return result;
	if (auto result = lhs.GetTimestamps() <=> rhs.GetTimestamps(); result != 0)
		return result;
	if (auto result = lhs.GetAssets() <=> rhs.GetAssets(); result != 0)
		return result;
	if (auto result = lhs.GetParty() <=> rhs.GetParty(); result != 0)
		return result;
	if (auto result = lhs.GetSecrets() <=> rhs.GetSecrets(); result != 0)
		return result;
	if (auto result = lhs.GetInstance() <=> rhs.GetInstance(); result != 0)
		return result;

	return std::strong_ordering::equal;
}

namespace discord
{
	template<typename CharT, typename Traits>
	static std::basic_ostream<CharT, Traits>& operator<<(
		std::basic_ostream<CharT, Traits>& os, const discord::ActivityTimestamps& ts)
	{
		mh::indenting_ostream(os)
			<< "\nStart: " << ts.GetStart()
			<< "\nEnd:   " << ts.GetEnd()
			;

		return os;
	}

	template<typename CharT, typename Traits>
	static std::basic_ostream<CharT, Traits>& operator<<(
		std::basic_ostream<CharT, Traits>& os, const discord::ActivityAssets& a)
	{
		mh::indenting_ostream(os)
			<< "\nLargeImage: " << std::quoted(a.GetLargeImage())
			<< "\nLargeText:  " << std::quoted(a.GetLargeText())
			<< "\nSmallImage: " << std::quoted(a.GetSmallImage())
			<< "\nSmallText:  " << std::quoted(a.GetSmallText())
			;

		return os;
	}

	template<typename CharT, typename Traits>
	static std::basic_ostream<CharT, Traits>& operator<<(
		std::basic_ostream<CharT, Traits>& os, const discord::PartySize& ps)
	{
		mh::indenting_ostream(os)
			<< "\nCurrent: " << ps.GetCurrentSize()
			<< "\nMax:     " << ps.GetMaxSize()
			;

		return os;
	}

	template<typename CharT, typename Traits>
	static std::basic_ostream<CharT, Traits>& operator<<(
		std::basic_ostream<CharT, Traits>& os, const discord::ActivityParty& p)
	{
		mh::indenting_ostream(os)
			<< "\nID:   " << std::quoted(p.GetId())
			<< "\nSize: " << p.GetSize()
			;

		return os;
	}

	template<typename CharT, typename Traits>
	static std::basic_ostream<CharT, Traits>& operator<<(
		std::basic_ostream<CharT, Traits>& os, const discord::ActivitySecrets& s)
	{
		mh::indenting_ostream(os)
			<< "\nMatch:    " << std::quoted(s.GetMatch())
			<< "\nJoin:     " << std::quoted(s.GetJoin())
			<< "\nSpectate: " << std::quoted(s.GetSpectate())
			;

		return os;
	}

	template<typename CharT, typename Traits>
	static std::basic_ostream<CharT, Traits>& operator<<(std::basic_ostream<CharT, Traits>& os, const discord::Activity& a)
	{
		mh::indenting_ostream(os) << std::boolalpha
			<< "\nType:          " << mh::enum_fmt(a.GetType())
			<< "\nApplicationID: " << a.GetApplicationId()
			<< "\nName:          " << std::quoted(a.GetName())
			<< "\nState:         " << std::quoted(a.GetState())
			<< "\nDetails:       " << std::quoted(a.GetDetails())
			<< "\nTimestamps:    " << a.GetTimestamps()
			<< "\nAssets:        " << a.GetAssets()
			<< "\nParty:         " << a.GetParty()
			<< "\nSecrets:       " << a.GetSecrets()
			<< "\nInstance:      " << a.GetInstance()
			;

		return os;
	}
}

namespace
{
	struct DiscordGameState final
	{
		DiscordGameState(const Settings& settings, const DRPInfo& drpInfo) : m_Settings(&settings), m_DRPInfo(&drpInfo) {}

		discord::Activity ConstructActivity() const;

		void OnQueueStateChange(TFMatchGroup queueType, TFQueueStateChange state);
		void OnQueueStatusUpdate(TFMatchGroup queueType, time_point_t queueStartTime);
		void OnServerIPUpdate(const std::string_view& localIP);
		void SetInLobby(bool inLobby);
		void SetInLocalServer(bool inLocalServer);
		void SetInParty(bool inParty);
		void SetMapName(std::string mapName);
		void UpdateParty(const TFParty& party);
		void UpdatePartyMatchmakingBanTime(MatchmakingBannedTimeLine::LadderType type, uint64_t banTime);
		void OnLocalPlayerSpawned(TFClassType classType);
		void OnConnectionCountUpdate(unsigned connectionCount);

		struct PartyInfo
		{
			TFParty m_TFParty{};
			uint64_t m_CasualBanTime{};
			uint64_t m_RankedBanTime{};
		};

	private:
		const Settings* m_Settings = nullptr;
		const DRPInfo* m_DRPInfo = nullptr;

		struct QueueState
		{
			bool m_Active = false;
			time_point_t m_StartTime{};
		};
		std::array<QueueState, size_t(TFMatchGroup::COUNT)> m_QueueStates;

		bool IsQueueActive(TFMatchGroup matchGroup) const { return m_QueueStates.at((int)matchGroup).m_Active; }
		bool IsAnyQueueActive(TFMatchGroupFlags matchGroups) const;
		bool IsAnyQueueActive() const;

		std::optional<time_point_t> GetEarliestActiveQueueStartTime() const;

		TFClassType m_LastSpawnedClass = TFClassType::Undefined;
		std::string m_MapName;
		bool m_InLobby = false;
		time_point_t m_LastStatusTimestamp;

		PartyInfo m_PartyInfo;

		struct : public mh::property<ConnectionState>
		{
			using BaseClass = mh::property<ConnectionState>;
			using BaseClass::operator=;

			bool OnChange(const ConnectionState& newValue) const override
			{
				DiscordDebugLog(mh::format("ConnectionState {} -> {}", mh::enum_fmt(get()), mh::enum_fmt(newValue)));
				return true;
			}

		} m_ConnectionState;
	};
}

bool DiscordGameState::IsAnyQueueActive(TFMatchGroupFlags matchGroups) const
{
	for (size_t i = 0; i < size_t(TFMatchGroup::COUNT); i++)
	{
		if (!!(matchGroups & TFMatchGroup(i)) && IsQueueActive(TFMatchGroup(i)))
			return true;
	}

	return false;
}

bool DiscordGameState::IsAnyQueueActive() const
{
	for (const auto& type : m_QueueStates)
	{
		if (type.m_Active)
			return true;
	}

	return false;
}

std::optional<time_point_t> DiscordGameState::GetEarliestActiveQueueStartTime() const
{
	std::optional<time_point_t> retVal{};

	for (const auto& type : m_QueueStates)
	{
		if (!type.m_Active)
			continue;

		if (!retVal)
			retVal = type.m_StartTime;
		else
			retVal = std::min(*retVal, type.m_StartTime);
	}

	return retVal;
}

static void SetHashedPartyId(discord::ActivityParty& discordParty, const DiscordGameState::PartyInfo& gameParty)
{
	// I'm not 100% sure that discord exposes the party id to clients, but rather than
	// find out the hard way that its possible to annoy people in parties, we hash the
	// party ID and the party leader SteamID together to create an ID for discord that
	// is not trivially reversible.
	CryptoPP::SHA256 partyIDHash;

	const auto HashByValue = [&partyIDHash](const auto& data)
	{
		partyIDHash.Update((const CryptoPP::byte*)&data, sizeof(data));
	};
	HashByValue(gameParty.m_TFParty.m_PartyID);
	HashByValue(gameParty.m_TFParty.m_LeaderID.ID64);
	HashByValue(gameParty.m_CasualBanTime);
	HashByValue(gameParty.m_RankedBanTime);

	CryptoPP::byte digestBufRaw[partyIDHash.DIGESTSIZE];
	partyIDHash.Final(digestBufRaw);

	// In case anyone ever stumbles upon this string without having a clue what it's for
	constexpr char HASH_PREFIX[] = "tf2bd-discord-partyhash:";

	constexpr size_t STR_HASH_LENGTH = (sizeof(HASH_PREFIX) - 1) + sizeof(digestBufRaw) * 2 + 1;
	mh::pfstr<STR_HASH_LENGTH> digestStr;  // enough room for hex + null terminator + tf2bd

	digestStr.puts(HASH_PREFIX);
	for (CryptoPP::byte byte : digestBufRaw)
		digestStr.sprintf("%02x", byte);

	discordParty.SetId(digestStr.c_str());

#ifdef _DEBUG
	// In case discord changes their max party id string length in the future or something
	{
		const char* setPartyID = discordParty.GetId();
		assert(setPartyID);
		if (setPartyID)
			assert(!strncmp(digestStr.c_str(), setPartyID, digestStr.size()));
	}
#endif
}

discord::Activity DiscordGameState::ConstructActivity() const
{
	discord::Activity retVal{};

	std::string details;

	if (m_ConnectionState != ConnectionState::Disconnected && !m_MapName.empty())
	{
		if (auto map = m_DRPInfo->FindMap(m_MapName))
			retVal.GetAssets().SetLargeImage(mh::format("map_{}", map->m_MapNames.at(0)).c_str());
		else
			retVal.GetAssets().SetLargeImage("map_unknown");

		details = m_MapName;
	}
	else
	{
		retVal.GetAssets().SetLargeImage(DEFAULT_LARGE_IMAGE_KEY);
	}

	const auto GetGameState = [&]
	{
		if (m_ConnectionState == ConnectionState::Local)
		{
			return "Local Server";
		}
		else if (m_ConnectionState == ConnectionState::Nonlocal)
		{
			if (m_InLobby)
				return "Casual";
			else
				return "Community";
		}
		else if (m_ConnectionState == ConnectionState::Disconnected)
		{
			return "Main Menu";
		}

		return "";
	};

	if (IsAnyQueueActive())
	{
		const bool isCasual = IsAnyQueueActive(TFMatchGroupFlags::Casual);
		const bool isMVM = IsAnyQueueActive(TFMatchGroupFlags::MVM);
		const bool isComp = IsAnyQueueActive(TFMatchGroupFlags::Competitive);

		if (isCasual && isComp && isMVM)
			retVal.SetState("Searching - Casual, Competitive, and MVM");
		else if (isCasual && isComp)
			retVal.SetState("Searching - Casual & Competitive");
		else if (isCasual && isMVM)
			retVal.SetState("Searching - Casual & MVM");
		else if (isCasual)
			retVal.SetState("Searching - Casual");
		else if (isComp && isMVM)
			retVal.SetState("Searching - Competitive & MVM");
		else if (isComp)
			retVal.SetState("Searching - Competitive");
		else if (isMVM)
			retVal.SetState("Searching - MVM");
		else
		{
			assert(!"Unknown combination of flags");
			retVal.SetState("Searching");
		}

		if (auto startTime = GetEarliestActiveQueueStartTime())
			retVal.GetTimestamps().SetStart(to_seconds<discord::Timestamp>(startTime->time_since_epoch()));

		if (m_ConnectionState != ConnectionState::Disconnected)
		{
			if (details.empty())
				details = GetGameState();
			else
				details = mh::format("{} - {}", GetGameState(), details);
		}
	}
	else
	{
		retVal.SetState(GetGameState());
	}

	if (m_PartyInfo.m_TFParty.m_MemberCount > 0)
	{
		// Party ID (so discord knows two people are in the same party)
		SetHashedPartyId(retVal.GetParty(), m_PartyInfo);

		retVal.GetParty().GetSize().SetCurrentSize(m_PartyInfo.m_TFParty.m_MemberCount);
		retVal.GetParty().GetSize().SetMaxSize(6);
	}

	if (m_ConnectionState != ConnectionState::Disconnected)
	{
		auto& assets = retVal.GetAssets();
		switch (m_LastSpawnedClass)
		{
		case TFClassType::Demoman:
			assets.SetSmallImage("leaderboard_class_demo");
			assets.SetSmallText("Demo");
			break;
		case TFClassType::Engie:
			assets.SetSmallImage("leaderboard_class_engineer");
			assets.SetSmallText("Engineer");
			break;
		case TFClassType::Heavy:
			assets.SetSmallImage("leaderboard_class_heavy");
			assets.SetSmallText("Heavy");
			break;
		case TFClassType::Medic:
			assets.SetSmallImage("leaderboard_class_medic");
			assets.SetSmallText("Medic");
			break;
		case TFClassType::Pyro:
			assets.SetSmallImage("leaderboard_class_pyro");
			assets.SetSmallText("Pyro");
			break;
		case TFClassType::Scout:
			assets.SetSmallImage("leaderboard_class_scout");
			assets.SetSmallText("Scout");
			break;
		case TFClassType::Sniper:
			assets.SetSmallImage("leaderboard_class_sniper");
			assets.SetSmallText("Sniper");
			break;
		case TFClassType::Soldier:
			assets.SetSmallImage("leaderboard_class_soldier");
			assets.SetSmallText("Soldier");
			break;
		case TFClassType::Spy:
			assets.SetSmallImage("leaderboard_class_spy");
			assets.SetSmallText("Spy");
			break;

		case TFClassType::Undefined:
			break;
		}
	}

	retVal.SetDetails(details.c_str());

	return retVal;
}

void DiscordGameState::OnQueueStateChange(TFMatchGroup queueType, TFQueueStateChange state)
{
	DiscordDebugLog(MH_SOURCE_LOCATION_CURRENT());
	auto& queue = m_QueueStates.at(size_t(queueType));

	if (state == TFQueueStateChange::Entered)
	{
		queue.m_Active = true;
		queue.m_StartTime = tfbd_clock_t::now();
	}
	else if (state == TFQueueStateChange::Exited || state == TFQueueStateChange::RequestedExit)
	{
		queue.m_Active = false;
	}
}

void DiscordGameState::OnQueueStatusUpdate(TFMatchGroup queueType, time_point_t queueStartTime)
{
	DiscordDebugLog(MH_SOURCE_LOCATION_CURRENT());
	auto& queue = m_QueueStates.at(size_t(queueType));
	queue.m_Active = true;
	queue.m_StartTime = queueStartTime;
}

void DiscordGameState::OnServerIPUpdate(const std::string_view& localIP)
{
	DiscordDebugLog(MH_SOURCE_LOCATION_CURRENT());
	if (localIP.starts_with("0.0.0.0:"))
	{
		SetInLocalServer(true);
	}
	else
	{
		SetInLocalServer(false);
		m_ConnectionState = ConnectionState::Nonlocal;
	}
}

void DiscordGameState::SetInLobby(bool inLobby)
{
	DiscordDebugLog(MH_SOURCE_LOCATION_CURRENT());
	if (inLobby)
		m_ConnectionState = ConnectionState::Nonlocal;

	m_InLobby = inLobby;
}

void DiscordGameState::SetInLocalServer(bool inLocalServer)
{
	DiscordDebugLog(MH_SOURCE_LOCATION_CURRENT());
	if (inLocalServer)
	{
		m_ConnectionState = ConnectionState::Local;
		m_InLobby = false;
	}
}

void DiscordGameState::SetInParty(bool inParty)
{
	DiscordDebugLog(MH_SOURCE_LOCATION_CURRENT());
	if (inParty)
	{
		if (m_PartyInfo.m_TFParty.m_MemberCount < 1)
			m_PartyInfo.m_TFParty.m_MemberCount = 1;
	}
	else
	{
		m_PartyInfo.m_TFParty =
		{
			.m_MemberCount = 0,
		};
	}
}

void DiscordGameState::SetMapName(std::string mapName)
{
	DiscordDebugLog(MH_SOURCE_LOCATION_CURRENT());
	m_MapName = std::move(mapName);
	if (m_MapName.empty())
		m_LastSpawnedClass = TFClassType::Undefined;
}

void DiscordGameState::UpdateParty(const TFParty& party)
{
	DiscordDebugLog(MH_SOURCE_LOCATION_CURRENT());
	if (party.m_MemberCount > 0)
	{
		SetInParty(true);
		m_PartyInfo.m_TFParty = party;
	}
}

void DiscordGameState::UpdatePartyMatchmakingBanTime(MatchmakingBannedTimeLine::LadderType ladderType, uint64_t banTime)
{
	using LadderType = MatchmakingBannedTimeLine::LadderType;
	switch (ladderType)
	{
	case LadderType::Casual:
		m_PartyInfo.m_CasualBanTime = banTime;
		return;
	case LadderType::Competitive:
		m_PartyInfo.m_RankedBanTime = banTime;
		return;
	}

	LogError("Unknown ladderType {}", mh::enum_fmt(ladderType));
}

void DiscordGameState::OnLocalPlayerSpawned(TFClassType classType)
{
	DiscordDebugLog(MH_SOURCE_LOCATION_CURRENT());
	m_LastSpawnedClass = classType;
}

void DiscordGameState::OnConnectionCountUpdate(unsigned connectionCount)
{
	DiscordDebugLog(MH_SOURCE_LOCATION_CURRENT());
	if (connectionCount < 1)
		m_ConnectionState = ConnectionState::Disconnected;
}

namespace
{
	class DiscordState final : public IDRPManager, AutoWorldEventListener, AutoConsoleLineListener
	{
	public:
		static constexpr duration_t UPDATE_INTERVAL = 10s;
		static_assert(UPDATE_INTERVAL >= (20s / 5), "Update interval too low");

		DiscordState(const Settings& settings, IWorldState& world);
		~DiscordState();

		void QueueUpdate() { m_WantsUpdate = true; }
		void Update() override;

		void OnConsoleLineParsed(IWorldState& world, IConsoleLine& line) override;
		void OnLocalPlayerSpawned(IWorldState& world, TFClassType classType) override;

	private:
		mh::thread_sentinel m_Sentinel;

		std::unique_ptr<discord::Core> m_Core;
		time_point_t m_LastDiscordInitializeTime{};

		const Settings& GetSettings() const { return m_Settings; }
		IWorldState& GetWorld() { return m_WorldState; }

		const Settings& m_Settings;
		IWorldState& m_WorldState;
		DRPInfo m_DRPInfo;
		DiscordGameState m_GameState;

		bool m_WantsUpdate = false;
		time_point_t m_LastUpdate{};
		discord::Activity m_CurrentActivity{};
	};
}

DiscordState::DiscordState(const Settings& settings, IWorldState& world) :
	AutoWorldEventListener(world),
	AutoConsoleLineListener(world),
	m_Settings(settings),
	m_WorldState(world),
	m_GameState(settings, m_DRPInfo),
	m_DRPInfo(settings)
{
}

DiscordState::~DiscordState()
{
	DiscordDebugLog(MH_SOURCE_LOCATION_CURRENT());
}

void DiscordState::OnConsoleLineParsed(IWorldState& world, IConsoleLine& line)
{
	m_Sentinel.check();

	switch (line.GetType())
	{
	case ConsoleLineType::PlayerStatusMapPosition:
	{
		QueueUpdate();
		auto& statusLine = static_cast<const ServerStatusMapLine&>(line);
		m_GameState.SetMapName(statusLine.GetMapName());
		break;
	}
	case ConsoleLineType::PartyHeader:
	{
		QueueUpdate();
		auto& partyLine = static_cast<const PartyHeaderLine&>(line);
		m_GameState.UpdateParty(partyLine.GetParty());
		break;
	}
	case ConsoleLineType::MatchmakingBannedTime:
	{
		QueueUpdate();
		auto& banLine = static_cast<const MatchmakingBannedTimeLine&>(line);
		m_GameState.UpdatePartyMatchmakingBanTime(banLine.GetLadderType(), banLine.GetBannedTime());
		break;
	}
	case ConsoleLineType::LobbyHeader:
	{
		QueueUpdate();
		m_GameState.SetInLobby(true);
		break;
	}
	case ConsoleLineType::LobbyStatusFailed:
	{
		QueueUpdate();
		m_GameState.SetInLobby(false);
		break;
	}
	case ConsoleLineType::QueueStateChange:
	{
		QueueUpdate();
		auto& queueLine = static_cast<const QueueStateChangeLine&>(line);
		m_GameState.OnQueueStateChange(queueLine.GetQueueType(), queueLine.GetStateChange());
		break;
	}
	case ConsoleLineType::InQueue:
	{
		QueueUpdate();
		auto& queueLine = static_cast<const InQueueLine&>(line);
		m_GameState.OnQueueStatusUpdate(queueLine.GetQueueType(), queueLine.GetQueueStartTime());
		break;
	}
	case ConsoleLineType::LobbyChanged:
	{
		QueueUpdate();
		auto& lobbyLine = static_cast<const LobbyChangedLine&>(line);
		if (lobbyLine.GetChangeType() == LobbyChangeType::Destroyed)
			m_GameState.SetMapName("");

		break;
	}
	case ConsoleLineType::ServerJoin:
	{
		QueueUpdate();
		auto& joinLine = static_cast<const ServerJoinLine&>(line);
		m_GameState.SetMapName(joinLine.GetMapName());
		// Not necessarily in a lobby at this point, but in-lobby state will be reapplied soon if we are in a lobby
		m_GameState.SetInLobby(false);
		break;
	}
	case ConsoleLineType::HostNewGame:
	{
		QueueUpdate();
		m_GameState.SetInLocalServer(true);
		break;
	}
	case ConsoleLineType::PlayerStatusIP:
	{
		QueueUpdate();
		auto& ipLine = static_cast<const ServerStatusPlayerIPLine&>(line);
		m_GameState.OnServerIPUpdate(ipLine.GetLocalIP());
		break;
	}
	case ConsoleLineType::NetStatusConfig:
	{
		QueueUpdate();
		auto& netLine = static_cast<const NetStatusConfigLine&>(line);
		m_GameState.OnConnectionCountUpdate(netLine.GetConnectionCount());
		break;
	}
	case ConsoleLineType::Connecting:
	{
		QueueUpdate();
		auto& connLine = static_cast<const ConnectingLine&>(line);
		m_GameState.OnServerIPUpdate(connLine.GetAddress());
		break;
	}
	case ConsoleLineType::SVC_UserMessage:
	{
		QueueUpdate();
		auto& umsgLine = static_cast<SVCUserMessageLine&>(line);
		m_GameState.OnServerIPUpdate(umsgLine.GetAddress());
		break;
	}

	default:
		break;
	}
}

void DiscordState::OnLocalPlayerSpawned(IWorldState& world, TFClassType classType)
{
	m_Sentinel.check();

	QueueUpdate();
	m_GameState.OnLocalPlayerSpawned(classType);
}

static void DiscordLogFunc(discord::LogLevel level, const char* msg)
{
	switch (level)
	{
	case discord::LogLevel::Debug:
		DebugLog(msg);
		break;
	case discord::LogLevel::Info:
		Log(msg);
		break;
	case discord::LogLevel::Warn:
		LogWarning(msg);
		break;

	default:
		LogError("Unknown discord LogLevel "s << +std::underlying_type_t<discord::LogLevel>(level));
		[[fallthrough]];
	case discord::LogLevel::Error:
		LogError(msg);
		break;
	}
}

void DiscordState::Update()
{
	m_Sentinel.check();

	s_DiscordDebugLogEnabled = GetSettings().m_Logging.m_DiscordRichPresence;

	// Initialize discord
	const auto curTime = tfbd_clock_t::now();
	if (!m_Core && (curTime - m_LastDiscordInitializeTime) > 10s)
	{
		m_LastDiscordInitializeTime = curTime;

		discord::Core* core = nullptr;
		if (auto result = discord::Core::Create(730945386390224976, DiscordCreateFlags_NoRequireDiscord, &core);
			result != discord::Result::Ok)
		{
			DebugLogWarning("Failed to initialize Discord Game SDK: {}", mh::enum_fmt(result));
		}
		else
		{
			m_Core.reset(core);

			core->SetLogHook(discord::LogLevel::Debug, &DiscordLogHookFunc);

			if (auto result = m_Core->ActivityManager().RegisterSteam(440); result != discord::Result::Ok)
				DebugLogWarning("Failed to register discord integration as steam appid 440: {}", mh::enum_fmt(result));
		}
	}

	if (m_Core)
	{
		std::invoke([&]()
			{
				if (!m_WantsUpdate)
					return;

				if ((curTime - m_LastUpdate) < UPDATE_INTERVAL)
					return;

				if (const auto nextActivity = m_GameState.ConstructActivity();
					std::is_neq(nextActivity <=> m_CurrentActivity))
				{
					// Perform the update
					m_Core->ActivityManager().UpdateActivity(nextActivity, [nextActivity](discord::Result result)
						{
							if (result == discord::Result::Ok)
							{
								DiscordDebugLog("Updated discord activity state: {}", nextActivity);
							}
							else
							{
								LogWarning(MH_SOURCE_LOCATION_CURRENT(),
									"Failed to update discord activity state: {}", mh::enum_fmt(result));
							}
						});

					m_CurrentActivity = nextActivity;
					m_LastUpdate = curTime;
				}
				else
				{
					DiscordDebugLog(MH_SOURCE_LOCATION_CURRENT(), "Discord activity state unchanged");
				}

				// Update successful
				m_WantsUpdate = false;
			});

		// Run discord callbacks
		if (auto result = m_Core->RunCallbacks(); result != discord::Result::Ok)
		{
			auto errMsg = mh::format("Failed to run discord callbacks: {}", mh::enum_fmt(result));
			switch (result)
			{
			case discord::Result::NotRunning:
				DebugLogWarning(std::move(errMsg));
				// Discord Game SDK will never recover from this state. Need to shutdown and try
				// to reinitialize in a bit. Also set the last initialize time to now, so we don't
				// try to reinitialize next frame (which will probably fail).
				m_Core.reset();
				m_LastDiscordInitializeTime = curTime;
				break;

			default:
				LogError(std::move(errMsg));
				break;
			}
		}
	}
}

#endif

std::unique_ptr<IDRPManager> IDRPManager::Create(const Settings& settings, IWorldState& world)
{
	return std::make_unique<DiscordState>(settings, world);
}
