cmake_minimum_required(VERSION 3.17.2)
project(tf2_bot_detector VERSION 1.1.0.12)

include(GenerateExportHeader)

message("TF2BD CMAKE_BUILD_TYPE = ${CMAKE_BUILD_TYPE}")
message("TF2BD CMAKE_PROJECT_VERSION_TWEAK = ${CMAKE_PROJECT_VERSION_TWEAK}")

option(TF2BD_IS_CI_COMPILE "Set to true if this is a compile on a CI service. Used to help determine if user has made modifications to the source code." off)
if (TF2BD_IS_CI_COMPILE)
	add_compile_definitions(TF2BD_IS_CI_COMPILE=1)
else()
	add_compile_definitions(TF2BD_IS_CI_COMPILE=0)
endif()

# Generate PDBs for release builds - RelWithDebInfo is NOT a Release build!
if (MSVC)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Zi")
	set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /DEBUG")
	set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /DEBUG")
endif()

set_property(GLOBAL PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE true)
if (MSVC AND (CMAKE_BUILD_TYPE MATCHES "Release"))
	add_link_options(/OPT:REF /OPT:ICF)
endif()

add_subdirectory(submodules/ValveFileVDF)
add_subdirectory(submodules/SourceRCON)
add_subdirectory(submodules/imgui_desktop)
add_subdirectory(submodules/mh_stuff)

set(ENABLE_EXAMPLES off CACHE BOOL "Build examples" FORCE)
add_subdirectory("submodules/cppcoro")

if (WIN32)
	add_library(tf2_bot_detector SHARED)
	set_target_properties(tf2_bot_detector PROPERTIES PDB_NAME "tf2_bot_detector_dll")

	generate_export_header(tf2_bot_detector
		EXPORT_FILE_NAME "tf2_bot_detector/tf2_bot_detector_export.h"
	)

	add_executable(tf2_bot_detector_launcher WIN32
		"tf2_bot_detector/Platform/Windows/CrashHandler.cpp"
		"tf2_bot_detector/Launcher/main.cpp"
		"tf2_bot_detector/Launcher/Resources.rc"
	)
	target_compile_definitions(tf2_bot_detector_launcher PRIVATE TF2BD_LAUNCHER_USE_WINMAIN)

	target_include_directories(tf2_bot_detector_launcher PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/tf2_bot_detector)
	target_link_libraries(tf2_bot_detector_launcher PRIVATE tf2_bot_detector)
	set_target_properties(tf2_bot_detector_launcher PROPERTIES OUTPUT_NAME "tf2_bot_detector")
	set_property(TARGET tf2_bot_detector_launcher PROPERTY CXX_STANDARD 17)
else()
	add_executable(tf2_bot_detector
		"tf2_bot_detector/Launcher/main.cpp"
	)
endif()

