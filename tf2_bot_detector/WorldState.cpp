#include "WorldState.h"
#include "Actions/Actions.h"
#include "Config/Settings.h"
#include "ConsoleLog/ConsoleLineListener.h"
#include "ConsoleLog/ConsoleLines.h"
#include "ConsoleLog/ConsoleLogParser.h"
#include "GameData/TFClassType.h"
#include "GameData/UserMessageType.h"
#include "Networking/HTTPHelpers.h"
#include "Networking/SteamAPI.h"
#include "Networking/LogsTFAPI.h"
#include "Util/RegexUtils.h"
#include "Util/TextUtils.h"
#include "BatchedAction.h"
#include "GenericErrors.h"
#include "IPlayer.h"
#include "Log.h"
#include "WorldEventListener.h"
#include "Config/AccountAges.h"
#include "GlobalDispatcher.h"
#include "Application.h"
#include "DB/TempDB.h"

#include <mh/algorithm/algorithm.hpp>
#include <mh/concurrency/dispatcher.hpp>
#include <mh/concurrency/main_thread.hpp>
#include <mh/concurrency/thread_pool.hpp>
#include <mh/concurrency/thread_sentinel.hpp>
#include <mh/future.hpp>
#include <mh/coroutine/future.hpp>

#undef GetCurrentTime
#undef max
#undef min

using namespace std::chrono_literals;
using namespace std::string_literals;
using namespace std::string_view_literals;
using namespace tf2_bot_detector;

namespace
{
	class Player;

	class WorldState final : public IWorldState, BaseConsoleLineListener
	{
	public:
		WorldState(const Settings& settings);
		~WorldState();

		std::shared_ptr<WorldState> shared_from_this() { return std::static_pointer_cast<WorldState>(IWorldState::shared_from_this()); }
		std::shared_ptr<const WorldState> shared_from_this() const { return std::static_pointer_cast<const WorldState>(IWorldState::shared_from_this()); }

		time_point_t GetCurrentTime() const override { return m_CurrentTimestamp.GetSnapshot(); }
		size_t GetApproxLobbyMemberCount() const override;

		void Update() override;
		void UpdateTimestamp(const ConsoleLogParser& parser);

		void AddWorldEventListener(IWorldEventListener* listener) override;
		void RemoveWorldEventListener(IWorldEventListener* listener) override;
		void AddConsoleLineListener(IConsoleLineListener* listener) override;
		void RemoveConsoleLineListener(IConsoleLineListener* listener) override;

		void AddConsoleOutputChunk(const std::string_view& chunk) override;
		mh::task<> AddConsoleOutputLine(std::string line) override;

		std::optional<SteamID> FindSteamIDForName(const std::string_view& playerName) const override;
		std::optional<LobbyMemberTeam> FindLobbyMemberTeam(const SteamID& id) const override;
		std::optional<UserID_t> FindUserID(const SteamID& id) const override;

		TeamShareResult GetTeamShareResult(const SteamID& id) const override;
		TeamShareResult GetTeamShareResult(const SteamID& id0, const SteamID& id1) const override;
		TeamShareResult GetTeamShareResult(const std::optional<LobbyMemberTeam>& team0, const SteamID& id1) const override;
		static TeamShareResult GetTeamShareResult(
			const std::optional<LobbyMemberTeam>& team0, const std::optional<LobbyMemberTeam>& team1);

		using IWorldState::FindPlayer;
		const IPlayer* FindPlayer(const SteamID& id) const override;

		mh::generator<const IPlayer&> GetLobbyMembers() const;
		mh::generator<const IPlayer&> GetPlayers() const;
		std::vector<const IPlayer*> GetRecentPlayers(size_t recentPlayerCount = 32) const;
		std::vector<IPlayer*> GetRecentPlayers(size_t recentPlayerCount = 32);

		time_point_t GetLastStatusUpdateTime() const { return m_LastStatusUpdateTime; }

		// Have we joined a team and picked a class?
		bool IsLocalPlayerInitialized() const override { return m_IsLocalPlayerInitialized; }
		bool IsVoteInProgress() const override { return m_IsVoteInProgress; }

		void QueuePlayerSummaryUpdate(const SteamID& id);
		void QueuePlayerBansUpdate(const SteamID& id);

		const Settings& GetSettings() const { return m_Settings; }
		const std::vector<LobbyMember>& GetCurrentLobbyMembers() const { return m_CurrentLobbyMembers; }
		const std::vector<LobbyMember>& GetPendingLobbyMembers() const { return m_PendingLobbyMembers; }
		const std::unordered_set<SteamID>& GetFriends() const { return m_Friends; }

		IAccountAges& GetAccountAges() { return *m_AccountAges; }
		const IAccountAges& GetAccountAges() const override { return *m_AccountAges; }

	protected:
		virtual IConsoleLineListener& GetConsoleLineListenerBroadcaster() { return m_ConsoleLineListenerBroadcaster; }

	private:
		const Settings& m_Settings;

		CompensatedTS m_CurrentTimestamp;

		void OnConsoleLineParsed(IWorldState& world, IConsoleLine& parsed) override;
		void OnConfigExecLineParsed(const ConfigExecLine& execLine);

		void UpdateFriends();
		mh::task<std::unordered_set<SteamID>> m_FriendsFuture;
		std::unordered_set<SteamID> m_Friends;
		time_point_t m_LastFriendsUpdate{};

		Player& FindOrCreatePlayer(const SteamID& id);

