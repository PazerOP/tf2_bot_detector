#include "Application.h"
#include "UI/MainWindow.h"

#include <mh/error/ensure.hpp>

#include <memory>

using namespace tf2_bot_detector;

static TF2BDApplication* s_Application;

TF2BDApplication::TF2BDApplication()
{
	s_Application = this;
	DebugLog("Initializing MainWindow...");
	AddManagedWindow(std::make_unique<tf2_bot_detector::MainWindow>(*this));
}

TF2BDApplication& TF2BDApplication::GetApplication()
{
	return *mh_ensure(s_Application);
}

MainWindow& TF2BDApplication::GetMainWindow()
{
	return *mh_ensure(m_MainWindow);
}

const MainWindow& TF2BDApplication::GetMainWindow() const
{
	return *mh_ensure(m_MainWindow);
}

void TF2BDApplication::OnAddingManagedWindow(ImGuiDesktop::Window& window)
{
	if (auto mainWindow = dynamic_cast<MainWindow*>(&window))
	{
		if (mh_ensure(!m_MainWindow))
			m_MainWindow = mainWindow;
	}
}

void TF2BDApplication::OnRemovingManagedWindow(ImGuiDesktop::Window& window)
{
	if (&window == m_MainWindow)
		m_MainWindow = nullptr;
}
