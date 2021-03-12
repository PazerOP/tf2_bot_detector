#include "Config/PlayerListJSON.h"
#include "Networking/LogsTFAPI.h"
#include "Networking/SteamAPI.h"
#include "UI/Components.h"
#include "UI/ImGui_TF2BotDetector.h"
#include "TextureManager.h"
#include "IPlayer.h"
#include "Log.h"
#include "WorldState.h"

#include <mh/text/formatters/error_code.hpp>

#include <set>

using namespace tf2_bot_detector;
using namespace std::chrono_literals;

static const ImVec4 COLOR_RED = { 1, 0, 0, 1 };
static const ImVec4 COLOR_YELLOW = { 1, 1, 0, 1 };
static const ImVec4 COLOR_GREEN = { 0, 1, 0, 1 };
static const ImVec4 COLOR_UNAVAILABLE = { 1, 1, 1, 0.5 };
static const ImVec4 COLOR_PRIVATE = COLOR_YELLOW;

namespace ImGui
{
	struct Span
	{
		using fmtstr_type = mh::fmtstr<256>;
		using string_type = std::string;

		template<typename... TArgs>
		Span(const ImVec4& color, const std::string_view& fmtStr, const TArgs&... args) :
			m_Color(color), m_Value(fmtstr_type(fmtStr, args...))
		{
		}

		template<typename... TArgs>
		Span(const std::string_view& fmtStr, const TArgs&... args) :
			m_Value(fmtstr_type(fmtStr, args...))
		{
		}

		Span(const std::string_view& text)
		{
			if (text.size() <= fmtstr_type::max_size())
				m_Value.emplace<fmtstr_type>(text);
			else
				m_Value.emplace<string_type>(text);
		}
		Span(const char* text) : Span(std::string_view(text)) {}

		std::string_view GetView() const
		{
			if (auto str = std::get_if<fmtstr_type>(&m_Value))
				return *str;
			else if (auto str = std::get_if<string_type>(&m_Value))
				return *str;

			LogError(MH_SOURCE_LOCATION_CURRENT(), "Unexpected variant index {}", m_Value.index());
			return __FUNCTION__ ": UNEXPECTED VARIANT INDEX";
		}

		std::optional<ImVec4> m_Color;
		std::variant<fmtstr_type, string_type> m_Value;
	};

	template<typename TIter>
	void TextSpan(TIter begin, TIter end)
	{
		if (begin == end)
			return;

		if (begin->m_Color.has_value())
			ImGui::TextFmt(*begin->m_Color, begin->GetView());
		else
			ImGui::TextFmt(begin->GetView());

		++begin;

		for (auto it = begin; it != end; ++it)
		{
			ImGui::SameLineNoPad();

			if (it->m_Color.has_value())
				ImGui::TextFmt(*it->m_Color, it->GetView());
			else
				ImGui::TextFmt(it->GetView());
		}
	}

	void TextSpan(const std::initializer_list<Span>& spans) { return TextSpan(spans.begin(), spans.end()); }
}

namespace tf2_bot_detector::UI
{
	static void EnterAPIKeyText()
	{
		ImGui::TextFmt(COLOR_UNAVAILABLE, "Enter Steam API key in Settings");
	}

	static void PrintPersonaState(SteamAPI::PersonaState state)
	{
		using SteamAPI::PersonaState;
		switch (state)
		{
		case PersonaState::Offline:
			return ImGui::TextFmt({ 0.4f, 0.4f, 0.4f, 1 }, "Offline");
		case PersonaState::Online:
			return ImGui::TextFmt(COLOR_GREEN, "Online");
		case PersonaState::Busy:
			return ImGui::TextFmt({ 1, 135 / 255.0f, 135 / 255.0f, 1 }, "Busy");
		case PersonaState::Away:
			return ImGui::TextFmt({ 92 / 255.0f, 154 / 255.0f, 245 / 255.0f, 0.5f }, "Away");
		case PersonaState::Snooze:
			return ImGui::TextFmt({ 92 / 255.0f, 154 / 255.0f, 245 / 255.0f, 0.35f }, "Snooze");
		case PersonaState::LookingToTrade:
			return ImGui::TextFmt({ 0, 1, 1, 1 }, "Looking to Trade");
		case PersonaState::LookingToPlay:
			return ImGui::TextFmt({ 0, 1, 0.5f, 1 }, "Looking to Play");
		}

		ImGui::TextFmt(COLOR_RED, "Unknown ({})", int(state));
	}