		struct PlayerSummaryUpdateAction final :
			BatchedAction<WorldState*, SteamID, std::vector<SteamAPI::PlayerSummary>>
		{
			using BatchedAction::BatchedAction;
		protected:
			response_future_type SendRequest(WorldState*& state, queue_collection_type& collection) override;
			void OnDataReady(WorldState*& state, const response_type& response,
				queue_collection_type& collection) override;
		} m_PlayerSummaryUpdates;

		struct PlayerBansUpdateAction final :
			BatchedAction<WorldState*, SteamID, std::vector<SteamAPI::PlayerBans>>
		{
			using BatchedAction::BatchedAction;
		protected:
			response_future_type SendRequest(state_type& state, queue_collection_type& collection) override;
			void OnDataReady(state_type& state, const response_type& response,
				queue_collection_type& collection) override;
		} m_PlayerBansUpdates;

		std::vector<LobbyMember> m_CurrentLobbyMembers;
		std::vector<LobbyMember> m_PendingLobbyMembers;
		std::unordered_map<SteamID, std::shared_ptr<Player>> m_CurrentPlayerData;
		bool m_IsLocalPlayerInitialized = false;
		bool m_IsVoteInProgress = false;

		std::shared_ptr<IAccountAges> m_AccountAges = IAccountAges::Create();

		time_point_t m_LastStatusUpdateTime{};

		std::unordered_set<IConsoleLineListener*> m_ConsoleLineListeners;
		std::unordered_set<IWorldEventListener*> m_EventListeners;

		mh::thread_pool m_ConsoleLineParsingPool{ 1 };
		std::vector<mh::shared_future<std::shared_ptr<IConsoleLine>>> m_ConsoleLineParsingTasks;

		struct ConsoleLineListenerBroadcaster final : IConsoleLineListener
		{
			ConsoleLineListenerBroadcaster(WorldState& world) : m_World(world) {}

			void OnConsoleLineParsed(IWorldState& world, IConsoleLine& line) override
			{
				for (IConsoleLineListener* l : m_World.m_ConsoleLineListeners)
					l->OnConsoleLineParsed(world, line);
			}
			void OnConsoleLineUnparsed(IWorldState& world, const std::string_view& text) override
			{
				for (IConsoleLineListener* l : m_World.m_ConsoleLineListeners)
					l->OnConsoleLineUnparsed(world, text);
			}
			void OnConsoleLogChunkParsed(IWorldState& world, bool consoleLinesParsed) override
			{
				for (IConsoleLineListener* l : m_World.m_ConsoleLineListeners)
					l->OnConsoleLogChunkParsed(world, consoleLinesParsed);
			}

			WorldState& m_World;

		} m_ConsoleLineListenerBroadcaster;
		template<typename TRet, typename... TArgs, typename... TArgs2>
		inline void InvokeEventListener(TRet(IWorldEventListener::* func)(TArgs... args), TArgs2&&... args)
		{
			for (IWorldEventListener* listener : m_EventListeners)
				(listener->*func)(std::forward<TArgs2>(args)...);
		}
	};

	class Player final : public IPlayer
	{
	public:
		Player(WorldState& world, SteamID id);

		WorldState& GetWorld() override { return static_cast<WorldState&>(IPlayer::GetWorld()); }
		const WorldState& GetWorld() const override;
		const LobbyMember* GetLobbyMember() const override;
		std::string GetNameUnsafe() const override { return m_Status.m_Name; }
		SteamID GetSteamID() const override { return m_Status.m_SteamID; }
		PlayerStatusState GetConnectionState() const override { return m_Status.m_State; }
		std::optional<UserID_t> GetUserID() const override;
		TFTeam GetTeam() const override { return m_Team; }
		time_point_t GetConnectionTime() const override { return m_Status.m_ConnectionTime; }
		duration_t GetConnectedTime() const override;
		const PlayerScores& GetScores() const override { return m_Scores; }
		uint16_t GetPing() const override { return m_Status.m_Ping; }
		time_point_t GetLastStatusUpdateTime() const override { return m_LastStatusUpdateTime; }
		const mh::expected<SteamAPI::PlayerSummary>& GetPlayerSummary() const override;
		const mh::expected<SteamAPI::PlayerBans>& GetPlayerBans() const override;
		mh::expected<duration_t> GetTF2Playtime() const override;
		bool IsFriend() const override;
		duration_t GetActiveTime() const override;

		std::optional<time_point_t> GetEstimatedAccountCreationTime() const override;
		const mh::expected<LogsTFAPI::PlayerLogsInfo>& GetLogsInfo() const override;
		const mh::expected<SteamAPI::PlayerInventoryInfo>& GetInventoryInfo() const override;

		PlayerScores m_Scores{};
		TFTeam m_Team{};

		uint8_t m_ClientIndex{};
		mutable mh::expected<SteamAPI::PlayerSummary> m_PlayerSummary = ErrorCode::LazyValueUninitialized;
		mutable mh::expected<SteamAPI::PlayerBans> m_PlayerSteamBans = ErrorCode::LazyValueUninitialized;

		void SetStatus(PlayerStatus status, time_point_t timestamp);
		const PlayerStatus& GetStatus() const { return m_Status; }

		void SetPing(uint16_t ping, time_point_t timestamp);

	protected:
		std::map<std::type_index, std::any> m_UserData;
		const std::any* FindDataStorage(const std::type_index& type) const override;
		std::any& GetOrCreateDataStorage(const std::type_index& type) override;

		std::shared_ptr<Player> shared_from_this() { return std::static_pointer_cast<Player>(IPlayer::shared_from_this()); }
		std::shared_ptr<const Player> shared_from_this() const { return std::static_pointer_cast<const Player>(IPlayer::shared_from_this()); }

	private:
		mh::thread_sentinel m_Sentinel;