target_sources(tf2_bot_detector PRIVATE
	"tf2_bot_detector/Actions/RCONActionManager.cpp"
	"tf2_bot_detector/Actions/RCONActionManager.h"
	"tf2_bot_detector/Actions/ActionGenerators.cpp"
	"tf2_bot_detector/Actions/ActionGenerators.h"
	"tf2_bot_detector/Actions/Actions.cpp"
	"tf2_bot_detector/Actions/Actions.h"
	"tf2_bot_detector/Actions/HijackActionManager.h"
	"tf2_bot_detector/Actions/HijackActionManager.cpp"
	"tf2_bot_detector/Actions/IActionManager.h"
	"tf2_bot_detector/Actions/ICommandSource.h"
	"tf2_bot_detector/Config/ConfigHelpers.cpp"
	"tf2_bot_detector/Config/ConfigHelpers.h"
	"tf2_bot_detector/Config/DRPInfo.cpp"
	"tf2_bot_detector/Config/DRPInfo.h"
	"tf2_bot_detector/Config/PlayerListJSON.cpp"
	"tf2_bot_detector/Config/PlayerListJSON.h"
	"tf2_bot_detector/Config/Rules.cpp"
	"tf2_bot_detector/Config/Rules.h"
	"tf2_bot_detector/Config/Settings.cpp"
	"tf2_bot_detector/Config/Settings.h"
	"tf2_bot_detector/Config/SponsorsList.h"
	"tf2_bot_detector/Config/SponsorsList.cpp"
	"tf2_bot_detector/ConsoleLog/ConsoleLogParser.h"
	"tf2_bot_detector/ConsoleLog/ConsoleLogParser.cpp"
	"tf2_bot_detector/ConsoleLog/ConsoleLines.cpp"
	"tf2_bot_detector/ConsoleLog/ConsoleLines.h"
	"tf2_bot_detector/ConsoleLog/IConsoleLine.h"
	"tf2_bot_detector/ConsoleLog/ConsoleLineListener.cpp"
	"tf2_bot_detector/ConsoleLog/ConsoleLineListener.h"
	"tf2_bot_detector/ConsoleLog/NetworkStatus.cpp"
	"tf2_bot_detector/ConsoleLog/NetworkStatus.h"
	"tf2_bot_detector/GameData/MatchmakingQueue.h"
	"tf2_bot_detector/GameData/TFClassType.h"
	"tf2_bot_detector/GameData/TFParty.h"
	"tf2_bot_detector/GameData/UserMessageType.h"
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
	"tf2_bot_detector/Platform/Platform.h"
	"tf2_bot_detector/SetupFlow/BasicSettingsPage.h"
	"tf2_bot_detector/SetupFlow/BasicSettingsPage.cpp"
	"tf2_bot_detector/SetupFlow/ChatWrappersGeneratorPage.h"
	"tf2_bot_detector/SetupFlow/ChatWrappersGeneratorPage.cpp"
	"tf2_bot_detector/SetupFlow/ChatWrappersVerifyPage.h"
	"tf2_bot_detector/SetupFlow/ChatWrappersVerifyPage.cpp"
	"tf2_bot_detector/SetupFlow/ISetupFlowPage.h"
	"tf2_bot_detector/SetupFlow/NetworkSettingsPage.h"
	"tf2_bot_detector/SetupFlow/NetworkSettingsPage.cpp"
	"tf2_bot_detector/SetupFlow/SetupFlow.cpp"
	"tf2_bot_detector/SetupFlow/SetupFlow.h"
	"tf2_bot_detector/SetupFlow/TF2CommandLinePage.h"
	"tf2_bot_detector/SetupFlow/TF2CommandLinePage.cpp"
	"tf2_bot_detector/UI/ImGui_TF2BotDetector.cpp"
	"tf2_bot_detector/UI/ImGui_TF2BotDetector.h"
	"tf2_bot_detector/UI/MainWindow.cpp"
	"tf2_bot_detector/UI/MainWindow.Scoreboard.cpp"
	"tf2_bot_detector/UI/MainWindow.h"
	"tf2_bot_detector/Util/JSONUtils.h"
	"tf2_bot_detector/Util/PathUtils.cpp"
	"tf2_bot_detector/Util/PathUtils.h"
	"tf2_bot_detector/Util/TextUtils.cpp"
	"tf2_bot_detector/Util/TextUtils.h"
	"tf2_bot_detector/BaseTextures.h"
	"tf2_bot_detector/BaseTextures.cpp"
	"tf2_bot_detector/BatchedAction.h"
	"tf2_bot_detector/Bitmap.h"
	"tf2_bot_detector/Bitmap.cpp"
	"tf2_bot_detector/Clock.cpp"
	"tf2_bot_detector/Clock.h"
	"tf2_bot_detector/CompensatedTS.cpp"
	"tf2_bot_detector/CompensatedTS.h"
	"tf2_bot_detector/Config/ChatWrappers.cpp"
	"tf2_bot_detector/Config/ChatWrappers.h"
	"tf2_bot_detector/DLLMain.cpp"
	"tf2_bot_detector/DLLMain.h"
	"tf2_bot_detector/Filesystem.cpp"
	"tf2_bot_detector/Filesystem.h"
	"tf2_bot_detector/IPlayer.cpp"
	"tf2_bot_detector/IPlayer.h"
	"tf2_bot_detector/Log.cpp"
	"tf2_bot_detector/Log.h"
	"tf2_bot_detector/ModeratorLogic.cpp"
	"tf2_bot_detector/ModeratorLogic.h"
	"tf2_bot_detector/PlayerStatus.h"
	"tf2_bot_detector/SteamID.cpp"
	"tf2_bot_detector/SteamID.h"
	"tf2_bot_detector/TextureManager.h"
	"tf2_bot_detector/TextureManager.cpp"
	"tf2_bot_detector/TFConstants.h"
	"tf2_bot_detector/Version.h"
	"tf2_bot_detector/WorldEventListener.cpp"
	"tf2_bot_detector/WorldEventListener.h"
	"tf2_bot_detector/WorldState.cpp"
	"tf2_bot_detector/WorldState.h"
)

