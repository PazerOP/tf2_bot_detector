#pragma once

#include "Config/Settings.h"

#include <mh/concurrency/dispatcher.hpp>
#include <imgui_desktop/Application.h>

#include <memory>

namespace tf2_bot_detector
{
	class IBaseTextures;
	class ITextureManager;
	class MainWindow;
	class Settings;

	namespace DB
	{
		class ITempDB;
	}

	class TF2BDApplication final : public ImGuiDesktop::Application
	{
		using Super = ImGuiDesktop::Application;

	public:
		TF2BDApplication();
		~TF2BDApplication();

		static TF2BDApplication& Get();

		Settings& GetSettings();

		std::shared_ptr<ITextureManager> GetTextureManager() const;
		const IBaseTextures& GetBaseTextures() const;

		MainWindow& GetMainWindow();
		const MainWindow& GetMainWindow() const;

		DB::ITempDB& GetTempDB();

	protected:
		void OnAddingManagedWindow(ImGuiDesktop::Window& window) override;
		void OnRemovingManagedWindow(ImGuiDesktop::Window& window) override;

		void OnEndFrame() override;
		void OnOpenGLInit() override;

	private:
		mh::dispatcher m_Dispatcher;

		Settings m_Settings;

		std::shared_ptr<ITextureManager> m_TextureManager;
		std::unique_ptr<IBaseTextures> m_BaseTextures;

		MainWindow* m_MainWindow = nullptr;

		std::unique_ptr<DB::ITempDB> m_TempDB;
	};

	mh::dispatcher GetDispatcher();
	Settings& GetSettings();
}
