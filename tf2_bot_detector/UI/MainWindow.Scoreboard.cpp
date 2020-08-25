#include "MainWindow.h"
#include "Config/Settings.h"
#include "Platform/Platform.h"
#include "UI/ImGui_TF2BotDetector.h"
#include "BaseTextures.h"
#include "IPlayer.h"
#include "Networking/SteamAPI.h"
#include "TextureManager.h"

#include <mh/math/interpolation.hpp>
#include <mh/text/fmtstr.hpp>

using namespace std::chrono_literals;
using namespace tf2_bot_detector;

void MainWindow::OnDrawScoreboard()
{
	static float frameWidth, contentWidth, windowContentWidth, windowWidth;
	bool forceRecalc = false;

	static float contentWidthMin = 500;

	// Horizontal scroller for color pickers
	OnDrawColorPickers("ScoreboardColorPickers",
		{
			{ "u homie", m_Settings.m_Theme.m_Colors.m_ScoreboardYouFG },
			{ "in da van", m_Settings.m_Theme.m_Colors.m_ScoreboardConnectingFG },
			{ "gang", m_Settings.m_Theme.m_Colors.m_ScoreboardFriendlyTeamBG },
			{ "da ops", m_Settings.m_Theme.m_Colors.m_ScoreboardEnemyTeamBG },
			{ "sik kunt", m_Settings.m_Theme.m_Colors.m_ScoreboardCheaterBG },
			{ "no one brah ur schizin", m_Settings.m_Theme.m_Colors.m_ScoreboardSuspiciousBG },
			{ "wizard", m_Settings.m_Theme.m_Colors.m_ScoreboardExploiterBG },
			{ "dog cunt", m_Settings.m_Theme.m_Colors.m_ScoreboardRacistBG },
		});

	ImGui::SetNextWindowContentSizeConstraints(ImVec2(contentWidthMin, -1), ImVec2(-1, -1));
	//ImGui::SetNextWindowContentSize(ImVec2(500, 0));
	if (ImGui::BeginChild("Scoreboard", { 0, ImGui::GetContentRegionAvail().y / 2 }, true, ImGuiWindowFlags_HorizontalScrollbar))
	{
		//ImGui::SetNextWindowSizeConstraints(ImVec2(500, -1), ImVec2(INFINITY, -1));
		//if (ImGui::BeginChild("ScoreboardScrollRegion", { 0, 0 }, false, ImGuiWindowFlags_AlwaysAutoResize))
		{
			static ImVec2 s_LastFrameSize;
			const bool scoreboardResized = [&]()
			{
				const auto thisFrameSize = ImGui::GetWindowSize();
				const bool changed = s_LastFrameSize != thisFrameSize;
				s_LastFrameSize = thisFrameSize;
				return changed || forceRecalc;
			}();

			/*const auto*/ frameWidth = ImGui::GetWorkRectSize().x;
			/*const auto*/ contentWidth = ImGui::GetContentRegionMax().x;
			/*const auto*/ windowContentWidth = ImGui::GetWindowContentRegionWidth();
			/*const auto*/ windowWidth = ImGui::GetWindowWidth();
			ImGui::Columns(7, "PlayersColumns");

			// Columns setup
			{
				float nameColumnWidth = frameWidth;
				// UserID header and column setup
				{
					ImGui::TextFmt("User ID");
					if (scoreboardResized)
					{
						const float width = ImGui::GetItemRectSize().x + ImGui::GetStyle().ItemSpacing.x * 2;
						nameColumnWidth -= width;
						ImGui::SetColumnWidth(-1, width);
					}

					ImGui::NextColumn();
				}

				// Name header and column setup
				ImGui::TextFmt("Name"); ImGui::NextColumn();

				// Kills header and column setup
				{
					ImGui::TextFmt("Kills");
					if (scoreboardResized)
					{
						const float width = ImGui::GetItemRectSize().x + ImGui::GetStyle().ItemSpacing.x * 2;
						nameColumnWidth -= width;
						ImGui::SetColumnWidth(-1, width);
					}

					ImGui::NextColumn();
				}

				// Deaths header and column setup
				{
					ImGui::TextFmt("Deaths");
					if (scoreboardResized)
					{
						const float width = ImGui::GetItemRectSize().x + ImGui::GetStyle().ItemSpacing.x * 2;
						nameColumnWidth -= width;
						ImGui::SetColumnWidth(-1, width);
					}

					ImGui::NextColumn();
				}

				// Connection time header and column setup
				{
					ImGui::TextFmt("Time");
					if (scoreboardResized)
					{
						const float width = 60;
						nameColumnWidth -= width;
						ImGui::SetColumnWidth(-1, width);
					}

					ImGui::NextColumn();
				}

				// Ping
				{
					ImGui::TextFmt("Ping");
					if (scoreboardResized)
					{
						const float width = ImGui::GetItemRectSize().x + ImGui::GetStyle().ItemSpacing.x * 2;
						nameColumnWidth -= width;
						ImGui::SetColumnWidth(-1, width);
					}

					ImGui::NextColumn();
				}

				// SteamID header and column setup
				{
					ImGui::TextFmt("Steam ID");
					if (scoreboardResized)
					{
						nameColumnWidth -= 100;// +ImGui::GetStyle().ItemSpacing.x * 2;
						ImGui::SetColumnWidth(1, std::max(10.0f, nameColumnWidth - ImGui::GetStyle().ItemSpacing.x * 2));
					}

					ImGui::NextColumn();
				}
				ImGui::Separator();
			}

			for (IPlayer& player : m_MainState->GeneratePlayerPrintData())
				OnDrawScoreboardRow(player);
		}
		//ImGui::EndChild();
	}

	ImGui::EndChild();
}

