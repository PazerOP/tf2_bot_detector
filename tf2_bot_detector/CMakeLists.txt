cmake_minimum_required(VERSION 3.16)

include(GenerateExportHeader)

if (WIN32)
	add_library(tf2_bot_detector SHARED)
	set_target_properties(tf2_bot_detector PROPERTIES PDB_NAME "tf2_bot_detector_dll")

	generate_export_header(tf2_bot_detector
		EXPORT_FILE_NAME "tf2_bot_detector_export.h"
	)

	add_executable(tf2_bot_detector_launcher WIN32
		"Platform/Windows/CrashHandler.cpp"
		"Launcher/main.cpp"
		"Launcher/Resources.rc"
	)
	target_compile_definitions(tf2_bot_detector_launcher PRIVATE TF2BD_LAUNCHER_USE_WINMAIN)

	target_include_directories(tf2_bot_detector_launcher PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
	target_link_libraries(tf2_bot_detector_launcher PRIVATE tf2_bot_detector)
	set_target_properties(tf2_bot_detector_launcher PROPERTIES OUTPUT_NAME "tf2_bot_detector")
	target_compile_features(tf2_bot_detector_launcher PUBLIC cxx_std_17)
else()
	add_executable(tf2_bot_detector
		"Launcher/main.cpp"
	)
endif()

target_compile_features(tf2_bot_detector PUBLIC cxx_std_20)
set_target_properties(tf2_bot_detector PROPERTIES
	VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/staging"
)

target_compile_definitions(tf2_bot_detector PRIVATE WIN32_LEAN_AND_MEAN)
target_include_directories(tf2_bot_detector PRIVATE
	"."
	"${CMAKE_CURRENT_BINARY_DIR}"
)

configure_file(Version.base.h Version.h)

target_sources(tf2_bot_detector PRIVATE
	"Actions/RCONActionManager.cpp"
	"Actions/RCONActionManager.h"
	"Actions/ActionGenerators.cpp"
	"Actions/ActionGenerators.h"
	"Actions/Actions.cpp"
	"Actions/Actions.h"
	"Actions/HijackActionManager.h"
	"Actions/HijackActionManager.cpp"
	"Actions/IActionManager.h"
	"Actions/ICommandSource.h"
	"Config/AccountAges.cpp"
	"Config/AccountAges.h"
	"Config/ConfigHelpers.cpp"
	"Config/ConfigHelpers.h"
	"Config/DRPInfo.cpp"
	"Config/DRPInfo.h"
	"Config/PlayerListJSON.cpp"
	"Config/PlayerListJSON.h"
	"Config/Rules.cpp"
	"Config/Rules.h"
	"Config/Settings.cpp"
	"Config/Settings.h"
	"Config/SponsorsList.h"
	"Config/SponsorsList.cpp"
	"ConsoleLog/ConsoleLogParser.h"
	"ConsoleLog/ConsoleLogParser.cpp"
	"ConsoleLog/ConsoleLines.cpp"
	"ConsoleLog/ConsoleLines.h"
	"ConsoleLog/IConsoleLine.h"
	"ConsoleLog/ConsoleLineListener.cpp"
	"ConsoleLog/ConsoleLineListener.h"
	"ConsoleLog/NetworkStatus.cpp"
	"ConsoleLog/NetworkStatus.h"
	"DB/DBHelpers.h"
	"DB/DBHelpers.cpp"
	"DB/TempDB.h"
	"DB/TempDB.cpp"
	"GameData/MatchmakingQueue.h"
	"GameData/TFClassType.h"
	"GameData/TFParty.h"
	"GameData/UserMessageType.h"
	"Networking/GithubAPI.h"
	"Networking/GithubAPI.cpp"
	"Networking/HTTPClient.h"
	"Networking/HTTPClient.cpp"
	"Networking/HTTPHelpers.h"
	"Networking/HTTPHelpers.cpp"
	"Networking/LogsTFAPI.cpp"
	"Networking/LogsTFAPI.h"
	"Networking/NetworkHelpers.h"
	"Networking/NetworkHelpers.cpp"
	"Networking/SteamAPI.h"
	"Networking/SteamAPI.cpp"
	"Platform/Platform.h"
	"SetupFlow/AddonManagerPage.h"
	"SetupFlow/AddonManagerPage.cpp"
	"SetupFlow/BasicSettingsPage.h"
	"SetupFlow/BasicSettingsPage.cpp"
	"SetupFlow/ChatWrappersGeneratorPage.h"
	"SetupFlow/ChatWrappersGeneratorPage.cpp"
	"SetupFlow/ChatWrappersVerifyPage.h"
	"SetupFlow/ChatWrappersVerifyPage.cpp"
	"SetupFlow/CheckSteamOpenPage.cpp"
	"SetupFlow/ISetupFlowPage.h"
	"SetupFlow/NetworkSettingsPage.h"
	"SetupFlow/NetworkSettingsPage.cpp"
	"SetupFlow/PermissionsCheckPage.cpp"
	"SetupFlow/SetupFlow.cpp"
	"SetupFlow/SetupFlow.h"
	"SetupFlow/TF2CommandLinePage.h"
	"SetupFlow/TF2CommandLinePage.cpp"
	"SetupFlow/UpdateCheckPage.cpp"
	"UI/ImGui_TF2BotDetector.cpp"
	"UI/ImGui_TF2BotDetector.h"
	"UI/MainWindow.cpp"
	"UI/MainWindow.Scoreboard.cpp"
	"UI/MainWindow.h"
	"UI/SettingsWindow.cpp"
	"UI/SettingsWindow.h"
	"Util/JSONUtils.h"
	"Util/PathUtils.cpp"
	"Util/PathUtils.h"
	"Util/TextUtils.cpp"
	"Util/TextUtils.h"
	"Application.cpp"
	"Application.h"
	"BaseTextures.h"
	"BaseTextures.cpp"
	"BatchedAction.h"
	"Bitmap.h"
	"Bitmap.cpp"
	"Clock.cpp"
	"Clock.h"
	"CompensatedTS.cpp"
	"CompensatedTS.h"
	"Config/ChatWrappers.cpp"
	"Config/ChatWrappers.h"
	"DLLMain.cpp"
	"DLLMain.h"
	"Filesystem.cpp"
	"Filesystem.h"
	"GenericErrors.cpp"
	"GenericErrors.h"
	"GlobalDispatcher.h"
	"IPlayer.cpp"
	"IPlayer.h"
	"Log.cpp"
	"Log.h"
	"ModeratorLogic.cpp"
	"ModeratorLogic.h"
	"PlayerStatus.h"
	"SteamID.cpp"
	"SteamID.h"
	"TextureManager.h"
	"TextureManager.cpp"
	"TFConstants.h"
	"UpdateManager.h"
	"UpdateManager.cpp"
	"Version.h"
	"Version.cpp"
	"WorldEventListener.cpp"
	"WorldEventListener.h"
	"WorldState.cpp"
	"WorldState.h"
)

target_precompile_headers(tf2_bot_detector
	PUBLIC
		"Clock.h"
	PRIVATE
		"Log.h"
)

if(WIN32)
	configure_file(Resources.base.rc Resources.rc)

	target_sources(tf2_bot_detector PRIVATE
		"Platform/Windows/Processes.cpp"
		"Platform/Windows/Shell.cpp"
		"Platform/Windows/Steam.cpp"
		"Platform/Windows/WindowsHelpers.h"
		"Resources.rc"
		"Platform/Windows/Windows.cpp"
		"Platform/Windows/PlatformInstall.cpp"
		"Platform/Windows/Platform.cpp"
	)
endif()

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
		"DiscordRichPresence.cpp"
		"DiscordRichPresence.h"
	)

	# We really only need this library for one thing in the DRP integration.
	# Later on it will probably be moved out of this if statement when it is required elsewhere in the code.
	find_package(cryptopp CONFIG REQUIRED)
	target_link_libraries(tf2_bot_detector PRIVATE cryptopp-static)