	static void PrintPlayerSummary(const IPlayer& player)
	{
		player.GetPlayerSummary()
			.or_else([&](std::error_condition err)
				{
					ImGui::TextFmt("Player Summary : ");
					ImGui::SameLineNoPad();

					if (err == std::errc::operation_in_progress)
						ImGui::PacifierText();
					else if (err == SteamAPI::ErrorCode::EmptyAPIKey)
						EnterAPIKeyText();
					else
						ImGui::TextFmt(COLOR_RED, "{}", err);
				})
			.map([&](const SteamAPI::PlayerSummary& summary)
				{
					using namespace SteamAPI;
					ImGui::TextFmt("    Steam Name : \"{}\"", summary.m_Nickname);

					ImGui::TextFmt("     Real Name : ");
					ImGui::SameLineNoPad();
					if (summary.m_RealName.empty())
						ImGui::TextFmt(COLOR_UNAVAILABLE, "Not set");
					else
						ImGui::TextFmt("\"{}\"", summary.m_RealName);

					ImGui::TextFmt("    Vanity URL : ");
					ImGui::SameLineNoPad();
					if (auto vanity = summary.GetVanityURL(); !vanity.empty())
						ImGui::TextFmt("\"{}\"", vanity);
					else
						ImGui::TextFmt(COLOR_UNAVAILABLE, "Not set");

					ImGui::TextFmt("   Account Age : ");
					ImGui::SameLineNoPad();
					if (auto age = summary.GetAccountAge())
					{
						ImGui::TextFmt("{}", HumanDuration(*age));
					}
					else
					{
						ImGui::TextFmt(COLOR_PRIVATE, "Private");

						if (auto estimated = player.GetEstimatedAccountAge())
						{
							ImGui::SameLine();
							ImGui::TextFmt("(estimated {})", HumanDuration(*estimated));
						}
#ifdef _DEBUG
						else
						{
							ImGui::SameLine();
							ImGui::TextFmt("(estimated ???)");
						}
#endif
					}

					ImGui::TextFmt("        Status : ");
					ImGui::SameLineNoPad();
					PrintPersonaState(summary.m_Status);

					ImGui::TextFmt(" Profile State : ");
					ImGui::SameLineNoPad();
					switch (summary.m_Visibility)
					{
					case CommunityVisibilityState::Public:
						ImGui::TextFmt(COLOR_GREEN, "Public");
						break;
					case CommunityVisibilityState::FriendsOnly:
						ImGui::TextFmt(COLOR_PRIVATE, "Friends Only");
						break;
					case CommunityVisibilityState::Private:
						ImGui::TextFmt(COLOR_PRIVATE, "Private");
						break;
					default:
						ImGui::TextFmt(COLOR_RED, "Unknown ({})", int(summary.m_Visibility));
						break;
					}

					if (!summary.m_ProfileConfigured)
					{
						ImGui::SameLineNoPad();
						ImGui::TextFmt(", ");
						ImGui::SameLineNoPad();
						ImGui::TextFmt(COLOR_RED, "Not Configured");
					}

#if 0 // decreed as useless information by overlord czechball
					ImGui::TextFmt("Comment Permissions: ");
					ImGui::SameLineNoPad();
					if (summary->m_CommentPermissions)
						ImGui::TextColoredUnformatted({ 0, 1, 0, 1 }, "You can comment");
					else
						ImGui::TextColoredUnformatted(COLOR_PRIVATE, "You cannot comment");
#endif
				});
	}

	static void PrintPlayerBans(const IPlayer& player)
	{
		player.GetPlayerBans()
			.or_else([&](std::error_condition err)
				{
					ImGui::TextFmt("   Player Bans : ");
					ImGui::SameLineNoPad();

					if (err == std::errc::operation_in_progress)
						ImGui::PacifierText();
					else if (err == SteamAPI::ErrorCode::EmptyAPIKey)
						EnterAPIKeyText();
					else
						ImGui::TextFmt(COLOR_RED, "{}", err);
				})
			.map([&](const SteamAPI::PlayerBans& bans)
				{
					using namespace SteamAPI;
					if (bans.m_CommunityBanned)
						ImGui::TextSpan({ "SteamCommunity : ", { COLOR_RED, "Banned" } });

					if (bans.m_EconomyBan != PlayerEconomyBan::None)
					{
						ImGui::TextFmt("  Trade Status :");
						switch (bans.m_EconomyBan)
						{
						case PlayerEconomyBan::Probation:
							ImGui::TextFmt(COLOR_YELLOW, "Banned (Probation)");
							break;
						case PlayerEconomyBan::Banned:
							ImGui::TextFmt(COLOR_RED, "Banned");
							break;

						default:
						case PlayerEconomyBan::Unknown:
							ImGui::TextFmt(COLOR_RED, "Unknown");
							break;
						}
					}

					{
						const ImVec4 banColor = (bans.m_TimeSinceLastBan >= (24h * 365 * 7)) ? COLOR_YELLOW : COLOR_RED;
						if (bans.m_VACBanCount > 0)
							ImGui::TextFmt(banColor, "      VAC Bans : {}", bans.m_VACBanCount);
						if (bans.m_GameBanCount > 0)
							ImGui::TextFmt(banColor, "     Game Bans : {}", bans.m_GameBanCount);
						if (bans.m_VACBanCount || bans.m_GameBanCount)
							ImGui::TextFmt(banColor, "      Last Ban : {} ago", HumanDuration(bans.m_TimeSinceLastBan));
					}
				});
	}