		template<typename T, typename TFunc>
		const mh::expected<T>& GetOrFetchDataAsync(mh::expected<T>& variable, TFunc&& updateFunc,
			std::initializer_list<std::error_condition> silentErrors = {}, MH_SOURCE_LOCATION_AUTO(location)) const;

		WorldState* m_World = nullptr;
		PlayerStatus m_Status{};

		time_point_t m_LastStatusActiveBegin{};

		time_point_t m_LastStatusUpdateTime{};
		time_point_t m_LastPingUpdateTime{};

		mutable mh::expected<duration_t> m_TF2Playtime = ErrorCode::LazyValueUninitialized;
		mutable mh::expected<LogsTFAPI::PlayerLogsInfo> m_LogsInfo = ErrorCode::LazyValueUninitialized;
		mutable mh::expected<SteamAPI::PlayerInventoryInfo> m_InventoryInfo = ErrorCode::LazyValueUninitialized;
	};
}

std::shared_ptr<IWorldState> IWorldState::Create(const Settings& settings)
{
	return std::make_shared<WorldState>(settings);
}

WorldState::WorldState(const Settings& settings) :
	m_Settings(settings),
	m_PlayerSummaryUpdates(this),
	m_PlayerBansUpdates(this),
	m_ConsoleLineListenerBroadcaster(*this)
{
	AddConsoleLineListener(this);
}

WorldState::~WorldState()
{
	RemoveConsoleLineListener(this);
}

void WorldState::Update()
{
	m_PlayerSummaryUpdates.Update();
	m_PlayerBansUpdates.Update();

	UpdateFriends();
}

void WorldState::UpdateFriends()
{
	if (auto client = GetSettings().GetHTTPClient();
		client && GetSettings().IsSteamAPIAvailable() && (tfbd_clock_t::now() - 5min) > m_LastFriendsUpdate)
	{
		m_LastFriendsUpdate = tfbd_clock_t::now();
		m_FriendsFuture = SteamAPI::GetFriendList(GetSettings(), GetSettings().GetLocalSteamID(), *client);
	}

	if (m_FriendsFuture.is_ready())
	{
		const auto GenericException = [this](const mh::source_location& loc)
		{
			LogException(loc, "Failed to update our friends list");
			m_FriendsFuture = {};
		};

		try
		{
			m_Friends = m_FriendsFuture.get();
		}
		catch (const http_error& e)
		{
			if (e.code() == HTTPResponseCode::Unauthorized)
			{
				static bool s_HasWarned = false;
				constexpr const char WARNING_MSG[] = "Failed to access our friends list (our friends list is private/friends only, and the Steam API is bugged). The tool will not be able to show who is friends with you.";

				if (!s_HasWarned)
				{
					LogWarning(WARNING_MSG);
					s_HasWarned = true;
				}
				else
				{
					DebugLogWarning(WARNING_MSG);
				}
			}
			else
			{
				GenericException(MH_SOURCE_LOCATION_CURRENT());
			}
		}
		catch (...)
		{
			GenericException(MH_SOURCE_LOCATION_CURRENT());
		}
	}
}

void WorldState::AddConsoleLineListener(IConsoleLineListener* listener)
{
	m_ConsoleLineListeners.insert(listener);
}

void WorldState::RemoveConsoleLineListener(IConsoleLineListener* listener)
{
	m_ConsoleLineListeners.erase(listener);
}

void WorldState::AddConsoleOutputChunk(const std::string_view& chunk)
{
	size_t last = 0;
	for (auto i = chunk.find('\n', 0); i != chunk.npos; i = chunk.find('\n', last))
	{
		auto line = chunk.substr(last, i - last);
		AddConsoleOutputLine(std::string(line));
		last = i + 1;
	}
}

mh::task<> WorldState::AddConsoleOutputLine(std::string line)
{
	auto worldState = shared_from_this();

	// Switch to thread "pool" thread (there is only 1 thread in this particular pool)
	co_await m_ConsoleLineParsingPool.co_add_task();

	auto parsed = IConsoleLine::ParseConsoleLine(line, GetCurrentTime(), *this);

	// switch to main thread
	co_await GetDispatcher().co_dispatch();

	if (parsed)
	{
		for (auto listener : m_ConsoleLineListeners)
			listener->OnConsoleLineParsed(*worldState, *parsed);
	}
	else
	{
		for (auto listener : m_ConsoleLineListeners)
			listener->OnConsoleLineUnparsed(*worldState, std::move(line));
	}
}

void WorldState::UpdateTimestamp(const ConsoleLogParser& parser)
{
	m_CurrentTimestamp = parser.GetCurrentTimestamp();
}

void WorldState::AddWorldEventListener(IWorldEventListener* listener)
{
	m_EventListeners.insert(listener);
}

void WorldState::RemoveWorldEventListener(IWorldEventListener* listener)
{
	m_EventListeners.erase(listener);
}

std::optional<SteamID> WorldState::FindSteamIDForName(const std::string_view& playerName) const
{
	std::optional<SteamID> retVal;
	time_point_t lastUpdated{};

	for (const auto& data : m_CurrentPlayerData)
	{
		if (data.second->GetStatus().m_Name == playerName && data.second->GetLastStatusUpdateTime() > lastUpdated)
		{
			retVal = data.second->GetSteamID();
			lastUpdated = data.second->GetLastStatusUpdateTime();
		}
	}

	return retVal;
}

std::optional<LobbyMemberTeam> WorldState::FindLobbyMemberTeam(const SteamID& id) const
{
	for (const auto& member : m_CurrentLobbyMembers)
	{
		if (member.m_SteamID == id)
			return member.m_Team;
	}

	for (const auto& member : m_PendingLobbyMembers)
	{
		if (member.m_SteamID == id)
			return member.m_Team;
	}

	return std::nullopt;
}