endif()

if (WIN32)
	INCLUDE_TF2BD_WINRT(tf2_bot_detector)
endif()

find_package(nlohmann_json CONFIG REQUIRED)
find_package(libzip CONFIG REQUIRED)
find_package(libzippp CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(BZip2 REQUIRED)
find_package(SQLiteCpp CONFIG REQUIRED)
find_package(ZLIB REQUIRED)
find_package(cpprestsdk CONFIG REQUIRED)

target_link_libraries(tf2_bot_detector PRIVATE
	tf2_bot_detector::common
	imgui_desktop
	ValveFileVDF
	libzip::zip
	libzippp::libzippp
	SourceRCON
	nlohmann_json::nlohmann_json
	fmt::fmt
	BZip2::BZip2
	SQLiteCpp
	ZLIB::ZLIB
	cpprestsdk::cpprest
)

if (TF2BD_ENABLE_TESTS)
	enable_testing()

	find_package(Catch2 CONFIG REQUIRED)
	target_link_libraries(tf2_bot_detector PRIVATE Catch2::Catch2)
	target_compile_definitions(tf2_bot_detector PRIVATE TF2BD_ENABLE_TESTS)
	target_sources(tf2_bot_detector PRIVATE
		"Tests/Catch2.cpp"
		"Tests/ConsoleLineTests.cpp"
		"Tests/FormattingTests.cpp"
		"Tests/HumanDurationTests.cpp"
		"Tests/PlayerRuleTests.cpp"
		"Tests/Tests.h"
	)

	SET(TF2BD_ENABLE_CLI_EXE true)

	add_test(NAME TF2BD_Tests COMMAND tf2_bot_detector_cli --run-tests
		WORKING_DIRECTORY staging
	)
endif()

if(TF2BD_ENABLE_CLI_EXE)
	add_executable(tf2_bot_detector_cli "Launcher/main.cpp")
	target_include_directories(tf2_bot_detector_cli PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
	target_link_libraries(tf2_bot_detector_cli PRIVATE tf2_bot_detector)
	target_compile_features(tf2_bot_detector_cli PUBLIC cxx_std_17)
endif()
