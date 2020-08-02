#ifdef TF2BD_ENABLE_DISCORD_INTEGRATION
#include "DiscordRichPresence.h"
#include "ConsoleLog/ConsoleLines.h"
#include "GameData/TFClassType.h"
#include "Log.h"
#include "WorldState.h"

#include <mh/text/format.hpp>
#include <mh/text/string_insertion.hpp>
#include <discord-game-sdk/core.h>

#include <cassert>

using namespace std::chrono_literals;
using namespace std::string_literals;
using namespace tf2_bot_detector;

static constexpr const char DEFAULT_LARGE_IMAGE_KEY[] = "tf2_1x1";

template<typename CharT, typename Traits>
std::basic_ostream<CharT, Traits>& operator<<(std::basic_ostream<CharT, Traits>& os, discord::Result result)
{
	using Result = discord::Result;

#undef OS_CASE
#define OS_CASE(v) case v : return os << #v
	switch (result)
	{
		OS_CASE(Result::Ok);
		OS_CASE(Result::ServiceUnavailable);
		OS_CASE(Result::InvalidVersion);
		OS_CASE(Result::LockFailed);
		OS_CASE(Result::InternalError);
		OS_CASE(Result::InvalidPayload);
		OS_CASE(Result::InvalidCommand);
		OS_CASE(Result::InvalidPermissions);
		OS_CASE(Result::NotFetched);
		OS_CASE(Result::NotFound);
		OS_CASE(Result::Conflict);
		OS_CASE(Result::InvalidSecret);
		OS_CASE(Result::InvalidJoinSecret);
		OS_CASE(Result::NoEligibleActivity);
		OS_CASE(Result::InvalidInvite);
		OS_CASE(Result::NotAuthenticated);
		OS_CASE(Result::InvalidAccessToken);
		OS_CASE(Result::ApplicationMismatch);
		OS_CASE(Result::InvalidDataUrl);
		OS_CASE(Result::InvalidBase64);
		OS_CASE(Result::NotFiltered);
		OS_CASE(Result::LobbyFull);
		OS_CASE(Result::InvalidLobbySecret);
		OS_CASE(Result::InvalidFilename);
		OS_CASE(Result::InvalidFileSize);
		OS_CASE(Result::InvalidEntitlement);
		OS_CASE(Result::NotInstalled);
		OS_CASE(Result::NotRunning);
		OS_CASE(Result::InsufficientBuffer);
		OS_CASE(Result::PurchaseCanceled);
		OS_CASE(Result::InvalidGuild);
		OS_CASE(Result::InvalidEvent);
		OS_CASE(Result::InvalidChannel);
		OS_CASE(Result::InvalidOrigin);
		OS_CASE(Result::RateLimited);
		OS_CASE(Result::OAuth2Error);
		OS_CASE(Result::SelectChannelTimeout);
		OS_CASE(Result::GetGuildTimeout);
		OS_CASE(Result::SelectVoiceForceRequired);
		OS_CASE(Result::CaptureShortcutAlreadyListening);
		OS_CASE(Result::UnauthorizedForAchievement);
		OS_CASE(Result::InvalidGiftCode);
		OS_CASE(Result::PurchaseError);
		OS_CASE(Result::TransactionAborted);

	default:
		return os << "Result(" << +std::underlying_type_t<Result>(result) << ')';
	}

#undef OS_CASE
}

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

#define EQUALS_OP_FROM_3WAY(type) \
	static bool operator==(const type& lhs, const type& rhs) { return std::is_eq(lhs <=> rhs); }

static std::strong_ordering operator<=>(const discord::ActivityTimestamps& lhs, const discord::ActivityTimestamps& rhs)
{
	if (auto result = lhs.GetStart() <=> rhs.GetStart(); result != 0)
		return result;
	if (auto result = lhs.GetEnd() <=> rhs.GetEnd(); result != 0)
		return result;

	return std::strong_ordering::equal;
}
EQUALS_OP_FROM_3WAY(discord::ActivityTimestamps);

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
EQUALS_OP_FROM_3WAY(discord::ActivityAssets);