std::optional<UserID_t> WorldState::FindUserID(const SteamID& id) const
{
	for (const auto& player : m_CurrentPlayerData)
	{
		if (player.second->GetSteamID() == id)
			return player.second->GetUserID();
	}

	return std::nullopt;
}

TeamShareResult WorldState::GetTeamShareResult(const SteamID& id) const
{
	return GetTeamShareResult(id, GetSettings().GetLocalSteamID());
}

auto WorldState::GetTeamShareResult(const std::optional<LobbyMemberTeam>& team0,
	const SteamID& id1) const -> TeamShareResult
{
	return GetTeamShareResult(team0, FindLobbyMemberTeam(id1));
}

auto WorldState::GetTeamShareResult(const std::optional<LobbyMemberTeam>& team0,
	const std::optional<LobbyMemberTeam>& team1) -> TeamShareResult
{
	if (!team0)
		return TeamShareResult::Neither;
	if (!team1)
		return TeamShareResult::Neither;

	if (*team0 == *team1)
		return TeamShareResult::SameTeams;
	else if (*team0 == OppositeTeam(*team1))
		return TeamShareResult::OppositeTeams;
	else
		throw std::runtime_error("Unexpected team value(s)");
}

const IPlayer* WorldState::FindPlayer(const SteamID& id) const
{
	if (auto found = m_CurrentPlayerData.find(id); found != m_CurrentPlayerData.end())
		return found->second.get();

	return nullptr;
}

size_t WorldState::GetApproxLobbyMemberCount() const
{
	return m_CurrentLobbyMembers.size() + m_PendingLobbyMembers.size();
}

mh::generator<const IPlayer&> WorldState::GetLobbyMembers() const
{
	const auto GetPlayer = [&](const LobbyMember& member) -> const IPlayer*
	{
		assert(member != LobbyMember{});
		assert(member.m_SteamID.IsValid());

		if (auto found = m_CurrentPlayerData.find(member.m_SteamID); found != m_CurrentPlayerData.end())
		{
			[[maybe_unused]] const LobbyMember* testMember = found->second->GetLobbyMember();
			//assert(*testMember == member);
			return found->second.get();
		}
		else
		{
			throw std::runtime_error("Missing player for lobby member!");
		}
	};

	for (const auto& member : m_CurrentLobbyMembers)
	{
		if (!member.IsValid())
			continue;

		if (auto found = GetPlayer(member))
			co_yield *found;
	}
	for (const auto& member : m_PendingLobbyMembers)
	{
		if (!member.IsValid())
			continue;

		if (std::any_of(m_CurrentLobbyMembers.begin(), m_CurrentLobbyMembers.end(),
			[&](const LobbyMember& member2) { return member.m_SteamID == member2.m_SteamID; }))
		{
			// Don't return two different instances with the same steamid.
			continue;
		}

		if (auto found = GetPlayer(member))
			co_yield *found;
	}
}

mh::generator<const IPlayer&> WorldState::GetPlayers() const
{
	for (const auto& pair : m_CurrentPlayerData)
		co_yield *pair.second;
}

void WorldState::QueuePlayerSummaryUpdate(const SteamID& id)
{
	return m_PlayerSummaryUpdates.Queue(id);
}

void WorldState::QueuePlayerBansUpdate(const SteamID& id)
{
	return m_PlayerBansUpdates.Queue(id);
}

template<typename TMap>
static auto GetRecentPlayersImpl(TMap&& map, size_t recentPlayerCount)
{
	using value_type = std::conditional_t<std::is_const_v<std::remove_reference_t<TMap>>, const IPlayer*, IPlayer*>;
	std::vector<value_type> retVal;

	for (auto& [sid, player] : map)
		retVal.push_back(player.get());

	std::sort(retVal.begin(), retVal.end(),
		[](const IPlayer* a, const IPlayer* b)
		{
			return b->GetLastStatusUpdateTime() < a->GetLastStatusUpdateTime();
		});

	if (retVal.size() > recentPlayerCount)
		retVal.resize(recentPlayerCount);

	return retVal;
}

std::vector<const IPlayer*> WorldState::GetRecentPlayers(size_t recentPlayerCount) const
{
	return GetRecentPlayersImpl(m_CurrentPlayerData, recentPlayerCount);
}

std::vector<IPlayer*> WorldState::GetRecentPlayers(size_t recentPlayerCount)
{
	return GetRecentPlayersImpl(m_CurrentPlayerData, recentPlayerCount);
}

void WorldState::OnConfigExecLineParsed(const ConfigExecLine& execLine)
{
	const std::string_view& cfgName = execLine.GetConfigFileName();
	if (cfgName == "scout.cfg"sv ||
		cfgName == "sniper.cfg"sv ||
		cfgName == "soldier.cfg"sv ||
		cfgName == "demoman.cfg"sv ||
		cfgName == "medic.cfg"sv ||
		cfgName == "heavyweapons.cfg"sv ||
		cfgName == "pyro.cfg"sv ||
		cfgName == "spy.cfg"sv ||
		cfgName == "engineer.cfg"sv)
	{
		DebugLog("Spawned as {}", cfgName.substr(0, cfgName.size() - 3));

		TFClassType cl = TFClassType::Undefined;
		if (cfgName.starts_with("scout"))
			cl = TFClassType::Scout;
		else if (cfgName.starts_with("sniper"))
			cl = TFClassType::Sniper;
		else if (cfgName.starts_with("soldier"))
			cl = TFClassType::Soldier;
		else if (cfgName.starts_with("demoman"))
			cl = TFClassType::Demoman;
		else if (cfgName.starts_with("medic"))
			cl = TFClassType::Medic;
		else if (cfgName.starts_with("heavyweapons"))
			cl = TFClassType::Heavy;
		else if (cfgName.starts_with("pyro"))
			cl = TFClassType::Pyro;
		else if (cfgName.starts_with("spy"))
			cl = TFClassType::Spy;
		else if (cfgName.starts_with("engineer"))
			cl = TFClassType::Engie;

		InvokeEventListener(&IWorldEventListener::OnLocalPlayerSpawned, *this, cl);

		if (!m_IsLocalPlayerInitialized)
		{
			m_IsLocalPlayerInitialized = true;
			InvokeEventListener(&IWorldEventListener::OnLocalPlayerInitialized, *this, m_IsLocalPlayerInitialized);
		}
	}
}

