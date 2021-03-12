#include "Actions/Actions.h"
#include "Config/PlayerListJSON.h"
#include "Config/Settings.h"
#include "Networking/SteamAPI.h"
#include "Platform/Platform.h"
#include "UI/Components.h"
#include "UI/ImGui_TF2BotDetector.h"
#include "Application.h"
#include "BaseTextures.h"
#include "Bitmap.h"
#include "GenericErrors.h"
#include "GlobalDispatcher.h"
#include "IPlayer.h"
#include "PlayerStatus.h"
#include "TextureManager.h"
#include "WorldState.h"

#include <imgui_desktop/StorageHelper.h>

using namespace std::chrono_literals;

namespace tf2_bot_detector::UI
{
	mh::expected<std::shared_ptr<ITexture>> TryGetAvatarTexture(IPlayer& player)
	{
		using StateTask_t = mh::task<mh::expected<std::shared_ptr<ITexture>, std::error_condition>>;

		struct PlayerAvatarData
		{
			StateTask_t m_State;

			static StateTask_t LoadAvatarAsync(mh::task<Bitmap> avatarBitmapTask)
			{
				const Bitmap* avatarBitmap = nullptr;

				auto textureManager = TF2BDApplication::Get().GetTextureManager();

				try
				{
					avatarBitmap = &(co_await avatarBitmapTask);
				}
				catch (...)
				{
					LogException(MH_SOURCE_LOCATION_CURRENT(), "Failed to load avatar bitmap");
					co_return ErrorCode::UnknownError;
				}

				// Switch to main thread
				co_await GetDispatcher().co_dispatch();

				try
				{
					co_return textureManager->CreateTexture(*avatarBitmap);
				}
				catch (...)
				{
					LogException(MH_SOURCE_LOCATION_CURRENT(), "Failed to create avatar bitmap");
					co_return ErrorCode::UnknownError;
				}
			}
		};

		auto& avatarData = player.GetOrCreateData<PlayerAvatarData>().m_State;

		if (avatarData.empty())
		{
			auto playerPtr = player.shared_from_this();
			const auto& summary = playerPtr->GetPlayerSummary();
			if (summary)
			{
				avatarData = PlayerAvatarData::LoadAvatarAsync(summary->GetAvatarBitmap(GetSettings().GetHTTPClient()));
			}
			else
			{
				return summary.error();
			}
		}

		if (auto data = avatarData.try_get())
			return *data;
		else
			return std::errc::operation_in_progress;
	}