static std::strong_ordering operator<=>(const discord::PartySize& lhs, const discord::PartySize& rhs)
{
	if (auto result = lhs.GetCurrentSize() <=> rhs.GetCurrentSize(); result != 0)
		return result;
	if (auto result = lhs.GetMaxSize() <=> rhs.GetMaxSize(); result != 0)
		return result;

	return std::strong_ordering::equal;
}
EQUALS_OP_FROM_3WAY(discord::PartySize);

static std::strong_ordering operator<=>(const discord::ActivityParty& lhs, const discord::ActivityParty& rhs)
{
	if (auto result = strcmp3(lhs.GetId(), rhs.GetId()); result != 0)
		return result;
	if (auto result = lhs.GetSize() <=> rhs.GetSize(); result != 0)
		return result;

	return std::strong_ordering::equal;
}
EQUALS_OP_FROM_3WAY(discord::ActivityParty);

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
EQUALS_OP_FROM_3WAY(discord::ActivitySecrets);

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
EQUALS_OP_FROM_3WAY(discord::Activity);

namespace
{
	struct MyDiscordActivityTimestamps
	{
		auto operator<=>(const MyDiscordActivityTimestamps&) const = default;

		discord::Timestamp m_Start{};
		discord::Timestamp m_End{};

		operator discord::ActivityTimestamps() const
		{
			discord::ActivityTimestamps retVal{};
			retVal.SetStart(m_Start);
			retVal.SetEnd(m_End);
			return retVal;
		}
	};

	struct MyDiscordActivityAssets
	{
		auto operator<=>(const MyDiscordActivityAssets&) const = default;

		std::string m_LargeImage;
		std::string m_LargeText;
		std::string m_SmallImage;
		std::string m_SmallText;

		operator discord::ActivityAssets() const
		{
			discord::ActivityAssets retVal{};
			retVal.SetLargeImage(m_LargeImage.c_str());
			retVal.SetLargeText(m_LargeText.c_str());
			retVal.SetSmallImage(m_SmallImage.c_str());
			retVal.SetSmallText(m_SmallText.c_str());
			return retVal;
		}
	};

	struct MyDiscordPartySize
	{
		auto operator<=>(const MyDiscordPartySize&) const = default;

		std::int32_t m_CurrentSize{};
		std::int32_t m_MaxSize{};

		operator discord::PartySize() const
		{
			discord::PartySize retVal{};
			retVal.SetCurrentSize(m_CurrentSize);
			retVal.SetMaxSize(m_MaxSize);
			return retVal;
		}
	};

	struct MyDiscordActivityParty
	{
		auto operator<=>(const MyDiscordActivityParty&) const = default;

		std::string m_ID;
		MyDiscordPartySize m_Size;

		operator discord::ActivityParty() const
		{
			discord::ActivityParty retVal{};
			retVal.SetId(m_ID.c_str());
			retVal.GetSize() = m_Size;
			return retVal;
		}
	};

	struct MyDiscordActivitySecrets
	{
		auto operator<=>(const MyDiscordActivitySecrets&) const = default;

		std::string m_Match;
		std::string m_Join;
		std::string m_Spectate;

		operator discord::ActivitySecrets() const
		{
			discord::ActivitySecrets retVal{};
			retVal.SetMatch(m_Match.c_str());
			retVal.SetJoin(m_Join.c_str());
			retVal.SetSpectate(m_Spectate.c_str());
			return retVal;
		}
	};

	struct MyDiscordActivity
	{
		auto operator<=>(const MyDiscordActivity&) const = default;

		discord::ActivityType m_Type{};
		std::int64_t m_ApplicationID{};
		std::string m_Name;
		std::string m_State;
		std::string m_Details;
		MyDiscordActivityTimestamps m_Timestamps;
		MyDiscordActivityAssets m_Assets;
		MyDiscordActivityParty m_Party;
		MyDiscordActivitySecrets m_Secrets;
		bool m_Instance{};

		operator discord::Activity() const
		{
			discord::Activity retVal{};
			retVal.SetType(m_Type);
			retVal.SetApplicationId(m_ApplicationID);
			retVal.SetName(m_Name.c_str());
			retVal.SetState(m_State.c_str());
			retVal.SetDetails(m_Details.c_str());
			retVal.GetTimestamps() = m_Timestamps;
			retVal.GetAssets() = m_Assets;
			retVal.GetParty() = m_Party;
			retVal.GetSecrets() = m_Secrets;
			retVal.SetInstance(m_Instance);
			return retVal;
		}
	};