void WorldState::OnConsoleLineParsed(IWorldState& world, IConsoleLine& parsed)
{
	assert(&world == this);

	const auto ClearLobbyState = [&]
	{
		m_CurrentLobbyMembers.clear();
		m_PendingLobbyMembers.clear();
		m_CurrentPlayerData.clear();
	};

	switch (parsed.GetType())
	{
	case ConsoleLineType::LobbyHeader:
	{
		auto& headerLine = static_cast<const LobbyHeaderLine&>(parsed);
		m_CurrentLobbyMembers.resize(headerLine.GetMemberCount());
		m_PendingLobbyMembers.resize(headerLine.GetPendingCount());
		break;
	}
	case ConsoleLineType::LobbyStatusFailed:
	{
		if (!m_CurrentLobbyMembers.empty() || !m_PendingLobbyMembers.empty())
		{
			m_CurrentLobbyMembers.clear();
			m_PendingLobbyMembers.clear();
			m_CurrentPlayerData.clear();
		}
		break;
	}
	case ConsoleLineType::LobbyChanged:
	{
		auto& lobbyChangedLine = static_cast<const LobbyChangedLine&>(parsed);
		const LobbyChangeType changeType = lobbyChangedLine.GetChangeType();

		if (changeType == LobbyChangeType::Created)
		{
			ClearLobbyState();
		}

		if (changeType == LobbyChangeType::Created || changeType == LobbyChangeType::Updated)
		{
			// We can't trust the existing client indices
			for (auto& player : m_CurrentPlayerData)
				player.second->m_ClientIndex = 0;
		}
		break;
	}
	case ConsoleLineType::HostNewGame:
	case ConsoleLineType::Connecting:
	case ConsoleLineType::ClientReachedServerSpawn:
	{
		if (m_IsLocalPlayerInitialized)
		{
			m_IsLocalPlayerInitialized = false;
			InvokeEventListener(&IWorldEventListener::OnLocalPlayerInitialized, *this, m_IsLocalPlayerInitialized);
		}

		m_IsVoteInProgress = false;
		break;
	}
	case ConsoleLineType::Chat:
	{
		auto& chatLine = static_cast<const ChatConsoleLine&>(parsed);
		if (auto sid = FindSteamIDForName(chatLine.GetPlayerName()))
		{
			DebugLog("Chat message from {}: {}", *sid, std::quoted(chatLine.GetMessage()));
			if (auto player = FindPlayer(*sid))
			{
				InvokeEventListener(&IWorldEventListener::OnChatMsg, *this, *player, chatLine.GetMessage());
			}
			else
			{
				LogWarning("Dropped chat message with unknown IPlayer from {} ({})",
					std::quoted(chatLine.GetPlayerName()), std::quoted(chatLine.GetMessage()));
			}
		}
		else
		{
			LogWarning("Dropped chat message with unknown SteamID from {}: {}",
				std::quoted(chatLine.GetPlayerName()), std::quoted(chatLine.GetMessage()));
		}

		break;
	}
	case ConsoleLineType::ServerDroppedPlayer:
	{
		auto& dropLine = static_cast<const ServerDroppedPlayerLine&>(parsed);
		if (auto sid = FindSteamIDForName(dropLine.GetPlayerName()))
		{
			if (auto player = FindPlayer(*sid))
			{
				InvokeEventListener(&IWorldEventListener::OnPlayerDroppedFromServer,
					*this, *player, dropLine.GetReason());
			}
			else
			{
				LogWarning("Dropped \"player dropped\" message with unknown IPlayer from {} ({})",
					std::quoted(dropLine.GetPlayerName()), *sid);
			}
		}
		else
		{
			LogWarning("Dropped \"player dropped\" message with unknown SteamID from {}",
				std::quoted(dropLine.GetPlayerName()));
		}
		break;
	}
	case ConsoleLineType::ConfigExec:
	{
		OnConfigExecLineParsed(static_cast<const ConfigExecLine&>(parsed));
		break;
	}

#if 0
	case ConsoleLineType::VoiceReceive:
	{
		auto& voiceReceiveLine = static_cast<const VoiceReceiveLine&>(parsed);
		for (auto& player : m_CurrentPlayerData)
		{
			if (player.second.m_ClientIndex == (voiceReceiveLine.GetEntIndex() + 1))
			{
				auto& voice = player.second.m_Voice;
				if (voice.m_LastTransmission != parsed.GetTimestamp())
				{
					voice.m_TotalTransmissions += 1s; // This is fine because we know the resolution of our timestamps is 1 second
					voice.m_LastTransmission = parsed.GetTimestamp();
				}

				break;
			}
		}
		break;
	}
#endif

	case ConsoleLineType::LobbyMember:
	{
		auto& memberLine = static_cast<const LobbyMemberLine&>(parsed);
		const auto& member = memberLine.GetLobbyMember();
		auto& vec = member.m_Pending ? m_PendingLobbyMembers : m_CurrentLobbyMembers;
		if (member.m_Index < vec.size())
			vec[member.m_Index] = member;

		const TFTeam tfTeam = member.m_Team == LobbyMemberTeam::Defenders ? TFTeam::Red : TFTeam::Blue;
		FindOrCreatePlayer(member.m_SteamID).m_Team = tfTeam;

		break;
	}
	case ConsoleLineType::Ping:
	{
		auto& pingLine = static_cast<const PingLine&>(parsed);
		if (auto found = FindSteamIDForName(pingLine.GetPlayerName()))
		{
			auto& playerData = FindOrCreatePlayer(*found);
			playerData.SetPing(pingLine.GetPing(), pingLine.GetTimestamp());
		}

		break;
	}
	case ConsoleLineType::PlayerStatus:
	{
		auto& statusLine = static_cast<const ServerStatusPlayerLine&>(parsed);
		auto newStatus = statusLine.GetPlayerStatus();
		auto& playerData = FindOrCreatePlayer(newStatus.m_SteamID);

		// Don't introduce stutter to our connection time view
		if (auto delta = (playerData.GetStatus().m_ConnectionTime - newStatus.m_ConnectionTime);
			delta < 2s && delta > -2s)
		{
			newStatus.m_ConnectionTime = playerData.GetStatus().m_ConnectionTime;
		}

		assert(playerData.GetStatus().m_SteamID == newStatus.m_SteamID);
		playerData.SetStatus(newStatus, statusLine.GetTimestamp());
		m_LastStatusUpdateTime = std::max(m_LastStatusUpdateTime, playerData.GetLastStatusUpdateTime());
		InvokeEventListener(&IWorldEventListener::OnPlayerStatusUpdate, *this, playerData);

		break;
	}
	case ConsoleLineType::PlayerStatusShort:
	{
		auto& statusLine = static_cast<const ServerStatusShortPlayerLine&>(parsed);
		const auto& status = statusLine.GetPlayerStatus();
		if (auto steamID = FindSteamIDForName(status.m_Name))
			FindOrCreatePlayer(*steamID).m_ClientIndex = status.m_ClientIndex;

		break;
	}
	case ConsoleLineType::KillNotification:
	{
		auto& killLine = static_cast<const KillNotificationLine&>(parsed);
		const auto localSteamID = GetSettings().GetLocalSteamID();
		const auto attackerSteamID = FindSteamIDForName(killLine.GetAttackerName());
		const auto victimSteamID = FindSteamIDForName(killLine.GetVictimName());

		if (attackerSteamID)
		{
			auto& attacker = FindOrCreatePlayer(*attackerSteamID);
			attacker.m_Scores.m_Kills++;

			if (victimSteamID == localSteamID)
				attacker.m_Scores.m_LocalKills++;
		}

		if (victimSteamID)
		{
			auto& victim = FindOrCreatePlayer(*victimSteamID);
			victim.m_Scores.m_Deaths++;

			if (attackerSteamID == localSteamID)
				victim.m_Scores.m_LocalDeaths++;
		}

		break;
	}
	case ConsoleLineType::SVC_UserMessage:
	{
		auto& userMsg = static_cast<const SVCUserMessageLine&>(parsed);
		switch (userMsg.GetUserMessageType())
		{
		case UserMessageType::VoteStart:
			m_IsVoteInProgress = true;
			break;
		case UserMessageType::VoteFailed:
		case UserMessageType::VotePass:
			m_IsVoteInProgress = false;
			break;
		default:
			break;
		}

		break;
	}

#if 0
	case ConsoleLineType::NetDataTotal:
	{
		auto& netDataLine = static_cast<const NetDataTotalLine&>(parsed);
		auto ts = round_time_point(netDataLine.GetTimestamp(), 100ms);
		m_NetSamplesIn.m_Data[ts].AddSample(netDataLine.GetInKBps());
		m_NetSamplesOut.m_Data[ts].AddSample(netDataLine.GetOutKBps());
		break;
	}
	case ConsoleLineType::NetLatency:
	{
		auto& netLatencyLine = static_cast<const NetLatencyLine&>(parsed);
		auto ts = round_time_point(netLatencyLine.GetTimestamp(), 100ms);
		m_NetSamplesIn.m_Latency[ts].AddSample(netLatencyLine.GetInLatency());
		m_NetSamplesOut.m_Latency[ts].AddSample(netLatencyLine.GetOutLatency());
		break;
	}
	case ConsoleLineType::NetPacketsTotal:
	{
		auto& netPacketsLine = static_cast<const NetPacketsTotalLine&>(parsed);
		auto ts = round_time_point(netPacketsLine.GetTimestamp(), 100ms);
		m_NetSamplesIn.m_Packets[ts].AddSample(netPacketsLine.GetInPacketsPerSecond());
		m_NetSamplesOut.m_Packets[ts].AddSample(netPacketsLine.GetOutPacketsPerSecond());
		break;
	}
	case ConsoleLineType::NetLoss:
	{
		auto& netLossLine = static_cast<const NetLossLine&>(parsed);
		auto ts = round_time_point(netLossLine.GetTimestamp(), 100ms);
		m_NetSamplesIn.m_Loss[ts].AddSample(netLossLine.GetInLossPercent());
		m_NetSamplesOut.m_Loss[ts].AddSample(netLossLine.GetOutLossPercent());
		break;
	}
#endif

	default:
		break;
	}
}