	void DrawScoreboardContextMenu(IModeratorLogic& modLogic, IPlayer& player)
	{
		const Settings& settings = GetSettings();

		if (auto popupScope = ImGui::BeginPopupContextItemScope("PlayerContextMenu"))
		{
			ImGuiDesktop::ScopeGuards::StyleColor textColor(ImGuiCol_Text, { 1, 1, 1, 1 });

			// Just so we can be 100% sure of who we clicked on
			ImGui::MenuItem(player.GetNameSafe().c_str(), nullptr, nullptr, false);
			ImGui::MenuItem(player.GetSteamID().str().c_str(), nullptr, nullptr, false);
			ImGui::Separator();

			const auto steamID = player.GetSteamID();
			if (ImGui::BeginMenu("Copy"))
			{
				if (ImGui::MenuItem("In-game Name"))
					ImGui::SetClipboardText(player.GetNameUnsafe().c_str());

				if (ImGui::MenuItem("Steam ID", nullptr, false, steamID.IsValid()))
					ImGui::SetClipboardText(steamID.str().c_str());

				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Go To"))
			{
				if (!settings.m_GotoProfileSites.empty())
				{
					for (const auto& item : settings.m_GotoProfileSites)
					{
						ImGuiDesktop::ScopeGuards::ID id(&item);
						if (ImGui::MenuItem(item.m_Name.c_str()))
							Shell::OpenURL(item.CreateProfileURL(player));
					}

					if (settings.m_GotoProfileSites.size() > 1)
					{
						ImGui::Separator();
						if (ImGui::MenuItem("Open All"))
						{
							for (const auto& item : settings.m_GotoProfileSites)
								Shell::OpenURL(item.CreateProfileURL(player));
						}
					}
				}
				else
				{
					ImGui::MenuItem("No sites configured", nullptr, nullptr, false);
				}

				ImGui::EndMenu();
			}

			const auto& world = player.GetWorld();

			if (ImGui::BeginMenu("Votekick",
				(world.GetTeamShareResult(steamID, settings.GetLocalSteamID()) == TeamShareResult::SameTeams) && world.FindUserID(steamID)))
			{
				if (ImGui::MenuItem("Cheating"))
					modLogic.InitiateVotekick(player, KickReason::Cheating);
				if (ImGui::MenuItem("Idle"))
					modLogic.InitiateVotekick(player, KickReason::Idle);
				if (ImGui::MenuItem("Other"))
					modLogic.InitiateVotekick(player, KickReason::Other);
				if (ImGui::MenuItem("Scamming"))
					modLogic.InitiateVotekick(player, KickReason::Scamming);

				ImGui::EndMenu();
			}

			ImGui::Separator();

			if (ImGui::BeginMenu("Mark"))
			{
				for (int i = 0; i < (int)PlayerAttribute::COUNT; i++)
				{
					const auto attr = PlayerAttribute(i);
					const bool existingMarked = (bool)modLogic.HasPlayerAttributes(player, attr, AttributePersistence::Saved);

					if (ImGui::MenuItem(mh::fmtstr<512>("{:v}", mh::enum_fmt(attr)).c_str(), nullptr, existingMarked))
					{
						if (modLogic.SetPlayerAttribute(player, attr, AttributePersistence::Saved, !existingMarked))
							Log("Manually marked {}{} {:v}", player, (existingMarked ? " NOT" : ""), mh::enum_fmt(attr));
					}
				}

				ImGui::EndMenu();
			}

#ifdef _DEBUG
			ImGui::Separator();

			if (bool isRunning = modLogic.IsUserRunningTool(player);
				ImGui::MenuItem("Is Running TFBD", nullptr, isRunning))
			{
				modLogic.SetUserRunningTool(player, !isRunning);
			}
#endif
		}
	}

	void DrawScoreboardRow(IModeratorLogic& modLogic, IPlayer& player)
	{
		const IBaseTextures& baseTextures = TF2BDApplication::Get().GetBaseTextures();
		Settings& settings = GetSettings();

		if (!settings.m_LazyLoadAPIData)
			TryGetAvatarTexture(player);

		const auto& playerName = player.GetNameSafe();
		ImGuiDesktop::ScopeGuards::ID idScope((int)player.GetSteamID().Lower32);
		ImGuiDesktop::ScopeGuards::ID idScope2((int)player.GetSteamID().Upper32);

		ImGuiDesktop::ScopeGuards::StyleColor textColor;
		if (player.GetConnectionState() != PlayerStatusState::Active || player.GetNameSafe().empty())
			textColor = { ImGuiCol_Text, settings.m_Theme.m_Colors.m_ScoreboardConnectingFG };
		else if (player.GetSteamID() == settings.GetLocalSteamID())
			textColor = { ImGuiCol_Text, settings.m_Theme.m_Colors.m_ScoreboardYouFG };

		mh::fmtstr<32> buf;
		if (!player.GetUserID().has_value())
			buf = "?";
		else
			buf.fmt("{}", player.GetUserID().value());

		bool shouldDrawPlayerTooltip = false;

		// Selectable
		const auto teamShareResult = modLogic.GetTeamShareResult(player);
		const auto playerAttribs = modLogic.GetPlayerAttributes(player);
		{
			ImVec4 bgColor = [&]() -> ImVec4
			{
				switch (teamShareResult)
				{
				case TeamShareResult::SameTeams:      return settings.m_Theme.m_Colors.m_ScoreboardFriendlyTeamBG;
				case TeamShareResult::OppositeTeams:  return settings.m_Theme.m_Colors.m_ScoreboardEnemyTeamBG;
				case TeamShareResult::Neither:        break;
				}

				switch (player.GetTeam())
				{
				case TFTeam::Red:   return ImVec4(1.0f, 0.5f, 0.5f, 0.5f);
				case TFTeam::Blue:  return ImVec4(0.5f, 0.5f, 1.0f, 0.5f);
				default: return ImVec4(0.5f, 0.5f, 0.5f, 0);
				}
			}();

			if (playerAttribs.Has(PlayerAttribute::Cheater))
				bgColor = BlendColors(bgColor.to_array(), settings.m_Theme.m_Colors.m_ScoreboardCheaterBG, TimeSine());
			else if (playerAttribs.Has(PlayerAttribute::Suspicious))
				bgColor = BlendColors(bgColor.to_array(), settings.m_Theme.m_Colors.m_ScoreboardSuspiciousBG, TimeSine());
			else if (playerAttribs.Has(PlayerAttribute::Exploiter))
				bgColor = BlendColors(bgColor.to_array(), settings.m_Theme.m_Colors.m_ScoreboardExploiterBG, TimeSine());
			else if (playerAttribs.Has(PlayerAttribute::Racist))
				bgColor = BlendColors(bgColor.to_array(), settings.m_Theme.m_Colors.m_ScoreboardRacistBG, TimeSine());

			ImGuiDesktop::ScopeGuards::StyleColor styleColorScope(ImGuiCol_Header, bgColor);

			bgColor.w = std::min(bgColor.w + 0.25f, 0.8f);
			ImGuiDesktop::ScopeGuards::StyleColor styleColorScopeHovered(ImGuiCol_HeaderHovered, bgColor);

			bgColor.w = std::min(bgColor.w + 0.5f, 1.0f);
			ImGuiDesktop::ScopeGuards::StyleColor styleColorScopeActive(ImGuiCol_HeaderActive, bgColor);
			ImGui::Selectable(buf.c_str(), true, ImGuiSelectableFlags_SpanAllColumns);

			shouldDrawPlayerTooltip = ImGui::IsItemHovered();

			ImGui::NextColumn();
		}

		DrawScoreboardContextMenu(modLogic, player);

		// player names column
		{
			static constexpr bool DEBUG_ALWAYS_DRAW_ICONS = false;

			const auto columnEndX = ImGui::GetCursorPosX() - ImGui::GetStyle().ItemSpacing.x + ImGui::GetColumnWidth();

			if (!playerName.empty())
				ImGui::TextFmt(playerName);
			else if (const auto& summary = player.GetPlayerSummary(); summary && !summary->m_Nickname.empty())
				ImGui::TextFmt(summary->m_Nickname);
			else
				ImGui::TextFmt("<Unknown>");

			// If their steamcommunity name doesn't match their ingame name
			if (auto summary = player.GetPlayerSummary();
				summary && !playerName.empty() && summary->m_Nickname != playerName)
			{
				ImGui::SameLine();
				ImGui::TextFmt({ 1, 0, 0, 1 }, "({})", summary->m_Nickname);
			}

			// Move cursor pos up a few pixels if we have icons to draw
			struct IconDrawData
			{
				ImTextureID m_Texture;
				ImVec4 m_Color{ 1, 1, 1, 1 };
				std::string_view m_Tooltip;
			};
			std::vector<IconDrawData> icons;

			// Check their steam bans
			if (auto bans = player.GetPlayerBans())
			{
				// If they are VAC banned
				std::invoke([&]
					{
						if constexpr (!DEBUG_ALWAYS_DRAW_ICONS)
						{
							if (bans->m_VACBanCount <= 0)
								return;
						}

						auto icon = baseTextures.GetVACShield_16();
						if (!icon)
							return;

						icons.push_back({ (ImTextureID)(intptr_t)icon->GetHandle(), { 1, 1, 1, 1 }, "VAC Banned" });
					});

				// If they are game banned
				std::invoke([&]
					{
						if constexpr (!DEBUG_ALWAYS_DRAW_ICONS)
						{
							if (bans->m_GameBanCount <= 0)
								return;
						}

						auto icon = baseTextures.GetGameBanIcon_16();
						if (!icon)
							return;

						icons.push_back({ (ImTextureID)(intptr_t)icon->GetHandle(), { 1, 1, 1, 1 }, "Game Banned" });
					});
			}

			// If they are friends with us on Steam
			std::invoke([&]
				{
					if constexpr (!DEBUG_ALWAYS_DRAW_ICONS)
					{
						if (!player.IsFriend())
							return;
					}

					auto icon = baseTextures.GetHeart_16();
					if (!icon)
						return;

					icons.push_back({ (ImTextureID)(intptr_t)icon->GetHandle(), { 1, 0, 0, 1 }, "Steam Friends" });
				});

			if (!icons.empty())
			{
				// We have at least one icon to draw
				ImGui::SameLine();

				// Move it up very slightly so it looks centered in these tiny rows
				ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2);

				const float iconSize = 16 * ImGui::GetCurrentFontScale();

				const auto spacing = ImGui::GetStyle().ItemSpacing.x;
				ImGui::SetCursorPosX(columnEndX - (iconSize + spacing) * icons.size());

				for (size_t i = 0; i < icons.size(); i++)
				{
					ImGui::Image(icons[i].m_Texture, { iconSize, iconSize }, { 0, 0 }, { 1, 1 }, icons[i].m_Color);

					ImGuiDesktop::ScopeGuards::TextColor color({ 1, 1, 1, 1 });
					if (ImGui::SetHoverTooltip(icons[i].m_Tooltip))
						shouldDrawPlayerTooltip = false;

					ImGui::SameLine(0, spacing);
				}
			}

			ImGui::NextColumn();
		}

		// Kills column
		{
			if (playerName.empty())
				ImGui::TextRightAligned("?");
			else
				ImGui::TextRightAlignedF("%u", player.GetScores().m_Kills);

			ImGui::NextColumn();
		}

		// Deaths column
		{
			if (playerName.empty())
				ImGui::TextRightAligned("?");
			else
				ImGui::TextRightAlignedF("%u", player.GetScores().m_Deaths);

			ImGui::NextColumn();
		}

		// Connected time column
		{
			if (playerName.empty())
			{
				ImGui::TextRightAligned("?");
			}
			else
			{
				ImGui::TextRightAlignedF("%u:%02u",
					std::chrono::duration_cast<std::chrono::minutes>(player.GetConnectedTime()).count(),
					std::chrono::duration_cast<std::chrono::seconds>(player.GetConnectedTime()).count() % 60);
			}

			ImGui::NextColumn();
		}

		// Ping column
		{
			if (playerName.empty())
				ImGui::TextRightAligned("?");
			else
				ImGui::TextRightAlignedF("%u", player.GetPing());

			ImGui::NextColumn();
		}

		// Steam ID column
		{
			const auto str = player.GetSteamID().str();
			if (player.GetSteamID().Type != SteamAccountType::Invalid)
				ImGui::TextFmt(ImGui::GetStyle().Colors[ImGuiCol_Text], str);
			else
				ImGui::TextFmt(str);

			ImGui::NextColumn();
		}

		if (shouldDrawPlayerTooltip)
		{
			DrawPlayerTooltip(player, teamShareResult, playerAttribs);
		}
	}

