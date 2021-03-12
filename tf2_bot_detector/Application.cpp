#include "Application.h"
#include "DB/TempDB.h"
#include "UI/MainWindow.h"
#include "BaseTextures.h"
#include "TextureManager.h"

#include <mh/error/ensure.hpp>

#include <cassert>

using namespace tf2_bot_detector;

static TF2BDApplication* s_Application;

TF2BDApplication::TF2BDApplication()
{
	assert(!s_Application);
	s_Application = this;

	m_TempDB = DB::ITempDB::Create();

	DebugLog("Initializing MainWindow...");
	AddManagedWindow(std::make_unique<tf2_bot_detector::MainWindow>(*this));
}

TF2BDApplication::~TF2BDApplication() = default;

TF2BDApplication& TF2BDApplication::Get()
{
	return *mh_ensure(s_Application);
}

Settings& TF2BDApplication::GetSettings()
{
	return m_Settings;
}

std::shared_ptr<ITextureManager> TF2BDApplication::GetTextureManager() const
{
	return mh_ensure(m_TextureManager);
}

const IBaseTextures& TF2BDApplication::GetBaseTextures() const
{
	return *mh_ensure(m_BaseTextures);
}

MainWindow& TF2BDApplication::GetMainWindow()
{
	return *mh_ensure(m_MainWindow);
}

const MainWindow& TF2BDApplication::GetMainWindow() const
{
	return *mh_ensure(m_MainWindow);
}

DB::ITempDB& TF2BDApplication::GetTempDB()
{
	return *mh_ensure(m_TempDB.get());
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

void TF2BDApplication::OnEndFrame()
{
	Super::OnEndFrame();

	GetTextureManager()->EndFrame();
}

void TF2BDApplication::OnOpenGLInit()
{
	Super::OnOpenGLInit();

	assert(!m_TextureManager);
	assert(!m_BaseTextures);

	m_TextureManager = ITextureManager::Create();
	m_BaseTextures = IBaseTextures::Create(m_TextureManager);
}

mh::dispatcher tf2_bot_detector::GetDispatcher()
{
	static mh::dispatcher s_Dispatcher;
	return s_Dispatcher;
}

Settings& tf2_bot_detector::GetSettings()
{
	return TF2BDApplication::Get().GetSettings();
}