	static void PrintPlayerPlaytime(const IPlayer& player)
	{
		ImGui::TextFmt("  TF2 Playtime : ");
		ImGui::SameLineNoPad();
		player.GetTF2Playtime()
			.or_else([&](std::error_condition err)
				{
					if (err == std::errc::operation_in_progress)
					{
						ImGui::PacifierText();
					}
					else if (err == SteamAPI::ErrorCode::InfoPrivate || err == SteamAPI::ErrorCode::GameNotOwned)
					{
						// The reason for the GameNotOwned check is that the API hides free games if you haven't played
						// them. So even if you can see other owned games, if you make your playtime private...
						// suddenly you don't own TF2 anymore, and it disappears from the owned games list.
						ImGui::TextFmt(COLOR_PRIVATE, "Private");
					}
					else if (err == SteamAPI::ErrorCode::EmptyAPIKey)
					{
						EnterAPIKeyText();
					}
					else
					{
						ImGui::TextFmt(COLOR_RED, "{}", err);
					}
				})
			.map([&](duration_t playtime)
				{
					const auto hours = std::chrono::duration_cast<std::chrono::hours>(playtime);
					ImGui::TextFmt("{} hours", hours.count());
				});
	}

	static void PrintPlayerLogsCount(const IPlayer& player)
	{
		ImGui::TextFmt("       Logs.TF : ");
		ImGui::SameLineNoPad();

		player.GetLogsInfo()
			.or_else([&](std::error_condition err)
				{
					if (err == std::errc::operation_in_progress)
					{
						ImGui::PacifierText();
					}
					else
					{
						ImGui::TextFmt(COLOR_RED, "{}", err);
					}
				})
			.map([&](const LogsTFAPI::PlayerLogsInfo& info)
				{
					ImGui::TextFmt("{} logs", info.m_LogsCount);
				});
	}

	static void PrintPlayerInventoryInfo(const IPlayer& player)
	{
		ImGui::TextFmt("Inventory Size : ");
		ImGui::SameLineNoPad();

		player.GetInventoryInfo()
			.or_else([&](std::error_condition err)
				{
					if (err == std::errc::operation_in_progress)
					{
						ImGui::PacifierText();
					}
					else if (err == SteamAPI::ErrorCode::InfoPrivate)
					{
						ImGui::TextFmt(COLOR_PRIVATE, "Private");
					}
					else
					{
						ImGui::TextFmt(COLOR_RED, "{}", err);
					}
				})
			.map([&](const SteamAPI::PlayerInventoryInfo& info)
				{
					ImGui::TextFmt("{} items ({} slots)", info.m_Items, info.m_Slots);
				});
	}

	static void PrintFriendsInServer(const IPlayer& player)
	{
		ImGui::TextFmt("       Friends : ");
		ImGui::SameLineNoPad();

		bool noneInServer = true;
		for (const IPlayer& otherPlayer : player.GetWorld().GetPlayers())
		{
			if (!player.GetSteamFriends().contains(otherPlayer.GetSteamID()))
				continue; // other player not on our friends list

			if (!noneInServer)
			{
				ImGui::TextFmt("               ");
				ImGui::SameLineNoPad();
			}
			else
			{
				noneInServer = false;
			}

			ImGui::TextFmt("- {}", otherPlayer);
		}

		if (noneInServer)
			ImGui::TextFmt(COLOR_UNAVAILABLE, "None");
	}

	void DrawPlayerTooltipBody(IPlayer& player, TeamShareResult teamShareResult,
		const PlayerMarks& playerAttribs)
	{
		ImGuiDesktop::ScopeGuards::StyleColor textColor(ImGuiCol_Text, { 1, 1, 1, 1 });

		/////////////////////
		// Draw the avatar //
		/////////////////////
		TryGetAvatarTexture(player)
			.or_else([&](std::error_condition ec)
				{
					if (ec != SteamAPI::ErrorCode::EmptyAPIKey)
						ImGui::Dummy({ 184, 184 });
				})
			.map([&](const std::shared_ptr<ITexture>& tex)
				{
					ImGui::Image((ImTextureID)(intptr_t)tex->GetHandle(), { 184, 184 });
				});

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
				ImGui::TextFmt("  In-game Name : ");
				ImGui::SameLineNoPad();
				if (auto name = player.GetNameUnsafe(); !name.empty())
					ImGui::TextFmt("\"{}\"", name);
				else
					ImGui::TextFmt(COLOR_UNAVAILABLE, "Unknown");

				PrintPlayerSummary(player);
				PrintPlayerBans(player);
				PrintPlayerPlaytime(player);
				PrintPlayerLogsCount(player);
				PrintPlayerInventoryInfo(player);

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

	void DrawPlayerTooltip(const IModeratorLogic& modLogic, IPlayer& player)
	{
		DrawPlayerTooltip(player, player.GetWorld().GetTeamShareResult(player), modLogic.GetPlayerAttributes(player));
	}

	void DrawPlayerTooltip(IPlayer& player, TeamShareResult teamShareResult, const PlayerMarks& playerAttribs)
	{
		ImGui::BeginTooltip();
		DrawPlayerTooltipBody(player, teamShareResult, playerAttribs);
		ImGui::EndTooltip();
	}
}
