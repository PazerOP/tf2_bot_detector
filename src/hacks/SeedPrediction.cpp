/*
 * SeedPrediction.cpp
 *
 *  Created on: Jul 27, 2018
 *      Author: bencat07
 */
#include <settings/Bool.hpp>
#include "common.hpp"
#include "SeedPrediction.hpp"
#include "reclasses.hpp"

namespace hacks::tf2::seedprediction
{
constexpr float MIN_CLOCKRES = 0.25;
constexpr float MAX_CLOCKRES = 8192.5;
float clockRes;
float seedFraction = 0.0f;

static settings::Boolean prediction{ "seed-prediction.enable", "false" };
bool predon()
{
    return *prediction;
}
buf bases{ 9999 };
buf2 rebased{ 9999 };
buf3 intervals{ 9999 };
seedstruct selected{ 9999 };
// Server sends us seeds when other players are shooting.
// Needs to be called in appropriate hook (not PostDataUpdate) since
// PostDataUpdate for TempEntities gives inaccurate tickcount
void handleFireBullets(C_TEFireBullets *ent)
{
    if (g_IEngine->IsInGame())
    {
        INetChannel *ch = (INetChannel *) g_IEngine->GetNetChannelInfo();
        float time      = g_GlobalVars->curtime * g_GlobalVars->interval_per_tick - (ch ? ch->GetLatency(MAX_FLOWS) / 2 : 0.0f);
        bases.push_back(seedstruct{ g_GlobalVars->tickcount, ent->m_iSeed(), time }); // It's circular buffer
        selectBase();
    }
}

void selectBase()
{
    if (bases.size() <= 1)
    {
        return;
    }

    int total    = bases.size();
    selected     = bases[bases.size() - 1];
    seedFraction = 0.0;

    // Algorithmic approach to estimate server time offset

    // 1. Find clock resolution
    // For each reasonable precision value "rebase" seeds to the same tick
    // and check if they are close to each other (by looking for largest gap
    // between).

    int bestGap       = 0;
    float newClockRes = 1.0;

    for (float res = MIN_CLOCKRES; res < MAX_CLOCKRES + 1.0; res *= 2.0)
    {
        rebased.clear();
        for (seedstruct &base : bases)
        {
            rebased.push_back(predictSeed2{ base, selected.tickcount, res });
        }

        std::sort(rebased.begin(), rebased.end());
        int gap = 0;

        for (int i = 0; i < rebased.size(); i++)
        {
            int left  = rebased[i].tickcount;
            int right = rebased[i + 1 < rebased.size() ? i + 1 : 0].tickcount;
            gap       = max(gap, (right - left) % 256);
        }

        gap = (gap > 0 ? gap : 256);
        if (bestGap < gap)
        {
            bestGap     = gap;
            newClockRes = res;
        }
    }

    if (total >= 5)
    {
        clockRes = newClockRes;
    }

    // 2. Find seed fraction offset
    // Estimate time more precisely: "rebase" seeds to same tick (keep fraction
    // part),
    // interpret them as intervals of size 1 and find offset which covers most
    // of them.

    float maxDisp = 5.0 / clockRes;
    intervals.clear();

    for (seedstruct &base : bases)
    {
        float disp = float(base.seed) - float(selected.seed);
        disp       = fmod(disp - predictOffset(selected, base.tickcount, clockRes), 256);
        disp       = (disp > 128.0f ? disp - 256.0f : disp);

        if (abs(disp) < fmaxf(1.2, maxDisp))
        {
            intervals.push_back({ 1, disp - 0.5f }); // Actually "interval ends", not "intervals"
            intervals.push_back({ -1, disp + 0.5f });
        }
    }

    int curChance = 0, bestChance = 0;
    sort(intervals.begin(), intervals.end());

    for (int i = 0; i + 1 < intervals.size(); i++)
    {
        IntervalEdge &inter = intervals[i];
        curChance += inter.val;

        if (curChance > bestChance)
        {
            bestChance   = curChance;
            seedFraction = (inter.pos + intervals[i + 1].pos) / 2.0f;
        }
    }

    logging::Info("seedpred-stats", "Seed prediction: res = %.3f, chance = %d%%\n", clockRes, bestChance * 100 / total);
}

float predictOffset(const seedstruct &entry, int targetTick, float clockRes)
{
    return (1000.0f * g_GlobalVars->interval_per_tick / clockRes) * float(targetTick - entry.tickcount);
}

int predictSeed(const seedstruct &entry, int targetTick, float clockRes, float SeedOffset)
{
    return (entry.seed + int(roundeven(predictOffset(entry, targetTick, clockRes)) + SeedOffset)) % 256;
}

int predictTick(float targetTime)
{
    INetChannel *ch = (INetChannel *) g_IEngine->GetNetChannelInfo();
    float ping      = ch ? ch->GetLatency(MAX_FLOWS) / 2 : 0.0f;
    float deltaTime = targetTime - selected.time + ping;
    return int(float(selected.tickcount) + deltaTime / g_GlobalVars->interval_per_tick + 0.7);
}

int predictTick()
{
    return predictTick(g_GlobalVars->curtime * g_GlobalVars->interval_per_tick);
}

int predictSeed(float targetTime)
{
    INetChannel *ch  = (INetChannel *) g_IEngine->GetNetChannelInfo();
    float ping       = ch ? ch->GetLatency(MAX_FLOWS) / 2 : 0.0f;
    float deltaTime  = targetTime - selected.time + ping;
    int tick         = int(float(selected.tickcount) + deltaTime / g_GlobalVars->interval_per_tick + 0.7);
    float SeedOffset = predictOffset(selected, tick, clockRes);
    int seed         = predictSeed(selected, tick, clockRes, SeedOffset);

    logging::Info("seedpred-pred", "Last shot: guessed server tick = %d, guessed seed = %03d\n", tick, seed);
    return seed;
}

int predictSeed()
{
    return predictSeed(g_GlobalVars->curtime * g_GlobalVars->interval_per_tick);
}

void reset()
{
    logging::Info("seedpred-stats", "Seed prediction: reset\n");
    if (!bases.empty())
    {
        bases.clear();
        clockRes = 2.0;
    }
}
} // namespace hacks::tf2::seedprediction