Player& WorldState::FindOrCreatePlayer(const SteamID& id)
{
	Player* data;
	if (auto found = m_CurrentPlayerData.find(id); found != m_CurrentPlayerData.end())
	{
		data = found->second.get();
	}
	else
	{
		data = m_CurrentPlayerData.emplace(id, std::make_shared<Player>(*this, id)).first->second.get();

		if (!GetSettings().m_LazyLoadAPIData)
		{
			data->GetPlayerSummary();
			data->GetPlayerBans();
			data->GetTF2Playtime();
			data->GetLogsInfo();
			data->GetInventoryInfo();
		}
	}


	assert(data->GetSteamID() == id);
	return *data;
}

auto WorldState::GetTeamShareResult(const SteamID& id0, const SteamID& id1) const -> TeamShareResult
{
	return GetTeamShareResult(FindLobbyMemberTeam(id0), FindLobbyMemberTeam(id1));
}

Player::Player(WorldState& world, SteamID id) :
	m_World(&world)
{
	m_Status.m_SteamID = id;
}

const WorldState& Player::GetWorld() const
{
	assert(m_World);
	return *m_World;
}

const LobbyMember* Player::GetLobbyMember() const
{
	const auto steamID = GetSteamID();
	for (const auto& member : m_World->GetCurrentLobbyMembers())
	{
		if (member.m_SteamID == steamID)
			return &member;
	}
	for (const auto& member : m_World->GetPendingLobbyMembers())
	{
		if (member.m_SteamID == steamID)
			return &member;
	}

	return nullptr;
}

