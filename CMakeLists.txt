cmake_minimum_required(VERSION 3.17.2)
project(tf2_bot_detector VERSION 1.0.0.26)

include(FetchContent)

# Generate PDBs for release builds - RelWithDebInfo is NOT a Release build!
if (MSVC)
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Zi")
	set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /DEBUG")
	set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /DEBUG")
endif()

set_property(GLOBAL PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE true)
if (MSVC AND (CMAKE_BUILD_TYPE MATCHES "Release"))
	add_link_options(/OPT:REF /OPT:ICF)
endif()

FetchContent_Declare(mh_stuff
	GIT_REPOSITORY https://github.com/PazerOP/stuff.git
	GIT_SHALLOW true
)
if (NOT mh_stuff_POPULATED)
	FetchContent_MakeAvailable(mh_stuff)
endif()

FetchContent_Declare(imgui_desktop
	GIT_REPOSITORY https://github.com/PazerOP/imgui_desktop.git
	GIT_SHALLOW true
)
if (NOT imgui_desktop_POPULATED)
	FetchContent_MakeAvailable(imgui_desktop)
endif()

FetchContent_Declare(nlohmann_json
	GIT_REPOSITORY https://github.com/nlohmann/json.git
	GIT_SHALLOW true
	GIT_PROGRESS true
)
set(JSON_MultipleHeaders on CACHE BOOL "Enables nlohmann/json multi-header mode" FORCE)
set(JSON_BuildTests off CACHE BOOL "Enables nlohmann/json tests" FORCE)
if (NOT nlohmann_json_POPULATED)
	FetchContent_MakeAvailable(nlohmann_json)
endif()

add_subdirectory(submodules/ValveFileVDF)
add_subdirectory(submodules/SourceRCON)

add_executable(tf2_bot_detector WIN32
	"tf2_bot_detector/Actions/RCONActionManager.cpp"
	"tf2_bot_detector/Actions/RCONActionManager.h"
	"tf2_bot_detector/Actions/ActionGenerators.cpp"
	"tf2_bot_detector/Actions/ActionGenerators.h"
	"tf2_bot_detector/Actions/Actions.cpp"
	"tf2_bot_detector/Actions/Actions.h"
	"tf2_bot_detector/Actions/HijackActionManager.h"
	"tf2_bot_detector/Actions/HijackActionManager.cpp"
	"tf2_bot_detector/Actions/ICommandSource.h"
	"tf2_bot_detector/Config/ConfigHelpers.cpp"
	"tf2_bot_detector/Config/ConfigHelpers.h"
	"tf2_bot_detector/Config/JSONHelpers.h"
	"tf2_bot_detector/Config/PlayerListJSON.cpp"
	"tf2_bot_detector/Config/PlayerListJSON.h"
	"tf2_bot_detector/Config/Rules.cpp"
	"tf2_bot_detector/Config/Rules.h"
	"tf2_bot_detector/Config/Settings.cpp"
	"tf2_bot_detector/Config/Settings.h"
	"tf2_bot_detector/ConsoleLog/ConsoleLogParser.h"
	"tf2_bot_detector/ConsoleLog/ConsoleLogParser.cpp"
	"tf2_bot_detector/ConsoleLog/ConsoleLines.cpp"
	"tf2_bot_detector/ConsoleLog/ConsoleLines.h"
	"tf2_bot_detector/ConsoleLog/IConsoleLine.h"
	"tf2_bot_detector/ConsoleLog/IConsoleLineListener.h"
	"tf2_bot_detector/ConsoleLog/NetworkStatus.cpp"
	"tf2_bot_detector/ConsoleLog/NetworkStatus.h"
	"tf2_bot_detector/Networking/GithubAPI.h"
	"tf2_bot_detector/Networking/GithubAPI.cpp"
	"tf2_bot_detector/Networking/HTTPClient.h"
	"tf2_bot_detector/Networking/HTTPClient.cpp"
	"tf2_bot_detector/Networking/HTTPHelpers.h"
	"tf2_bot_detector/Networking/HTTPHelpers.cpp"
	"tf2_bot_detector/Networking/NetworkHelpers.h"
	"tf2_bot_detector/Networking/NetworkHelpers.cpp"
	"tf2_bot_detector/Networking/SteamAPI.h"
	"tf2_bot_detector/Networking/SteamAPI.cpp"
	"tf2_bot_detector/PlatformSpecific/Processes.h"
	"tf2_bot_detector/PlatformSpecific/Shell.h"
	"tf2_bot_detector/PlatformSpecific/Steam.h"
	"tf2_bot_detector/SetupFlow/BasicSettingsPage.h"
	"tf2_bot_detector/SetupFlow/BasicSettingsPage.cpp"
	"tf2_bot_detector/SetupFlow/ISetupFlowPage.h"
	"tf2_bot_detector/SetupFlow/NetworkSettingsPage.h"
	"tf2_bot_detector/SetupFlow/NetworkSettingsPage.cpp"
	"tf2_bot_detector/SetupFlow/RCONConnectionPage.h"
	"tf2_bot_detector/SetupFlow/RCONConnectionPage.cpp"
	"tf2_bot_detector/SetupFlow/TF2CommandLinePage.h"
	"tf2_bot_detector/SetupFlow/TF2CommandLinePage.cpp"
	"tf2_bot_detector/AsyncObject.h"
	"tf2_bot_detector/Clock.cpp"
	"tf2_bot_detector/Clock.h"
	"tf2_bot_detector/CompensatedTS.cpp"
	"tf2_bot_detector/CompensatedTS.h"
	"tf2_bot_detector/FileMods.cpp"
	"tf2_bot_detector/FileMods.h"
	"tf2_bot_detector/ImGui_TF2BotDetector.cpp"
	"tf2_bot_detector/ImGui_TF2BotDetector.h"
	"tf2_bot_detector/IPlayer.cpp"
	"tf2_bot_detector/IPlayer.h"
	"tf2_bot_detector/Log.cpp"
	"tf2_bot_detector/Log.h"
	"tf2_bot_detector/main.cpp"
	"tf2_bot_detector/MainWindow.cpp"
	"tf2_bot_detector/MainWindow.h"
	"tf2_bot_detector/ModeratorLogic.cpp"
	"tf2_bot_detector/ModeratorLogic.h"
	"tf2_bot_detector/PathUtils.cpp"
	"tf2_bot_detector/PathUtils.h"
	"tf2_bot_detector/PlayerStatus.h"
	"tf2_bot_detector/SetupFlow/SetupFlow.cpp"
	"tf2_bot_detector/SetupFlow/SetupFlow.h"
	"tf2_bot_detector/SteamID.cpp"
	"tf2_bot_detector/SteamID.h"
	"tf2_bot_detector/TextUtils.cpp"
	"tf2_bot_detector/TextUtils.h"
	"tf2_bot_detector/TFConstants.h"
	"tf2_bot_detector/WorldState.cpp"
	"tf2_bot_detector/WorldState.h"
	"tf2_bot_detector/Version.h"
	"tf2_bot_detector/Resources.rc"
)

