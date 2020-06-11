cmake_minimum_required(VERSION 3.1.0)
project(tf2_bot_detector VERSION 1.0.9)

set(JSON_BuildTests OFF)

add_subdirectory(submodules/imgui_desktop)
add_subdirectory(submodules/implot)
add_subdirectory(submodules/mh_stuff)
# add_subdirectory(submodules/nlohmann_json)
add_subdirectory(submodules/ValveFileVDF)

add_executable(tf2_bot_detector
	tf2_bot_detector/Config/ConfigHelpers.cpp
	tf2_bot_detector/Config/PlayerListJSON.cpp
	tf2_bot_detector/Config/Rules.cpp
	tf2_bot_detector/Config/Settings.cpp
	tf2_bot_detector/Config/Whitelist.cpp
	tf2_bot_detector/ActionManager.cpp
	tf2_bot_detector/Actions.cpp
	tf2_bot_detector/Clock.cpp
	tf2_bot_detector/CompensatedTS.cpp
	tf2_bot_detector/ConsoleLines.cpp
	tf2_bot_detector/FileMods.cpp
	tf2_bot_detector/ImGui_TF2BotDetector.cpp
	tf2_bot_detector/IPlayer.cpp
	tf2_bot_detector/Log.cpp
	tf2_bot_detector/main.cpp
	tf2_bot_detector/MainWindow.cpp
	tf2_bot_detector/ModeratorLogic.cpp
	tf2_bot_detector/NetworkStatus.cpp
	tf2_bot_detector/PathUtils.cpp
	tf2_bot_detector/PeriodicActions.cpp
	tf2_bot_detector/SetupFlow.cpp
	tf2_bot_detector/SteamID.cpp
	tf2_bot_detector/TextUtils.cpp
	tf2_bot_detector/WorldState.cpp
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
if (WIN32)
	add_definitions(/await)
endif()

# set(CPACK_PROJECT_NAME ${PROJECT_NAME})
# set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
# include(CPack)
