#include "Networking/SteamAPI.h"
#include "UI/Components.h"
#include "UI/ImGui_TF2BotDetector.h"
#include "Application.h"
#include "IPlayer.h"
#include "WorldState.h"

#include <mh/math/interpolation.hpp>
#include <mh/text/fmtstr.hpp>

namespace tf2_bot_detector::UI
{
	void DrawTeamStats(const IWorldState& world)
	{
		Settings& settings = GetSettings();

		if (!settings.m_UIState.m_MainWindow.m_TeamStatsEnabled)
			return;

		struct TeamStats
		{
			uint8_t m_PlayerCount = 0;
			uint32_t m_Kills = 0;
			uint32_t m_Deaths = 0;
			duration_t m_AccountAgeSum{};
		};

		TeamStats statsArray[2]{};

		for (const IPlayer& player : world.GetPlayers())
		{
			TeamStats* stats = nullptr;
			switch (world.GetTeamShareResult(player))
			{
			case TeamShareResult::SameTeams:
				stats = &statsArray[0];
				break;
			case TeamShareResult::OppositeTeams:
				stats = &statsArray[1];
				break;
			default:
				LogError("Unknown TeamShareResult");
				[[fallthrough]];
			case TeamShareResult::Neither:
				break;
			}

			if (!stats)
				continue;

			stats->m_PlayerCount++;

			bool foundAccountAge = false;
			if (auto summary = player.GetPlayerSummary())
			{
				if (auto age = summary->GetAccountAge())
				{
					stats->m_AccountAgeSum += *age;
					foundAccountAge = true;
				}
			}

			if (!foundAccountAge)
			{
				if (auto age = player.GetEstimatedAccountAge())
				{
					stats->m_AccountAgeSum += *age;
					foundAccountAge = true;
				}
			}

			stats->m_Kills += player.GetScores().m_Kills;
			stats->m_Deaths += player.GetScores().m_Deaths;
		}

		const auto& themeCols = settings.m_Theme.m_Colors;
		auto friendlyBG = mh::lerp(themeCols.m_ScoreboardFriendlyTeamBG[3],
			ImGui::GetStyle().Colors[ImGuiCol_WindowBg], (ImVec4)themeCols.m_ScoreboardFriendlyTeamBG);
		friendlyBG.w = 1;
		const auto enemyBG = themeCols.m_ScoreboardEnemyTeamBG;

		ImGuiDesktop::ScopeGuards::StyleColor progressBarFG(ImGuiCol_PlotHistogram, friendlyBG);
		ImGuiDesktop::ScopeGuards::StyleColor progressBarBG(ImGuiCol_FrameBg, enemyBG);

		if (const auto totalPlayers = statsArray[0].m_PlayerCount + statsArray[1].m_PlayerCount; totalPlayers > 0)
		{
			const float playersFraction = statsArray[0].m_PlayerCount / float(totalPlayers);

			ImGui::ProgressBar(playersFraction, { -FLT_MIN, 0 },
				mh::fmtstr<128>("Team Players: {} | {}", statsArray[0].m_PlayerCount, statsArray[1].m_PlayerCount).c_str());
		}

		if (const auto totalKills = statsArray[0].m_Kills + statsArray[1].m_Kills; totalKills > 0)
		{
			const float killsFraction = statsArray[0].m_Kills / float(totalKills);

			ImGui::ProgressBar(killsFraction, { -FLT_MIN, 0 },
				mh::fmtstr<128>("Team Kills: {} | {}", statsArray[0].m_Kills, statsArray[1].m_Kills).c_str());
		}

#if 0
		if (const auto totalTime = statsArray[0].m_AccountAgeSum + statsArray[1].m_AccountAgeSum; totalTime.count() > 0)
		{
			const float timeFraction = float(statsArray[0].m_AccountAgeSum.count() / double(totalTime.count()));

			ImGui::ProgressBar(timeFraction, { -FLT_MIN, 0 },
				mh::fmtstr<128>("Team Account Age: {} | {}",
					HumanDuration(statsArray[0].m_AccountAgeSum), HumanDuration(statsArray[1].m_AccountAgeSum)).c_str());
		}
#endif
	}
}
