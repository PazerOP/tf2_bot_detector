#pragma once

#include <memory>
#include <vector>

namespace tf2_bot_detector
{
	class ActionManager;
	class Settings;

	class SetupFlow final
	{
	public:
		SetupFlow();

		class IPage
		{
		public:
			virtual ~IPage() = default;

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
				ActionManager* m_ActionManager = nullptr;
			};

			[[nodiscard]] virtual OnDrawResult OnDraw(const DrawState& ds) = 0;

			virtual void Init(const Settings& settings) = 0;
			virtual bool CanCommit() const = 0;
			virtual void Commit(Settings& settings) = 0;
			virtual bool WantsSetupText() const { return true; }
			virtual bool WantsContinueButton() const { return true; }
		};

		// Returns true if the setup flow needs to draw.
		[[nodiscard]] bool OnUpdate(const Settings& settings);
		[[nodiscard]] bool OnDraw(Settings& settings, const IPage::DrawState& ds);

		bool ShouldDraw() const { return m_ShouldDraw; }

	private:
		bool m_ShouldDraw = false;
		std::vector<std::unique_ptr<IPage>> m_Pages;

		void GetPageState(const Settings& settings, size_t& currentPage, bool& hasNextPage) const;

		static constexpr size_t INVALID_PAGE = size_t(-1);
		size_t m_ActivePage = INVALID_PAGE;
	};
}
