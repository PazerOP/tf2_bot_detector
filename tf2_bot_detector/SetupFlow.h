#pragma once

namespace tf2_bot_detector
{
	class Settings;

	class SetupFlow final
	{
	public:
		// Returns true if the setup flow needs to draw.
		[[nodiscard]] bool OnUpdate(const Settings& settings);
		[[nodiscard]] bool OnDraw(Settings& settings);

		bool ShouldDraw() const { return m_ShouldDraw; }

	private:
		bool m_ShouldDraw = false;

		static bool ValidateSettings(const Settings& settings);
	};
}