configure_file(tf2_bot_detector/Version.base.h tf2_bot_detector/Version.h)
target_include_directories(tf2_bot_detector PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/tf2_bot_detector)

if(WIN32)
	if ((CMAKE_BUILD_TYPE MATCHES "Release"))
		set(TF2BD_RESOURCE_FILEFLAGS "0")
	else()
		set(TF2BD_RESOURCE_FILEFLAGS "VS_FF_DEBUG")
	endif()
	configure_file(tf2_bot_detector/Resources.base.rc tf2_bot_detector/Resources.rc)

	target_sources(tf2_bot_detector PRIVATE
		"tf2_bot_detector/Platform/Windows/Processes.cpp"
		"tf2_bot_detector/Platform/Windows/Shell.cpp"
		"tf2_bot_detector/Platform/Windows/Steam.cpp"
		"tf2_bot_detector/Platform/Windows/WindowsHelpers.h"
		"tf2_bot_detector/Resources.rc"
		"tf2_bot_detector/Platform/Windows/Windows.cpp"
	)
endif()

target_include_directories(tf2_bot_detector
	PRIVATE tf2_bot_detector
)
target_compile_definitions(tf2_bot_detector PRIVATE WIN32_LEAN_AND_MEAN)

option(TF2BD_ENABLE_DISCORD_INTEGRATION "Enable discord integration" on)
if (TF2BD_ENABLE_DISCORD_INTEGRATION)
	target_compile_definitions(tf2_bot_detector PRIVATE TF2BD_ENABLE_DISCORD_INTEGRATION)
	find_library(DISCORD_GAME_SDK discord_game_sdk)
	message("DISCORD_GAME_SDK = ${DISCORD_GAME_SDK}")
	target_link_libraries(tf2_bot_detector PRIVATE ${DISCORD_GAME_SDK})

	find_path(GAME_SDK_INCLUDE discord-game-sdk/discord.h)
	target_include_directories(tf2_bot_detector PRIVATE ${DISCORD_GAME_SDK_INCLUDE})

	find_library(DISCORD_CPP_GAME_SDK discord_game_sdk_cpp)
	message("DISCORD_CPP_GAME_SDK = ${DISCORD_CPP_GAME_SDK}")
	target_link_libraries(tf2_bot_detector PRIVATE ${DISCORD_CPP_GAME_SDK})

	target_sources(tf2_bot_detector PRIVATE
		"tf2_bot_detector/DiscordRichPresence.cpp"
		"tf2_bot_detector/DiscordRichPresence.h"
	)
endif()

find_package(OpenSSL REQUIRED)
target_link_libraries(tf2_bot_detector)

find_path(HTTPLIB_PATH NAMES httplib.h)
target_include_directories(tf2_bot_detector PRIVATE ${HTTPLIB_PATH})

find_package(nlohmann_json CONFIG REQUIRED)
find_package(libzippp CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)

target_link_libraries(tf2_bot_detector PRIVATE
	imgui_desktop
	mh_stuff
	ValveFileVDF
	libzip::libzip
	libzippp::libzippp
	cppcoro
	SourceRCON
	OpenSSL::SSL # cpp-httplib requires openssl
	nlohmann_json::nlohmann_json
	fmt::fmt
)

option(TF2BD_ENABLE_TESTS "Enable test compilation" off)
if (TF2BD_ENABLE_TESTS)
	enable_testing()

	find_package(Catch2 CONFIG REQUIRED)
	target_link_libraries(tf2_bot_detector PRIVATE Catch2::Catch2)
	target_compile_definitions(tf2_bot_detector PRIVATE TF2BD_ENABLE_TESTS)
	target_sources(tf2_bot_detector PRIVATE
		"tf2_bot_detector/Tests/Catch2.cpp"
		"tf2_bot_detector/Tests/ConsoleLineTests.cpp"
		"tf2_bot_detector/Tests/Tests.h"
	)

	SET(TF2BD_ENABLE_CLI_EXE true)

	add_test(NAME TF2BD_Tests COMMAND tf2_bot_detector_cli --run-tests
		WORKING_DIRECTORY staging
	)
endif()

if(TF2BD_ENABLE_CLI_EXE)
	add_executable(tf2_bot_detector_cli "tf2_bot_detector/Launcher/main.cpp")
	target_include_directories(tf2_bot_detector_cli PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/tf2_bot_detector)
	target_link_libraries(tf2_bot_detector_cli PRIVATE tf2_bot_detector)
	set_property(TARGET tf2_bot_detector_cli PROPERTY CXX_STANDARD 17)
endif()

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