	struct DiscordState final : IDRPManager, BaseWorldEventListener, IConsoleLineListener
	{
		static constexpr duration_t UPDATE_INTERVAL = 10s; // 20s / 5;

		DiscordState(WorldState& world);

		discord::Core* m_Core = nullptr;

		bool m_GameOpen = false;

		bool m_WantsUpdate = false;

		std::string m_MapName;
		discord::Activity m_NextActivity{};

		void QueueUpdate() { m_WantsUpdate = true; }
		void Update() override;

		void OnConsoleLineParsed(WorldState& world, IConsoleLine& line) override;
		void OnLocalPlayerSpawned(WorldState& world, TFClassType classType) override;

	private:
		time_point_t m_LastUpdate{};
		discord::Activity m_CurrentActivity{};
	};

	DiscordState::DiscordState(WorldState& world)
	{
		world.AddConsoleLineListener(this);
		world.AddWorldEventListener(this);

		if (auto result = discord::Core::Create(730945386390224976, DiscordCreateFlags_Default, &m_Core); result != discord::Result::Ok)
			throw std::runtime_error("Failed to initialize discord: "s << result);

		if (auto result = m_Core->ActivityManager().RegisterSteam(440); result != discord::Result::Ok)
			LogError("Failed to register discord integration as steam appid 440: "s << result);
	}

	void DiscordState::OnConsoleLineParsed(WorldState& world, IConsoleLine& line)
	{
		switch (line.GetType())
		{
		case ConsoleLineType::PlayerStatusMapPosition:
		{
			auto& statusLine = static_cast<const ServerStatusMapLine&>(line);
			const auto& mapName = statusLine.GetMapName();

			char buf[512];
			sprintf_s(buf, "map_%s", mapName.c_str());
			m_NextActivity.GetAssets().SetLargeImage(buf);

			m_NextActivity.SetDetails(mapName.c_str());
			m_GameOpen = true;
			QueueUpdate();
			break;
		}
		case ConsoleLineType::PartyHeader:
		{
			auto& partyLine = static_cast<const PartyHeaderLine&>(line);
			auto& partySize = m_NextActivity.GetParty().GetSize();
			partySize.SetCurrentSize(partyLine.GetParty().m_MemberCount);
			partySize.SetMaxSize(6);
			m_GameOpen = true;
			QueueUpdate();
			break;
		}
		case ConsoleLineType::LobbyHeader:
		{
			m_NextActivity.SetState("Casual");
			m_GameOpen = true;
			QueueUpdate();
			break;
		}
		case ConsoleLineType::LobbyStatusFailed:
		{
			m_MapName = "";
			m_NextActivity.GetAssets().SetLargeImage(DEFAULT_LARGE_IMAGE_KEY);
			m_NextActivity.SetState("Main Menu");
			m_GameOpen = true;
			QueueUpdate();
			break;
		}
		case ConsoleLineType::GameQuit:
		{
			m_GameOpen = false;
			QueueUpdate();
			break;
		}
		}
	}

	void DiscordState::OnLocalPlayerSpawned(WorldState& world, TFClassType classType)
	{
		auto& assets = m_NextActivity.GetAssets();
		switch (classType)
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
		}

		QueueUpdate();
	}
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
	case discord::LogLevel::Error:
		LogError(msg);
		break;
	}
}

void DiscordState::Update()
{
	if (m_WantsUpdate)
	{
		const auto curTime = clock_t::now();
		if ((curTime - m_LastUpdate) >= UPDATE_INTERVAL &&
			m_NextActivity != m_CurrentActivity)
		{
			// Perform the update
			m_Core->ActivityManager().UpdateActivity(m_NextActivity, [](discord::Result result)
				{
					if (result != discord::Result::Ok)
						LogWarning("Failed to update discord activity state: "s << result);
				});

			m_CurrentActivity = m_NextActivity;
			m_LastUpdate = curTime;
		}

		// Update successful
		m_WantsUpdate = false;
	}

	// Run discord callbacks
	if (auto result = m_Core->RunCallbacks(); result != discord::Result::Ok)
		LogError("Failed to run discord callbacks: "s << result);
}

#endif

std::unique_ptr<IDRPManager> IDRPManager::Create(WorldState& world)
{
	return std::make_unique<DiscordState>(world);
}
