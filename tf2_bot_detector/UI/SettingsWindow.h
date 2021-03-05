#pragma once

#include <imgui_desktop/Window.h>

namespace tf2_bot_detector
{
	class MainWindow;
	class Settings;

	class SettingsWindow final : public ImGuiDesktop::Window
	{
	public:
		SettingsWindow(ImGuiDesktop::Application& app, Settings& settings, MainWindow& mainWindow);

	private:
		void OnDraw() override;

		void OnDrawASOSettings();
		void OnDrawCompatibilitySettings();
		void OnDrawLoggingSettings();
		void OnDrawModerationSettings();
		void OnDrawModSettings();
		void OnDrawPerformanceSettings();
		void OnDrawServiceIntegrationSettings();
		void OnDrawUISettings();

		Settings& m_Settings;
		MainWindow& m_MainWindow;

		bool m_ModsChanged = false;
	};
}
