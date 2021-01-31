#pragma once

#include <imgui_desktop/Application.h>

namespace tf2_bot_detector
{
	class MainWindow;

	class TF2BDApplication final : public ImGuiDesktop::Application
	{
	public:
		TF2BDApplication();

		static TF2BDApplication& GetApplication();

		MainWindow& GetMainWindow();
		const MainWindow& GetMainWindow() const;

	protected:
		void OnAddingManagedWindow(ImGuiDesktop::Window& window) override;
		void OnRemovingManagedWindow(ImGuiDesktop::Window& window) override;

	private:
		MainWindow* m_MainWindow = nullptr;
	};
}