std::optional<UserID_t> Player::GetUserID() const
{
	if (m_Status.m_UserID > 0)
		return m_Status.m_UserID;

	return std::nullopt;
}

duration_t Player::GetConnectedTime() const
{
	auto result = GetWorld().GetCurrentTime() - GetConnectionTime();
	//assert(result >= -1s);
	result = std::max<duration_t>(result, 0s);
	return result;
}

const mh::expected<SteamAPI::PlayerSummary>& Player::GetPlayerSummary() const
{
	if (!m_PlayerSummary && m_PlayerSummary.error() == ErrorCode::LazyValueUninitialized)
	{
		m_PlayerSummary = std::errc::operation_in_progress;
		m_World->QueuePlayerSummaryUpdate(GetSteamID());
	}

	return m_PlayerSummary;
}

const mh::expected<SteamAPI::PlayerBans>& Player::GetPlayerBans() const
{
	if (!m_PlayerSteamBans && m_PlayerSteamBans.error() == ErrorCode::LazyValueUninitialized)
	{
		m_PlayerSteamBans = std::errc::operation_in_progress;
		m_World->QueuePlayerBansUpdate(GetSteamID());
	}

	return m_PlayerSteamBans;
}

template<typename T, typename TFunc>
const mh::expected<T>& Player::GetOrFetchDataAsync(mh::expected<T>& var, TFunc&& updateFunc,
	std::initializer_list<std::error_condition> silentErrors, const mh::source_location& location) const
{
	m_Sentinel.check(location);

	if (var == ErrorCode::LazyValueUninitialized ||
		var == ErrorCode::InternetConnectivityDisabled)
	{
		auto client = m_World->GetSettings().GetHTTPClient();

		if (!client)
		{
			var = ErrorCode::InternetConnectivityDisabled;
		}
		else
		{
			var = std::errc::operation_in_progress;

			auto sharedThis = shared_from_this();

			[](std::shared_ptr<const Player> sharedThis, std::shared_ptr<const IHTTPClient> client,
				mh::expected<T>& var, std::vector<std::error_condition> silentErrors, TFunc updateFunc,
				mh::source_location location) -> mh::task<>
			{
				try
				{
					mh::expected<T> result;
					try
					{
						result = co_await updateFunc(sharedThis, client);
					}
					catch (const std::system_error& e)
					{
						result = e.code().default_error_condition();

						if (!mh::contains(silentErrors, result.error()))
							DebugLogException(location, e);
					}
					catch (...)
					{
						LogException(location);
						result = ErrorCode::UnknownError;
					}

					co_await GetDispatcher().co_dispatch();  // switch to main thread

					var = std::move(result);
				}
				catch (...)
				{
					LogException(location);
				}

			}(sharedThis, client, var, silentErrors, std::move(updateFunc), location);
		}
	}

	return var;
}

const mh::expected<LogsTFAPI::PlayerLogsInfo>& Player::GetLogsInfo() const
{
	return GetOrFetchDataAsync(m_LogsInfo,
		[&](std::shared_ptr<const Player> pThis, auto client) -> mh::task<LogsTFAPI::PlayerLogsInfo>
		{
			DB::ITempDB& cacheDB = TF2BDApplication::GetApplication().GetTempDB();

			DB::LogsTFCacheInfo cacheInfo{};
			cacheInfo.m_ID = pThis->GetSteamID();

			co_await cacheDB.GetOrUpdateAsync(cacheInfo, [client](DB::LogsTFCacheInfo& info) -> mh::task<>
				{
					info = co_await LogsTFAPI::GetPlayerLogsInfoAsync(client, info.m_ID);
				});

			co_return cacheInfo;
		});
}