void MainWindow::OnDrawScoreboardRow(IPlayer& player)
{
	if (!m_Settings.m_LazyLoadAPIData)
		TryGetAvatarTexture(player);

	const auto& playerName = player.GetNameSafe();
	ImGuiDesktop::ScopeGuards::ID idScope((int)player.GetSteamID().Lower32);
	ImGuiDesktop::ScopeGuards::ID idScope2((int)player.GetSteamID().Upper32);

	ImGuiDesktop::ScopeGuards::StyleColor textColor;
	if (player.GetConnectionState() != PlayerStatusState::Active || player.GetNameSafe().empty())
		textColor = { ImGuiCol_Text, m_Settings.m_Theme.m_Colors.m_ScoreboardConnectingFG };
	else if (player.GetSteamID() == m_Settings.GetLocalSteamID())
		textColor = { ImGuiCol_Text, m_Settings.m_Theme.m_Colors.m_ScoreboardYouFG };

	mh::fmtstr<32> buf;
	if (!player.GetUserID().has_value())
		buf = "?";
	else
		buf.fmt("{}", player.GetUserID().value());

	bool shouldDrawPlayerTooltip = false;

	// Selectable
	const auto teamShareResult = GetModLogic().GetTeamShareResult(player);
	const auto playerAttribs = GetModLogic().GetPlayerAttributes(player);
	{
		ImVec4 bgColor = [&]() -> ImVec4
		{
			switch (teamShareResult)
			{
			case TeamShareResult::SameTeams:      return m_Settings.m_Theme.m_Colors.m_ScoreboardFriendlyTeamBG;
			case TeamShareResult::OppositeTeams:  return m_Settings.m_Theme.m_Colors.m_ScoreboardEnemyTeamBG;
			}

			switch (player.GetTeam())
			{
			case TFTeam::Red:   return ImVec4(1.0f, 0.5f, 0.5f, 0.5f);
			case TFTeam::Blue:  return ImVec4(0.5f, 0.5f, 1.0f, 0.5f);
			default: return ImVec4(0.5f, 0.5f, 0.5f, 0);
			}
		}();

		if (playerAttribs.Has(PlayerAttribute::Cheater))
			bgColor = mh::lerp(TimeSine(), bgColor, ImVec4(m_Settings.m_Theme.m_Colors.m_ScoreboardCheaterBG));
		else if (playerAttribs.Has(PlayerAttribute::Suspicious))
			bgColor = mh::lerp(TimeSine(), bgColor, ImVec4(m_Settings.m_Theme.m_Colors.m_ScoreboardSuspiciousBG));
		else if (playerAttribs.Has(PlayerAttribute::Exploiter))
			bgColor = mh::lerp(TimeSine(), bgColor, ImVec4(m_Settings.m_Theme.m_Colors.m_ScoreboardExploiterBG));
		else if (playerAttribs.Has(PlayerAttribute::Racist))
			bgColor = mh::lerp(TimeSine(), bgColor, ImVec4(m_Settings.m_Theme.m_Colors.m_ScoreboardRacistBG));

		ImGuiDesktop::ScopeGuards::StyleColor styleColorScope(ImGuiCol_Header, bgColor);

		bgColor.w = std::min(bgColor.w + 0.25f, 0.8f);
		ImGuiDesktop::ScopeGuards::StyleColor styleColorScopeHovered(ImGuiCol_HeaderHovered, bgColor);

		bgColor.w = std::min(bgColor.w + 0.5f, 1.0f);
		ImGuiDesktop::ScopeGuards::StyleColor styleColorScopeActive(ImGuiCol_HeaderActive, bgColor);
		ImGui::Selectable(buf.c_str(), true, ImGuiSelectableFlags_SpanAllColumns);

		shouldDrawPlayerTooltip = ImGui::IsItemHovered();

		ImGui::NextColumn();
	}

	OnDrawScoreboardContextMenu(player);

	// player names column
	{
		static constexpr bool DEBUG_ALWAYS_DRAW_ICONS = false;

		const auto columnEndX = ImGui::GetCursorPosX() - ImGui::GetStyle().ItemSpacing.x + ImGui::GetColumnWidth();

		if (!playerName.empty())
			ImGui::TextFmt(playerName);
		else if (const SteamAPI::PlayerSummary* summary = player.GetPlayerSummary(); summary && !summary->m_Nickname.empty())
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

					auto icon = m_BaseTextures->GetVACShield_16();
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

					auto icon = m_BaseTextures->GetGameBanIcon_16();
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

				auto icon = m_BaseTextures->GetHeart_16();
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

			const auto spacing = ImGui::GetStyle().ItemSpacing.x;
			ImGui::SetCursorPosX(columnEndX - (16 + spacing) * icons.size());

			for (size_t i = 0; i < icons.size(); i++)
			{
				ImGui::Image(icons[i].m_Texture, { 16, 16 }, { 0, 0 }, { 1, 1 }, icons[i].m_Color);

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
		OnDrawPlayerTooltip(player, teamShareResult, playerAttribs);
}

void MainWindow::OnDrawScoreboardContextMenu(IPlayer& player)
{
	if (auto popupScope = ImGui::BeginPopupContextItemScope("PlayerContextMenu"))
	{
		ImGuiDesktop::ScopeGuards::StyleColor textColor(ImGuiCol_Text, { 1, 1, 1, 1 });

		const auto steamID = player.GetSteamID();
		if (ImGui::MenuItem("Copy SteamID", nullptr, false, steamID.IsValid()))
			ImGui::SetClipboardText(steamID.str().c_str());

		if (ImGui::BeginMenu("Go To"))
		{
			if (!m_Settings.m_GotoProfileSites.empty())
			{
				for (const auto& item : m_Settings.m_GotoProfileSites)
				{
					ImGuiDesktop::ScopeGuards::ID id(&item);
					if (ImGui::MenuItem(item.m_Name.c_str()))
						Shell::OpenURL(item.CreateProfileURL(player));
				}
			}
			else
			{
				ImGui::MenuItem("No sites configured", nullptr, nullptr, false);
			}

			ImGui::EndMenu();
		}

		const auto& world = GetWorld();
		auto& modLogic = GetModLogic();

		if (ImGui::BeginMenu("Votekick",
			(world.GetTeamShareResult(steamID, m_Settings.GetLocalSteamID()) == TeamShareResult::SameTeams) && world.FindUserID(steamID)))
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
				const bool existingMarked = modLogic.HasPlayerAttributes(player, PlayerAttribute(i));

				if (ImGui::MenuItem(mh::fmtstr<512>("{}", PlayerAttribute(i)).c_str(), nullptr, existingMarked))
				{
					if (modLogic.SetPlayerAttribute(player, PlayerAttribute(i), !existingMarked))
						Log("Manually marked {}{} {}", player, (existingMarked ? " NOT" : ""), PlayerAttribute(i));
				}
			}

			ImGui::EndMenu();
		}

#ifdef _DEBUG
		ImGui::Separator();

		if (bool isRunning = GetModLogic().IsUserRunningTool(player);
			ImGui::MenuItem("Is Running TFBD", nullptr, isRunning))
		{
			GetModLogic().SetUserRunningTool(player, !isRunning);
		}
#endif
	}
}

