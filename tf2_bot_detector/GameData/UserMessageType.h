#pragma once

#include <ostream>

namespace tf2_bot_detector
{
	enum class UserMessageType
	{
		// From "meta game" on 7/18/2020
		Geiger = 0,
		Train = 1,
		HudText = 2,
		SayText = 3,
		SayText2 = 4,
		TextMsg = 5,
		ResetHUD = 6,
		GameTitle = 7,
		ItemPickup = 8,
		ShowMenu = 9,
		Shake = 10,
		Fade = 11,
		VGUIMenu = 12,
		Rumble = 13,
		CloseCaption = 14,
		SendAudio = 15,
		VoiceMask = 16,
		RequestState = 17,
		Damage = 18,
		HintText = 19,
		KeyHintText = 20,
		HudMsg = 21,
		AmmoDenied = 22,
		AchievementEvent = 23,
		UpdateRadar = 24,
		VoiceSubtitle = 25,
		HudNotify = 26,
		HudNotifyCustom = 27,
		PlayerStatsUpdate = 28,
		MapStatsUpdate = 29,
		PlayerIgnited = 30,
		PlayerIgnitedInv = 31,
		HudArenaNotify = 32,
		UpdateAchievement = 33,
		TrainingMsg = 34,
		TrainingObjective = 35,
		DamageDodged = 36,
		PlayerJarated = 37,
		PlayerExtinguished = 38,
		PlayerJaratedFade = 39,
		PlayerShieldBlocked = 40,
		BreakModel = 41,
		CheapBreakModel = 42,
		BreakModel_Pumpkin = 43,
		BreakModelRocketDud = 44,
		CallVoteFailed = 45,
		VoteStart = 46,
		VotePass = 47,
		VoteFailed = 48,
		VoteSetup = 49,
		PlayerBonusPoints = 50,
		RDTeamPointsChanged = 51,
		SpawnFlyingBird = 52,
		PlayerGodRayEffect = 53,
		PlayerTeleportHomeEffect = 54,
		MVMStatsReset = 55,
		MVMPlayerEvent = 56,
		MVMResetPlayerStats = 57,
		MVMWaveFailed = 58,
		MVMAnnouncement = 59,
		MVMPlayerUpgradedEvent = 60,
		MVMVictory = 61,
		MVMWaveChange = 62,
		MVMLocalPlayerUpgradesClear = 63,
		MVMLocalPlayerUpgradesValue = 64,
		MVMLocalPlayerWaveSpendingStats = 65,
		MVMLocalPlayerWaveSpendingValue = 66,
		MVMLocalPlayerUpgradeSpending = 67,
		MVMServerKickTimeUpdate = 68,
		PlayerLoadoutUpdated = 69,
		PlayerTauntSoundLoopStart = 70,
		PlayerTauntSoundLoopEnd = 71,
		ForcePlayerViewAngles = 72,
		BonusDucks = 73,
		EOTLDuckEvent = 74,
		PlayerPickupWeapon = 75,
		QuestObjectiveCompleted = 76,
		SPHapWeapEvent = 77,
		HapDmg = 78,
		HapPunch = 79,
		HapSetDrag = 80,
		HapSetConst = 81,
		HapMeleeContact = 82,
	};
}

