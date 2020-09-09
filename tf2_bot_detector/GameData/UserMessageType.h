#pragma once

#include <mh/reflection/enum.hpp>

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

MH_ENUM_REFLECT_BEGIN(tf2_bot_detector::UserMessageType)
	MH_ENUM_REFLECT_VALUE(Geiger)
	MH_ENUM_REFLECT_VALUE(Train)
	MH_ENUM_REFLECT_VALUE(HudText)
	MH_ENUM_REFLECT_VALUE(SayText)
	MH_ENUM_REFLECT_VALUE(SayText2)
	MH_ENUM_REFLECT_VALUE(TextMsg)
	MH_ENUM_REFLECT_VALUE(ResetHUD)
	MH_ENUM_REFLECT_VALUE(GameTitle)
	MH_ENUM_REFLECT_VALUE(ItemPickup)
	MH_ENUM_REFLECT_VALUE(ShowMenu)
	MH_ENUM_REFLECT_VALUE(Shake)
	MH_ENUM_REFLECT_VALUE(Fade)
	MH_ENUM_REFLECT_VALUE(VGUIMenu)
	MH_ENUM_REFLECT_VALUE(Rumble)
	MH_ENUM_REFLECT_VALUE(CloseCaption)
	MH_ENUM_REFLECT_VALUE(SendAudio)
	MH_ENUM_REFLECT_VALUE(VoiceMask)
	MH_ENUM_REFLECT_VALUE(RequestState)
	MH_ENUM_REFLECT_VALUE(Damage)
	MH_ENUM_REFLECT_VALUE(HintText)
	MH_ENUM_REFLECT_VALUE(KeyHintText)
	MH_ENUM_REFLECT_VALUE(HudMsg)
	MH_ENUM_REFLECT_VALUE(AmmoDenied)
	MH_ENUM_REFLECT_VALUE(AchievementEvent)
	MH_ENUM_REFLECT_VALUE(UpdateRadar)
	MH_ENUM_REFLECT_VALUE(VoiceSubtitle)
	MH_ENUM_REFLECT_VALUE(HudNotify)
	MH_ENUM_REFLECT_VALUE(HudNotifyCustom)
	MH_ENUM_REFLECT_VALUE(PlayerStatsUpdate)
	MH_ENUM_REFLECT_VALUE(MapStatsUpdate)
	MH_ENUM_REFLECT_VALUE(PlayerIgnited)
	MH_ENUM_REFLECT_VALUE(PlayerIgnitedInv)
	MH_ENUM_REFLECT_VALUE(HudArenaNotify)
	MH_ENUM_REFLECT_VALUE(UpdateAchievement)
	MH_ENUM_REFLECT_VALUE(TrainingMsg)
	MH_ENUM_REFLECT_VALUE(TrainingObjective)
	MH_ENUM_REFLECT_VALUE(DamageDodged)
	MH_ENUM_REFLECT_VALUE(PlayerJarated)
	MH_ENUM_REFLECT_VALUE(PlayerExtinguished)
	MH_ENUM_REFLECT_VALUE(PlayerJaratedFade)
	MH_ENUM_REFLECT_VALUE(PlayerShieldBlocked)
	MH_ENUM_REFLECT_VALUE(BreakModel)
	MH_ENUM_REFLECT_VALUE(CheapBreakModel)
	MH_ENUM_REFLECT_VALUE(BreakModel_Pumpkin)
	MH_ENUM_REFLECT_VALUE(BreakModelRocketDud)
	MH_ENUM_REFLECT_VALUE(CallVoteFailed)
	MH_ENUM_REFLECT_VALUE(VoteStart)
	MH_ENUM_REFLECT_VALUE(VotePass)
	MH_ENUM_REFLECT_VALUE(VoteFailed)
	MH_ENUM_REFLECT_VALUE(VoteSetup)
	MH_ENUM_REFLECT_VALUE(PlayerBonusPoints)
	MH_ENUM_REFLECT_VALUE(RDTeamPointsChanged)
	MH_ENUM_REFLECT_VALUE(SpawnFlyingBird)
	MH_ENUM_REFLECT_VALUE(PlayerGodRayEffect)
	MH_ENUM_REFLECT_VALUE(PlayerTeleportHomeEffect)
	MH_ENUM_REFLECT_VALUE(MVMStatsReset)
	MH_ENUM_REFLECT_VALUE(MVMPlayerEvent)
	MH_ENUM_REFLECT_VALUE(MVMResetPlayerStats)
	MH_ENUM_REFLECT_VALUE(MVMWaveFailed)
	MH_ENUM_REFLECT_VALUE(MVMAnnouncement)
	MH_ENUM_REFLECT_VALUE(MVMPlayerUpgradedEvent)
	MH_ENUM_REFLECT_VALUE(MVMVictory)
	MH_ENUM_REFLECT_VALUE(MVMWaveChange)
	MH_ENUM_REFLECT_VALUE(MVMLocalPlayerUpgradesClear)
	MH_ENUM_REFLECT_VALUE(MVMLocalPlayerUpgradesValue)
	MH_ENUM_REFLECT_VALUE(MVMLocalPlayerWaveSpendingStats)
	MH_ENUM_REFLECT_VALUE(MVMLocalPlayerWaveSpendingValue)
	MH_ENUM_REFLECT_VALUE(MVMLocalPlayerUpgradeSpending)
	MH_ENUM_REFLECT_VALUE(MVMServerKickTimeUpdate)
	MH_ENUM_REFLECT_VALUE(PlayerLoadoutUpdated)
	MH_ENUM_REFLECT_VALUE(PlayerTauntSoundLoopStart)
	MH_ENUM_REFLECT_VALUE(PlayerTauntSoundLoopEnd)
	MH_ENUM_REFLECT_VALUE(ForcePlayerViewAngles)
	MH_ENUM_REFLECT_VALUE(BonusDucks)
	MH_ENUM_REFLECT_VALUE(EOTLDuckEvent)
	MH_ENUM_REFLECT_VALUE(PlayerPickupWeapon)
	MH_ENUM_REFLECT_VALUE(QuestObjectiveCompleted)
	MH_ENUM_REFLECT_VALUE(SPHapWeapEvent)
	MH_ENUM_REFLECT_VALUE(HapDmg)
	MH_ENUM_REFLECT_VALUE(HapPunch)
	MH_ENUM_REFLECT_VALUE(HapSetDrag)
	MH_ENUM_REFLECT_VALUE(HapSetConst)
	MH_ENUM_REFLECT_VALUE(HapMeleeContact)
MH_ENUM_REFLECT_END()
