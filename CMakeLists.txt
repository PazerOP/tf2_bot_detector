cmake_minimum_required(VERSION 3.1.0)
project(tf2_bot_detector VERSION 1.0.9)

set(JSON_BuildTests OFF)

# Generate PDBs for release builds - RelWithDebInfo is NOT a Release build!
if (MSVC)
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Zi")
	set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /DEBUG")
	set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /DEBUG")
endif()

# set(VCPKG_APPLOCAL_DEPS ON)

add_subdirectory(submodules/imgui_desktop)
add_subdirectory(submodules/implot)
add_subdirectory(submodules/mh_stuff)
# add_subdirectory(submodules/nlohmann_json)
add_subdirectory(submodules/ValveFileVDF)

add_executable(tf2_bot_detector
	tf2_bot_detector/Config/ConfigHelpers.cpp
	tf2_bot_detector/Config/ConfigHelpers.h
	tf2_bot_detector/Config/JSONHelpers.h
	tf2_bot_detector/Config/PlayerListJSON.cpp
	tf2_bot_detector/Config/PlayerListJSON.h
	tf2_bot_detector/Config/Rules.cpp
	tf2_bot_detector/Config/Rules.h
	tf2_bot_detector/Config/Settings.cpp
	tf2_bot_detector/Config/Settings.h
	tf2_bot_detector/Config/Whitelist.cpp
	tf2_bot_detector/Config/Whitelist.h
	tf2_bot_detector/PlatformSpecific/Shell.h
	tf2_bot_detector/PlatformSpecific/Shitty.h
	tf2_bot_detector/ActionManager.cpp
	tf2_bot_detector/ActionManager.h
	tf2_bot_detector/Actions.cpp
	tf2_bot_detector/Actions.h
	tf2_bot_detector/Clock.cpp
	tf2_bot_detector/Clock.h
	tf2_bot_detector/CompensatedTS.cpp
	tf2_bot_detector/CompensatedTS.h
	tf2_bot_detector/ConsoleLines.cpp
	tf2_bot_detector/ConsoleLines.h
	tf2_bot_detector/FileMods.cpp
	tf2_bot_detector/FileMods.h
	tf2_bot_detector/ICommandSource.h
	tf2_bot_detector/IConsoleLine.h
	tf2_bot_detector/IConsoleLineListener.h
	tf2_bot_detector/ImGui_TF2BotDetector.cpp
	tf2_bot_detector/ImGui_TF2BotDetector.h
	tf2_bot_detector/IPlayer.cpp
	tf2_bot_detector/IPlayer.h
	tf2_bot_detector/Log.cpp
	tf2_bot_detector/Log.h
	tf2_bot_detector/main.cpp
	tf2_bot_detector/MainWindow.cpp
	tf2_bot_detector/MainWindow.h
	tf2_bot_detector/ModeratorLogic.cpp
	tf2_bot_detector/ModeratorLogic.h
	tf2_bot_detector/NetworkStatus.cpp
	tf2_bot_detector/NetworkStatus.h
	tf2_bot_detector/PathUtils.cpp
	tf2_bot_detector/PathUtils.h
	tf2_bot_detector/PeriodicActions.cpp
	tf2_bot_detector/PeriodicActions.h
	tf2_bot_detector/PlayerStatus.h
	tf2_bot_detector/SetupFlow.cpp
	tf2_bot_detector/SetupFlow.h
	tf2_bot_detector/SteamID.cpp
	tf2_bot_detector/SteamID.h
	tf2_bot_detector/TextUtils.cpp
	tf2_bot_detector/TextUtils.h
	tf2_bot_detector/TFConstants.h
	tf2_bot_detector/WorldState.cpp
	tf2_bot_detector/WorldState.h
)

if(WIN32)
	target_sources(tf2_bot_detector PRIVATE
		tf2_bot_detector/PlatformSpecific/Windows/CrashHandler.cpp
		tf2_bot_detector/PlatformSpecific/Windows/Shell.cpp
		tf2_bot_detector/PlatformSpecific/Windows/Shitty.cpp
	)
endif()

target_include_directories(tf2_bot_detector
	PRIVATE tf2_bot_detector
	PRIVATE submodules/nlohmann_json/include
)

target_link_libraries(tf2_bot_detector
	imgui_desktop
	implot
	mh_stuff
	ValveFileVDF
)

set_property(TARGET tf2_bot_detector PROPERTY CXX_STANDARD 20)
if (MSVC)
	add_definitions(/await)
endif()

# set(CPACK_PROJECT_NAME ${PROJECT_NAME})
# set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
# include(CPack)