template<typename CharT, typename Traits>
std::basic_ostream<CharT, Traits>& operator<<(std::basic_ostream<CharT, Traits>& os, tf2_bot_detector::UserMessageType type)
{
	using tf2_bot_detector::UserMessageType;
#undef OS_CASE
#define OS_CASE(v) case v : return os << #v
	switch (type)
	{
		OS_CASE(UserMessageType::Geiger);
		OS_CASE(UserMessageType::Train);
		OS_CASE(UserMessageType::HudText);
		OS_CASE(UserMessageType::SayText);
		OS_CASE(UserMessageType::SayText2);
		OS_CASE(UserMessageType::TextMsg);
		OS_CASE(UserMessageType::ResetHUD);
		OS_CASE(UserMessageType::GameTitle);
		OS_CASE(UserMessageType::ItemPickup);
		OS_CASE(UserMessageType::ShowMenu);
		OS_CASE(UserMessageType::Shake);
		OS_CASE(UserMessageType::Fade);
		OS_CASE(UserMessageType::VGUIMenu);
		OS_CASE(UserMessageType::Rumble);
		OS_CASE(UserMessageType::CloseCaption);
		OS_CASE(UserMessageType::SendAudio);
		OS_CASE(UserMessageType::VoiceMask);
		OS_CASE(UserMessageType::RequestState);
		OS_CASE(UserMessageType::Damage);
		OS_CASE(UserMessageType::HintText);
		OS_CASE(UserMessageType::KeyHintText);
		OS_CASE(UserMessageType::HudMsg);
		OS_CASE(UserMessageType::AmmoDenied);
		OS_CASE(UserMessageType::AchievementEvent);
		OS_CASE(UserMessageType::UpdateRadar);
		OS_CASE(UserMessageType::VoiceSubtitle);
		OS_CASE(UserMessageType::HudNotify);
		OS_CASE(UserMessageType::HudNotifyCustom);
		OS_CASE(UserMessageType::PlayerStatsUpdate);
		OS_CASE(UserMessageType::MapStatsUpdate);
		OS_CASE(UserMessageType::PlayerIgnited);
		OS_CASE(UserMessageType::PlayerIgnitedInv);
		OS_CASE(UserMessageType::HudArenaNotify);
		OS_CASE(UserMessageType::UpdateAchievement);
		OS_CASE(UserMessageType::TrainingMsg);
		OS_CASE(UserMessageType::TrainingObjective);
		OS_CASE(UserMessageType::DamageDodged);
		OS_CASE(UserMessageType::PlayerJarated);
		OS_CASE(UserMessageType::PlayerExtinguished);
		OS_CASE(UserMessageType::PlayerJaratedFade);
		OS_CASE(UserMessageType::PlayerShieldBlocked);
		OS_CASE(UserMessageType::BreakModel);
		OS_CASE(UserMessageType::CheapBreakModel);
		OS_CASE(UserMessageType::BreakModel_Pumpkin);
		OS_CASE(UserMessageType::BreakModelRocketDud);
		OS_CASE(UserMessageType::CallVoteFailed);
		OS_CASE(UserMessageType::VoteStart);
		OS_CASE(UserMessageType::VotePass);
		OS_CASE(UserMessageType::VoteFailed);
		OS_CASE(UserMessageType::VoteSetup);
		OS_CASE(UserMessageType::PlayerBonusPoints);
		OS_CASE(UserMessageType::RDTeamPointsChanged);
		OS_CASE(UserMessageType::SpawnFlyingBird);
		OS_CASE(UserMessageType::PlayerGodRayEffect);
		OS_CASE(UserMessageType::PlayerTeleportHomeEffect);
		OS_CASE(UserMessageType::MVMStatsReset);
		OS_CASE(UserMessageType::MVMPlayerEvent);
		OS_CASE(UserMessageType::MVMResetPlayerStats);
		OS_CASE(UserMessageType::MVMWaveFailed);
		OS_CASE(UserMessageType::MVMAnnouncement);
		OS_CASE(UserMessageType::MVMPlayerUpgradedEvent);
		OS_CASE(UserMessageType::MVMVictory);
		OS_CASE(UserMessageType::MVMWaveChange);
		OS_CASE(UserMessageType::MVMLocalPlayerUpgradesClear);
		OS_CASE(UserMessageType::MVMLocalPlayerUpgradesValue);
		OS_CASE(UserMessageType::MVMLocalPlayerWaveSpendingStats);
		OS_CASE(UserMessageType::MVMLocalPlayerWaveSpendingValue);
		OS_CASE(UserMessageType::MVMLocalPlayerUpgradeSpending);
		OS_CASE(UserMessageType::MVMServerKickTimeUpdate);
		OS_CASE(UserMessageType::PlayerLoadoutUpdated);
		OS_CASE(UserMessageType::PlayerTauntSoundLoopStart);
		OS_CASE(UserMessageType::PlayerTauntSoundLoopEnd);
		OS_CASE(UserMessageType::ForcePlayerViewAngles);
		OS_CASE(UserMessageType::BonusDucks);
		OS_CASE(UserMessageType::EOTLDuckEvent);
		OS_CASE(UserMessageType::PlayerPickupWeapon);
		OS_CASE(UserMessageType::QuestObjectiveCompleted);
		OS_CASE(UserMessageType::SPHapWeapEvent);
		OS_CASE(UserMessageType::HapDmg);
		OS_CASE(UserMessageType::HapPunch);
		OS_CASE(UserMessageType::HapSetDrag);
		OS_CASE(UserMessageType::HapSetConst);
		OS_CASE(UserMessageType::HapMeleeContact);

	default:
		return os << "UserMessageType(" << +std::underlying_type_t<UserMessageType>(type) << ')';
	}
#undef OS_CASE
}