const mh::expected<SteamAPI::PlayerInventoryInfo>& Player::GetInventoryInfo() const
{
	return GetOrFetchDataAsync(m_InventoryInfo,
		[&](std::shared_ptr<const Player> pThis, auto client) -> mh::task<mh::expected<SteamAPI::PlayerInventoryInfo>>
		{
			DB::ITempDB& cacheDB = TF2BDApplication::GetApplication().GetTempDB();

			DB::AccountInventorySizeInfo cacheInfo{};
			cacheInfo.m_SteamID = pThis->GetSteamID();
			cacheInfo.m_Items = cacheInfo.m_Slots = (uint32_t)-1; // sanity check

			const auto& settings = pThis->GetWorld().GetSettings();
			if (!settings.IsSteamAPIAvailable())
				co_return SteamAPI::ErrorCode::SteamAPIDisabled;

			co_await cacheDB.GetOrUpdateAsync(cacheInfo, [&settings, client](DB::AccountInventorySizeInfo& info) -> mh::task<>
				{
					info = co_await SteamAPI::GetTF2InventoryInfoAsync(settings, info.GetSteamID(), *client);
				});

			co_return cacheInfo;
		});
}

mh::expected<duration_t> Player::GetTF2Playtime() const
{
	using ErrorCode = SteamAPI::ErrorCode;

	return GetOrFetchDataAsync(m_TF2Playtime,
		[&](std::shared_ptr<const Player> pThis, std::shared_ptr<const IHTTPClient> client) -> mh::task<mh::expected<duration_t>>
		{
			const auto& settings = pThis->GetWorld().GetSettings();
			if (!settings.IsSteamAPIAvailable())
				co_return ErrorCode::SteamAPIDisabled;

			co_return co_await SteamAPI::GetTF2PlaytimeAsync(settings, GetSteamID(), *client);
		}, { ErrorCode::InfoPrivate, ErrorCode::GameNotOwned });
}

bool Player::IsFriend() const
{
	return m_World->GetFriends().contains(GetSteamID());
}

duration_t Player::GetActiveTime() const
{
	if (m_Status.m_State != PlayerStatusState::Active)
		return 0s;

	return m_LastStatusUpdateTime - m_LastStatusActiveBegin;
}

std::optional<time_point_t> Player::GetEstimatedAccountCreationTime() const
{
	if (auto& summary = GetPlayerSummary())
	{
		if (summary->GetAccountAge())
			return summary->m_CreationTime;
	}

	return m_World->GetAccountAges().EstimateAccountCreationTime(GetSteamID());
}

void Player::SetStatus(PlayerStatus status, time_point_t timestamp)
{
	if (m_Status.m_State != PlayerStatusState::Active && status.m_State == PlayerStatusState::Active)
		m_LastStatusActiveBegin = timestamp;

	m_Status = std::move(status);
	m_LastStatusUpdateTime = m_LastPingUpdateTime = timestamp;
}
void Player::SetPing(uint16_t ping, time_point_t timestamp)
{
	m_Status.m_Ping = ping;
	m_LastPingUpdateTime = timestamp;
}

const std::any* Player::FindDataStorage(const std::type_index& type) const
{
	if (auto found = m_UserData.find(type); found != m_UserData.end())
		return &found->second;

	return nullptr;
}

std::any& Player::GetOrCreateDataStorage(const std::type_index& type)
{
	return m_UserData[type];
}

template<typename T>
static std::vector<SteamID> Take100(const T& collection)
{
	std::vector<SteamID> retVal;
	for (SteamID id : collection)
	{
		if (retVal.size() >= 100)
			break;

		retVal.push_back(id);
	}
	return retVal;
}

auto WorldState::PlayerSummaryUpdateAction::SendRequest(
	WorldState*& state, queue_collection_type& collection) -> response_future_type
{
	auto client = state->GetSettings().GetHTTPClient();
	if (!client)
		return {};

	if (!state->GetSettings().IsSteamAPIAvailable())
	{
		for (auto& entry : collection)
		{
			if (auto found = state->FindPlayer(entry))
				static_cast<Player*>(found)->m_PlayerSummary = SteamAPI::ErrorCode::SteamAPIDisabled;
		}
		return {};
	}

	std::vector<SteamID> steamIDs = Take100(collection);

	return SteamAPI::GetPlayerSummariesAsync(
		state->GetSettings(), std::move(steamIDs), *client);
}

void WorldState::PlayerSummaryUpdateAction::OnDataReady(WorldState*& state,
	const response_type& response, queue_collection_type& collection)
{
	DebugLog("[SteamAPI] Received {} player summaries", response.size());
	for (const SteamAPI::PlayerSummary& entry : response)
	{
		auto& player = state->FindOrCreatePlayer(entry.m_SteamID);
		player.m_PlayerSummary = entry;

		collection.erase(entry.m_SteamID);

		if (entry.m_CreationTime.has_value())
			state->m_AccountAges->OnDataReady(entry.m_SteamID, entry.m_CreationTime.value());
	}
}

auto WorldState::PlayerBansUpdateAction::SendRequest(state_type& state,
	queue_collection_type& collection) -> response_future_type
{
	auto client = state->GetSettings().GetHTTPClient();
	if (!client)
		return {};

	if (!state->GetSettings().IsSteamAPIAvailable())
	{
		for (auto& entry : collection)
		{
			if (auto found = state->FindPlayer(entry))
				static_cast<Player*>(found)->m_PlayerSteamBans = SteamAPI::ErrorCode::SteamAPIDisabled;
		}
		return {};
	}

	std::vector<SteamID> steamIDs = Take100(collection);
	return SteamAPI::GetPlayerBansAsync(
		state->GetSettings(), std::move(steamIDs), *client);
}

void WorldState::PlayerBansUpdateAction::OnDataReady(state_type& state,
	const response_type& response, queue_collection_type& collection)
{
	DebugLog("[SteamAPI] Received {} player bans", response.size());
	for (const SteamAPI::PlayerBans& bans : response)
	{
		state->FindOrCreatePlayer(bans.m_SteamID).m_PlayerSteamBans = bans;
		collection.erase(bans.m_SteamID);
	}
}
