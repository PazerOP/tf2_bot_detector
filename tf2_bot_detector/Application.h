#pragma once

#include <imgui_desktop/Application.h>

#include <memory>

namespace tf2_bot_detector
{
	class MainWindow;

	namespace DB
	{
		class ITempDB;
	}

	class TF2BDApplication final : public ImGuiDesktop::Application
	{
	public:
		TF2BDApplication();
		~TF2BDApplication();

		static TF2BDApplication& GetApplication();

		MainWindow& GetMainWindow();
		const MainWindow& GetMainWindow() const;

		DB::ITempDB& GetTempDB();

	protected:
		void OnAddingManagedWindow(ImGuiDesktop::Window& window) override;
		void OnRemovingManagedWindow(ImGuiDesktop::Window& window) override;

	private:
		MainWindow* m_MainWindow = nullptr;

		std::unique_ptr<DB::ITempDB> m_TempDB;
	};
}