void MainWindow::OnDrawPlayerTooltip(IPlayer& player, TeamShareResult teamShareResult,
	const PlayerMarks& playerAttribs)
{
	ImGui::BeginTooltip();
	OnDrawPlayerTooltipBody(player, teamShareResult, playerAttribs);
	ImGui::EndTooltip();
}

void MainWindow::OnDrawPlayerTooltipBody(IPlayer& player, TeamShareResult teamShareResult,
	const PlayerMarks& playerAttribs)
{
	ImGuiDesktop::ScopeGuards::StyleColor textColor(ImGuiCol_Text, { 1, 1, 1, 1 });

	/////////////////////
	// Draw the avatar //
	/////////////////////
	{
		if (auto tex = TryGetAvatarTexture(player))
			ImGui::Image((ImTextureID)(intptr_t)tex->GetHandle(), { 184, 184 });
		else
			ImGui::Dummy({ 184, 184 });
	}

	////////////////////////////////
	// Fix up the cursor position //
	////////////////////////////////
	{
		const auto pos = ImGui::GetCursorPos();
		ImGui::SetItemAllowOverlap();
		ImGui::SameLine();
		ImGui::NewLine();
		ImGui::SetCursorPos(ImGui::GetCursorStartPos());
		ImGui::Indent(pos.y - ImGui::GetStyle().FramePadding.x);
	}

	///////////////////
	// Draw the text //
	///////////////////
	const ImVec4 COLOR_PRIVATE = { 1, 1, 0, 1 };
	ImGui::TextFmt("  In-game Name : \"{}\"", player.GetNameUnsafe());
	if (const SteamAPI::PlayerSummary* summary = player.GetPlayerSummary())
	{
		using namespace SteamAPI;
		ImGui::TextFmt("    Steam Name : \"{}\"", summary->m_Nickname);

		if (!summary->m_RealName.empty())
			ImGui::TextFmt("     Real Name : \"{}\"", summary->m_RealName);

		if (auto vanity = summary->GetVanityURL(); !vanity.empty())
			ImGui::TextFmt("    Vanity URL : \"{}\"", vanity);

		ImGui::TextFmt("   Account Age : ");
		ImGui::SameLineNoPad();
		if (auto age = summary->GetAccountAge())
			ImGui::TextFmt("{}", HumanDuration(*age));
		else
			ImGui::TextFmt(COLOR_PRIVATE, "Private");

		ImGui::TextFmt("        Status : ");
		ImGui::SameLineNoPad();
		switch (summary->m_Status)
		{
		case PersonaState::Offline:
			ImGui::TextFmt({ 0.4f, 0.4f, 0.4f, 1 }, "Offline");
			break;
		case PersonaState::Online:
			ImGui::TextFmt({ 0, 1, 0, 1 }, "Online");
			break;
		case PersonaState::Busy:
			ImGui::TextFmt({ 1, 135 / 255.0f, 135 / 255.0f, 1 }, "Busy");
			break;
		case PersonaState::Away:
			ImGui::TextFmt({ 92 / 255.0f, 154 / 255.0f, 245 / 255.0f, 0.5f }, "Away");
			break;
		case PersonaState::Snooze:
			ImGui::TextFmt({ 92 / 255.0f, 154 / 255.0f, 245 / 255.0f, 0.35f }, "Snooze");
			break;
		case PersonaState::LookingToTrade:
			ImGui::TextFmt({ 0, 1, 1, 1 }, "Looking to Trade");
			break;
		case PersonaState::LookingToPlay:
			ImGui::TextFmt({ 0, 1, 0.5f, 1 }, "Looking to Play");
			break;
		default:
			ImGui::TextFmt({ 1, 0, 0, 1 }, "Unknown ({})", int(summary->m_Status));
			break;
		}

		ImGui::TextFmt(" Profile State : ");
		ImGui::SameLineNoPad();
		switch (summary->m_Visibility)
		{
		case CommunityVisibilityState::Public:
			ImGui::TextFmt({ 0, 1, 0, 1 }, "Public");
			break;
		case CommunityVisibilityState::FriendsOnly:
			ImGui::TextFmt(COLOR_PRIVATE, "Friends Only");
			break;
		case CommunityVisibilityState::Private:
			ImGui::TextFmt(COLOR_PRIVATE, "Private");
			break;
		default:
			ImGui::TextFmt({ 1, 0, 0, 1 }, "Unknown ({})", int(summary->m_Visibility));
			break;
		}

		if (!summary->m_ProfileConfigured)
		{
			ImGui::SameLineNoPad();
			ImGui::TextFmt(", ");
			ImGui::SameLineNoPad();
			ImGui::TextFmt({ 1, 0, 0, 1 }, "Not Configured");
		}

#if 0 // decreed as useless information by overlord czechball
		ImGui::TextFmt("Comment Permissions: ");
		ImGui::SameLineNoPad();
		if (summary->m_CommentPermissions)
			ImGui::TextColoredUnformatted({ 0, 1, 0, 1 }, "You can comment");
		else
			ImGui::TextColoredUnformatted(COLOR_PRIVATE, "You cannot comment");
#endif
	}
	else
	{
		ImGui::TextFmt("Loading player summary...");
	}

	if (const SteamAPI::PlayerBans* bans = player.GetPlayerBans())
	{
		using namespace SteamAPI;
		if (bans->m_CommunityBanned)
		{
			ImGui::TextFmt("SteamCommunity : ");
			ImGui::SameLineNoPad();
			ImGui::TextFmt({ 1, 0, 0, 1 }, "Banned");
		}

		{
			const ImVec4 banColor = (bans->m_TimeSinceLastBan >= (24h * 365 * 7)) ?
				ImVec4(1, 1, 0, 1) : ImVec4(1, 0, 0, 1);
			if (bans->m_VACBanCount > 0)
				ImGui::TextFmt(banColor, "      VAC Bans : {}", bans->m_VACBanCount);
			if (bans->m_GameBanCount > 0)
				ImGui::TextFmt(banColor, "     Game Bans : {}", bans->m_GameBanCount);
			if (bans->m_VACBanCount || bans->m_GameBanCount)
				ImGui::TextFmt(banColor, "Time Since Last Ban: {}", HumanDuration(bans->m_TimeSinceLastBan));
		}

		if (bans->m_EconomyBan != PlayerEconomyBan::None)
		{
			ImGui::TextFmt("  Trade Status :");
			switch (bans->m_EconomyBan)
			{
			case PlayerEconomyBan::Probation:
				ImGui::TextFmt({ 1, 1, 0, 1 }, "Banned (Probation)");
				break;
			case PlayerEconomyBan::Banned:
				ImGui::TextFmt({ 1, 0, 0, 1 }, "Banned");
				break;

			default:
			case PlayerEconomyBan::Unknown:
				ImGui::TextFmt({ 1, 0, 0, 1 }, "Unknown");
				break;
			}
		}


#if 0 // Don't show useless normal information like "hey this person isn't banned
		if (bans->m_VACBanCount < 1 &&
			bans->m_GameBanCount < 1 &&
			!bans->m_CommunityBanned &&
			bans->m_EconomyBan == PlayerEconomyBan::None)
		{
			ImGui::TextColoredUnformatted({ 0, 1, 0, 1 }, "No Steam Bans");
		}
#endif
	}
	else
	{
		ImGui::TextFmt("Loading player bans...");
	}

	if (const auto playtime = player.GetTF2Playtime())
	{
		ImGui::TextFmt("  TF2 Playtime : ");
		ImGui::SameLineNoPad();
		if (playtime->IsError())
		{
			// The reason for the GameNotOwned check is that the API hides free games if you haven't played them.
			// So even if you can see other owned games, if you make your playtime private... suddenly you don't
			// own TF2 anymore, and it disappears from the owned games list.
			if (playtime->GetError().value() == int(SteamAPI::ErrorCode::InfoPrivate) ||
				playtime->GetError().value() == int(SteamAPI::ErrorCode::GameNotOwned))
				ImGui::TextFmt(COLOR_PRIVATE, "Private");
			else
				ImGui::TextFmt({ 1, 0, 0, 1 }, playtime->GetError().message());
		}
		else
		{
			const auto hours = std::chrono::duration_cast<std::chrono::hours>(*playtime->GetValue());
			ImGui::TextFmt("{} hours", hours.count());
		}
	}
	else
	{
		ImGui::TextFmt("Loading player TF2 game time...");
	}

	ImGui::NewLine();

#ifdef _DEBUG
	ImGui::TextFmt("   Active time : {}", HumanDuration(player.GetActiveTime()));
#endif

	if (teamShareResult != TeamShareResult::SameTeams)
	{
		auto kills = player.GetScores().m_LocalKills;
		auto deaths = player.GetScores().m_LocalDeaths;
		//ImGui::Text("Your Thirst: %1.0f%%", kills == 0 ? float(deaths) * 100 : float(deaths) / kills * 100);
		ImGui::TextFmt("  Their Thirst : {}%", int(deaths == 0 ? float(kills) * 100 : float(kills) / deaths * 100));
	}

	if (playerAttribs)
	{
		ImGui::NewLine();
		ImGui::TextFmt("Player {} marked in playerlist(s):{}", player, playerAttribs);
	}
}
