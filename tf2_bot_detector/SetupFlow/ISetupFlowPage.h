#pragma once

#undef DrawState

namespace tf2_bot_detector
{
	class IActionManager;
	class IUpdateManager;
	class Settings;

	enum class SetupFlowPage
	{
		Invalid = -1,

		PermissionsCheck = 0,
		NetworkSettings,
		UpdateCheck,
		CheckSteamOpen,
		BasicSettings,
		AddonManager,
		ChatWrappersGenerate,
		TF2CommandLine,
		ChatWrappersVerify,
	};

	class ISetupFlowPage
	{
	public:
		virtual ~ISetupFlowPage() = default;

		enum class ValidateSettingsResult
		{
			// Everything is A-OK. No reason to open this page for user interaction.
			Success,

			// Either something's wrong, or we need to run some blocking logic.
			// Open this page.
			TriggerOpen,
		};

		[[nodiscard]] virtual ValidateSettingsResult ValidateSettings(const Settings& settings) const = 0;

		enum class OnDrawResult
		{
			// Draw again next frame unless the user clicks "Done"/"Next"
			ContinueDrawing,

			// Pretend the user pressed "Done"/"Next"
			EndDrawing,
		};

		struct DrawState
		{
			IActionManager* m_ActionManager = nullptr;
			IUpdateManager* m_UpdateManager = nullptr;
			Settings* m_Settings = nullptr;
		};
		[[nodiscard]] virtual OnDrawResult OnDraw(const DrawState& ds) = 0;

		struct InitState
		{
			explicit InitState(const Settings& settings) : m_Settings(settings) {}

			const Settings& m_Settings;
			IUpdateManager* m_UpdateManager = nullptr;
		};
		virtual void Init(const InitState& is) = 0;
		virtual bool CanCommit() const = 0;

		struct CommitState
		{
			explicit CommitState(Settings& settings) : m_Settings(settings) {}

			Settings& m_Settings;
			IUpdateManager* m_UpdateManager = nullptr;
		};
		virtual void Commit(const CommitState& cs) = 0;
		virtual bool WantsSetupText() const { return true; }
		virtual bool WantsContinueButton() const { return true; }

		virtual SetupFlowPage GetPage() const = 0;
	};
}
