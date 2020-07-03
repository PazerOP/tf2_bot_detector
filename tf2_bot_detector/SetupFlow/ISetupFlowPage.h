#pragma once

namespace tf2_bot_detector
{
	class RCONActionManager;
#ifdef _WIN32
	class HijackActionManager;
#endif
	class Settings;

	class ISetupFlowPage
	{
	public:
		virtual ~ISetupFlowPage() = default;

		[[nodiscard]] virtual bool ValidateSettings(const Settings& settings) const = 0;

		enum class OnDrawResult
		{
			// Draw again next frame unless the user clicks "Done"/"Next"
			ContinueDrawing,

			// Pretend the user pressed "Done"/"Next"
			EndDrawing,
		};

		struct DrawState
		{
#ifdef _WIN32
			HijackActionManager* m_HijackActionManager = nullptr;
#endif
			RCONActionManager* m_ActionManager = nullptr;
			Settings* m_Settings = nullptr;
		};

		[[nodiscard]] virtual OnDrawResult OnDraw(const DrawState& ds) = 0;

		virtual void Init(const Settings& settings) = 0;
		virtual bool CanCommit() const = 0;
		virtual void Commit(Settings& settings) = 0;
		virtual bool WantsSetupText() const { return true; }
		virtual bool WantsContinueButton() const { return true; }
	};
}
