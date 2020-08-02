#ifdef TF2BD_ENABLE_DISCORD_INTEGRATION
#include "DiscordRichPresence.h"
#include "ConsoleLog/ConsoleLines.h"
#include "Log.h"
#include "WorldState.h"

#include <mh/text/format.hpp>
#include <mh/text/string_insertion.hpp>
#include <discord-game-sdk/core.h>

#include <cassert>

using namespace std::chrono_literals;
using namespace std::string_literals;
using namespace tf2_bot_detector;


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

namespace
{
	struct DiscordState final : BaseWorldEventListener, IConsoleLineListener
	{
		static constexpr duration_t UPDATE_INTERVAL = 10s; // 20s / 5;

		DiscordState();

		discord::Core* m_Core = nullptr;

		time_point_t m_LastUpdate{};
		bool m_WantsUpdate = false;
		std::string m_SmallImageKey;
		std::string m_SmallImageTooltip;

		std::string m_MapName;

		bool TryUpdate();

		void OnConsoleLineParsed(WorldState& world, IConsoleLine& line) override;
		void OnLocalPlayerSpawned(WorldState& world, TFClassType classType) override;

	private:
		void UpdateImpl();
	};

	static DiscordState& GetDiscordState()
	{
		static DiscordState s_State;
		return s_State;
	}

	DiscordState::DiscordState()
	{
		if (auto result = discord::Core::Create(730945386390224976, DiscordCreateFlags_Default, &m_Core); result != discord::Result::Ok)
			throw std::runtime_error("Failed to initialize discord: "s << result);

		if (auto result = m_Core->ActivityManager().RegisterSteam(440); result != discord::Result::Ok)
			LogError("Failed to register discord integration as steam appid 440: "s << result);
	}

	bool DiscordState::TryUpdate()
	{
		auto curTime = clock_t::now();
		if ((curTime - m_LastUpdate) < UPDATE_INTERVAL)
		{
			m_WantsUpdate = true;
			return false;
		}

		UpdateImpl();
		return true;
	}

	void DiscordState::OnConsoleLineParsed(WorldState& world, IConsoleLine& line)
	{
		switch (line.GetType())
		{
		case ConsoleLineType::PlayerStatusMapPosition:
		{
			auto& statusLine = static_cast<const ServerStatusMapLine&>(line);
			m_MapName = statusLine.GetMapName();
			TryUpdate();
			break;
		}
		}
	}

	void DiscordState::OnLocalPlayerSpawned(WorldState& world, TFClassType classType)
	{
		switch (classType)
		{
		case TFClassType::Demoman:
			m_SmallImageKey = "leaderboard_class_demo";
			m_SmallImageTooltip = "Demo";
			break;
		case TFClassType::Engie:
			m_SmallImageKey = "leaderboard_class_engineer";
			m_SmallImageTooltip = "Engineer";
			break;
		case TFClassType::Heavy:
			m_SmallImageKey = "leaderboard_class_heavy";
			m_SmallImageTooltip = "Heavy";
			break;
		case TFClassType::Medic:
			m_SmallImageKey = "leaderboard_class_medic";
			m_SmallImageTooltip = "Medic";
			break;
		case TFClassType::Pyro:
			m_SmallImageKey = "leaderboard_class_pyro";
			m_SmallImageTooltip = "Pyro";
			break;
		case TFClassType::Scout:
			m_SmallImageKey = "leaderboard_class_scout";
			m_SmallImageTooltip = "Scout";
			break;
		case TFClassType::Sniper:
			m_SmallImageKey = "leaderboard_class_sniper";
			m_SmallImageTooltip = "Sniper";
			break;
		case TFClassType::Soldier:
			m_SmallImageKey = "leaderboard_class_soldier";
			m_SmallImageTooltip = "Soldier";
			break;
		case TFClassType::Spy:
			m_SmallImageKey = "leaderboard_class_spy";
			m_SmallImageTooltip = "Spy";
			break;
		}

		TryUpdate();
	}

	void DiscordState::UpdateImpl()
	{
		{
			auto curTime = clock_t::now();
			assert((curTime - m_LastUpdate) >= UPDATE_INTERVAL);
			m_LastUpdate = curTime;
		}

		discord::Activity act{};

		act.SetDetails("Casual");
		if (!m_MapName.empty())
			act.SetState(m_MapName.c_str());

		auto& party = act.GetParty();
		party.GetSize().SetCurrentSize(1);
		party.GetSize().SetMaxSize(6);

		auto& assets = act.GetAssets();
		assets.SetSmallImage(m_SmallImageKey.c_str());
		assets.SetSmallText(m_SmallImageTooltip.c_str());

#if 0
		assets.SetLargeImage("tf2_1x1");
#else
		assets.SetLargeImage(mh::format("map_{}", m_MapName).c_str());
#endif

		assets.SetLargeText("https://github.com/PazerOP/tf2_bot_detector");

		m_Core->ActivityManager().UpdateActivity(act, [](discord::Result result)
			{
				if (result != discord::Result::Ok)
					LogWarning("Failed to update discord activity state: "s << result);
			});
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

void Discord::Update()
{
	auto& state = GetDiscordState();

	if (state.m_WantsUpdate)
		state.TryUpdate();

	if (auto result = state.m_Core->RunCallbacks(); result != discord::Result::Ok)
		LogError("Failed to run discord callbacks: "s << result);
}

void Discord::AddEventListeners(WorldState& world)
{
	auto& state = GetDiscordState();
	world.AddConsoleLineListener(&state);
	world.AddWorldEventListener(&state);
}

#endif