configure_file(tf2_bot_detector/Version.base.h tf2_bot_detector/Version.h)
target_include_directories(tf2_bot_detector PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/tf2_bot_detector)

if(WIN32)
	target_sources(tf2_bot_detector PRIVATE
		"tf2_bot_detector/PlatformSpecific/Windows/CrashHandler.cpp"
		"tf2_bot_detector/PlatformSpecific/Windows/Processes.cpp"
		"tf2_bot_detector/PlatformSpecific/Windows/Shell.cpp"
		"tf2_bot_detector/PlatformSpecific/Windows/Steam.cpp"
		"tf2_bot_detector/PlatformSpecific/Windows/WindowsHelpers.h"
		"tf2_bot_detector/PlatformSpecific/Windows/WinMain.cpp"
	)
endif()

target_include_directories(tf2_bot_detector
	PRIVATE tf2_bot_detector
)

find_package(OpenSSL REQUIRED)
target_link_libraries(tf2_bot_detector)

find_path(HTTPLIB_PATH NAMES httplib.h)
target_include_directories(tf2_bot_detector PRIVATE ${HTTPLIB_PATH})

find_package(libzippp CONFIG REQUIRED)
find_package(cppcoro CONFIG REQUIRED)

target_link_libraries(tf2_bot_detector
	PRIVATE imgui_desktop
	PRIVATE mh_stuff
	PRIVATE ValveFileVDF
	PRIVATE libzip::libzip
	PRIVATE libzippp::libzippp
	PRIVATE cppcoro
	PRIVATE SourceRCON
	PRIVATE OpenSSL::SSL # cpp-httplib requires openssl
	PRIVATE nlohmann_json::nlohmann_json
)

# TODO: Find a way to do this locally
if(MSVC)
	target_compile_options(tf2_bot_detector PRIVATE /WX)
endif()

set_property(TARGET tf2_bot_detector PROPERTY CXX_STANDARD 20)
if (MSVC)
	add_definitions(/await)
endif()

set_target_properties(tf2_bot_detector PROPERTIES
	VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/staging"
)

# "installation" aka create a build we can upload to github as a release
if (WIN32)
	file(GLOB TF2BD_INSTALL_DEPS_DLL LIST_DIRECTORIES false "${CMAKE_BINARY_DIR}/*.dll")
	install(FILES ${TF2BD_INSTALL_DEPS_DLL} DESTINATION /)
endif()
install(DIRECTORY staging/ staging/ DESTINATION "/" FILES_MATCHING
	PATTERN "*"
	PATTERN "staging/cfg/settings.json" EXCLUDE
	PATTERN "staging/logs" EXCLUDE
)
install(TARGETS tf2_bot_detector DESTINATION /)