	static mh::generator<IPlayer&> GeneratePlayerPrintData(std::shared_ptr<IWorldState> world)
	{
		IPlayer* printData[33]{};
		auto begin = std::begin(printData);
		auto end = std::end(printData);
		assert(begin <= end);
		assert(static_cast<size_t>(end - begin) >= world->GetApproxLobbyMemberCount());

		std::fill(begin, end, nullptr);

		{
			auto* current = begin;
			for (IPlayer& member : world->GetLobbyMembers())
			{
				*current = &member;
				current++;
			}

			if (current == begin)
			{
				// We seem to have either an empty lobby or we're playing on a community server.
				// Just find the most recent status updates.
				for (IPlayer& playerData : world->GetPlayers())
				{
					if (playerData.GetLastStatusUpdateTime() >= (world->GetLastStatusUpdateTime() - 15s))
					{
						*current = &playerData;
						current++;

						if (current >= end)
							break; // This might happen, but we're not in a lobby so everything has to be approximate
					}
				}
			}

			end = current;
		}

		std::sort(begin, end, [](const IPlayer* lhs, const IPlayer* rhs) -> bool
			{
				assert(lhs);
				assert(rhs);
				//if (!lhs && !rhs)
				//	return false;
				//if (auto result = !!rhs <=> !!lhs; !std::is_eq(result))
				//	return result < 0;

				// Intentionally reversed, we want descending kill order
				if (auto killsResult = rhs->GetScores().m_Kills <=> lhs->GetScores().m_Kills; !std::is_eq(killsResult))
					return std::is_lt(killsResult);

				if (auto deathsResult = lhs->GetScores().m_Deaths <=> rhs->GetScores().m_Deaths; !std::is_eq(deathsResult))
					return std::is_lt(deathsResult);

				// Sort by ascending userid
				{
					auto luid = lhs->GetUserID();
					auto ruid = rhs->GetUserID();
					if (luid && ruid)
					{
						if (auto result = *luid <=> *ruid; !std::is_eq(result))
							return std::is_lt(result);
					}
				}

				return false;
			});

		for (auto it = begin; it != end; ++it)
			co_yield **it;
	}

	void DrawScoreboard(IWorldState& world, IModeratorLogic& modLogic)
	{
		Settings& settings = GetSettings();

		const auto& style = ImGui::GetStyle();
		const auto currentFontScale = ImGui::GetCurrentFontScale();

		constexpr bool forceRecalc = false;

		static constexpr float contentWidthMin = 500;
		float contentWidthMinOuter = contentWidthMin + style.WindowPadding.x * 2;

		// Horizontal scroller for color pickers
		settings.SaveFileIf(
			DrawColorPickers("ScoreboardColorPickers",
				{
					{ "You", settings.m_Theme.m_Colors.m_ScoreboardYouFG },
					{ "Connecting", settings.m_Theme.m_Colors.m_ScoreboardConnectingFG },
					{ "Friendly", settings.m_Theme.m_Colors.m_ScoreboardFriendlyTeamBG },
					{ "Enemy", settings.m_Theme.m_Colors.m_ScoreboardEnemyTeamBG },
					{ "Cheater", settings.m_Theme.m_Colors.m_ScoreboardCheaterBG },
					{ "Suspicious", settings.m_Theme.m_Colors.m_ScoreboardSuspiciousBG },
					{ "Exploiter", settings.m_Theme.m_Colors.m_ScoreboardExploiterBG },
					{ "Racist", settings.m_Theme.m_Colors.m_ScoreboardRacistBG },
				}));

		// TODO: Should not be part of DrawScoreboard
		DrawTeamStats(world);

		const auto availableSpaceOuter = ImGui::GetContentRegionAvail();

		//ImGui::SetNextWindowContentSizeConstraints(ImVec2(contentWidthMin, -1), ImVec2(-1, -1));
		float extraScoreboardHeight = 0;
		if (availableSpaceOuter.x < contentWidthMinOuter)
		{
			ImGui::SetNextWindowContentSize(ImVec2{ contentWidthMin, -1 });
			extraScoreboardHeight += style.ScrollbarSize;
		}

		static ImGuiDesktop::Storage<float> s_ScoreboardHeightStorage;
		const auto lastScoreboardHeight = s_ScoreboardHeightStorage.Snapshot();
		const float minScoreboardHeight = ImGui::GetContentRegionAvail().y / (settings.m_UIState.m_MainWindow.m_AppLogEnabled ? 2 : 1);
		const auto actualScoreboardHeight = std::max(minScoreboardHeight, lastScoreboardHeight.Get()) + extraScoreboardHeight;
		if (ImGui::BeginChild("Scoreboard", { 0, actualScoreboardHeight }, true, ImGuiWindowFlags_HorizontalScrollbar))
		{
			{
				static ImVec2 s_LastFrameSize;
				const bool scoreboardResized = [&]()
				{
					const auto thisFrameSize = ImGui::GetWindowSize();
					const bool changed = s_LastFrameSize != thisFrameSize;
					s_LastFrameSize = thisFrameSize;
					return changed || forceRecalc;
				}();

				const auto windowContentWidth = ImGui::GetWindowContentRegionWidth();
				const auto windowWidth = ImGui::GetWindowWidth();
				ImGui::BeginGroup();
				ImGui::Columns(7, "PlayersColumns");

				// Columns setup
				{
					float nameColumnWidth = windowContentWidth;

					const auto AddColumnHeader = [&](const char* name, float widthOverride = -1)
					{
						ImGui::TextFmt(name);
						if (scoreboardResized)
						{
							float width;

							if (widthOverride > 0)
								width = widthOverride * currentFontScale;
							else
								width = (ImGui::GetItemRectSize().x + style.ItemSpacing.x * 2);

							nameColumnWidth -= width;
							ImGui::SetColumnWidth(-1, width);
						}

						ImGui::NextColumn();
					};

					AddColumnHeader("User ID");

					// Name header and column setup
					ImGui::TextFmt("Name"); ImGui::NextColumn();

					AddColumnHeader("Kills");
					AddColumnHeader("Deaths");
					AddColumnHeader("Time", 60);
					AddColumnHeader("Ping");

					// SteamID header and column setup
					{
						ImGui::TextFmt("Steam ID");
						if (scoreboardResized)
						{
							nameColumnWidth -= 100 * currentFontScale;// +ImGui::GetStyle().ItemSpacing.x * 2;
							ImGui::SetColumnWidth(1, std::max(10.0f, nameColumnWidth - style.ItemSpacing.x * 2));
						}

						ImGui::NextColumn();
					}
					ImGui::Separator();
				}

				for (IPlayer& player : GeneratePlayerPrintData(world.shared_from_this()))
					DrawScoreboardRow(modLogic, player);

				ImGui::EndGroup();

				// Save the height of the scoreboard contents so we can resize to fit it next frame
				{
					float height = ImGui::GetItemRectSize().y;
					height += style.WindowPadding.y * 2;
					lastScoreboardHeight = height;
				}
			}
		}

		ImGui::EndChild();
	}
}
