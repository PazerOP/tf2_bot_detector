cmake_minimum_required(VERSION 3.17.2)
project(tf2_bot_detector VERSION 1.0.0.22)

set(JSON_BuildTests OFF)

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

# set(VCPKG_APPLOCAL_DEPS ON)

add_subdirectory(submodules/imgui_desktop)
add_subdirectory(submodules/mh_stuff)
# add_subdirectory(submodules/nlohmann_json)
add_subdirectory(submodules/ValveFileVDF)

add_executable(tf2_bot_detector WIN32
	"tf2_bot_detector/Config/ConfigHelpers.cpp"
	"tf2_bot_detector/Config/ConfigHelpers.h"
	"tf2_bot_detector/Config/JSONHelpers.h"
	"tf2_bot_detector/Config/PlayerListJSON.cpp"
	"tf2_bot_detector/Config/PlayerListJSON.h"
	"tf2_bot_detector/Config/Rules.cpp"
	"tf2_bot_detector/Config/Rules.h"
	"tf2_bot_detector/Config/Settings.cpp"
	"tf2_bot_detector/Config/Settings.h"
	"tf2_bot_detector/Config/Whitelist.cpp"
	"tf2_bot_detector/Config/Whitelist.h"
	"tf2_bot_detector/Networking/GithubAPI.h"
	"tf2_bot_detector/Networking/GithubAPI.cpp"
	"tf2_bot_detector/Networking/HTTPClient.h"
	"tf2_bot_detector/Networking/HTTPClient.cpp"
	"tf2_bot_detector/Networking/HTTPHelpers.h"
	"tf2_bot_detector/Networking/HTTPHelpers.cpp"
	"tf2_bot_detector/Networking/SteamAPI.h"
	"tf2_bot_detector/Networking/SteamAPI.cpp"
	"tf2_bot_detector/PlatformSpecific/Shell.h"
	"tf2_bot_detector/PlatformSpecific/Shitty.h"
	"tf2_bot_detector/PlatformSpecific/Steam.h"
	"tf2_bot_detector/ActionManager.cpp"
	"tf2_bot_detector/ActionManager.h"
	"tf2_bot_detector/Actions.cpp"
	"tf2_bot_detector/Actions.h"
	"tf2_bot_detector/AsyncObject.h"
	"tf2_bot_detector/Clock.cpp"
	"tf2_bot_detector/Clock.h"
	"tf2_bot_detector/CompensatedTS.cpp"
	"tf2_bot_detector/CompensatedTS.h"
	"tf2_bot_detector/ConsoleLines.cpp"
	"tf2_bot_detector/ConsoleLines.h"
	"tf2_bot_detector/FileMods.cpp"
	"tf2_bot_detector/FileMods.h"
	"tf2_bot_detector/ICommandSource.h"
	"tf2_bot_detector/IConsoleLine.h"
	"tf2_bot_detector/IConsoleLineListener.h"
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
	"tf2_bot_detector/NetworkStatus.cpp"
	"tf2_bot_detector/NetworkStatus.h"
	"tf2_bot_detector/PathUtils.cpp"
	"tf2_bot_detector/PathUtils.h"
	"tf2_bot_detector/PeriodicActions.cpp"
	"tf2_bot_detector/PeriodicActions.h"
	"tf2_bot_detector/PlayerStatus.h"
	"tf2_bot_detector/SetupFlow.cpp"
	"tf2_bot_detector/SetupFlow.h"
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
		tf2_bot_detector/PlatformSpecific/Windows/CrashHandler.cpp
		tf2_bot_detector/PlatformSpecific/Windows/Shell.cpp
		tf2_bot_detector/PlatformSpecific/Windows/Shitty.cpp
		tf2_bot_detector/PlatformSpecific/Windows/Steam.cpp
	)
endif()

target_include_directories(tf2_bot_detector
	PRIVATE tf2_bot_detector
	PRIVATE submodules/nlohmann_json/include
)

# cpp-httplib requires openssl
find_package(OpenSSL REQUIRED)
target_link_libraries(tf2_bot_detector PRIVATE OpenSSL::SSL)

find_path(HTTPLIB_PATH NAMES httplib.h)
target_include_directories(tf2_bot_detector PRIVATE ${HTTPLIB_PATH})

find_package(libzippp CONFIG REQUIRED)
find_package(cppcoro CONFIG REQUIRED)

target_link_libraries(tf2_bot_detector PRIVATE
	imgui_desktop
	mh_stuff
	ValveFileVDF
	libzip::libzip
	libzippp::libzippp
	cppcoro
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

file(GLOB_RECURSE TF2BD_EXTRA_FILES RELATIVE "${CMAKE_SOURCE_DIR}/staging" "staging/cfg/*" "staging/licenses/*")
list(REMOVE_ITEM TF2BD_EXTRA_FILES "cfg/settings.json")
foreach(EXTRA_FILE ${TF2BD_EXTRA_FILES})
	configure_file("staging/${EXTRA_FILE}" "${CMAKE_BINARY_DIR}/${EXTRA_FILE}")
endforeach()

add_custom_command(
	OUTPUT tf2_bot_detector.zip
	DEPENDS tf2_bot_detector
	COMMAND ${CMAKE_COMMAND} -E tar "cfv" "tf2_bot_detector_${PROJECT_VERSION}.zip" --format=zip $<TARGET_FILE_NAME:tf2_bot_detector> ${TF2BD_EXTRA_FILES} ${TF2BD_INSTALL_DEPS_DLL}
)

if (WIN32)
	add_custom_command(
		OUTPUT symbols.zip
		DEPENDS tf2_bot_detector
		COMMAND ${CMAKE_COMMAND} -E tar "cfv" "symbols_${PROJECT_VERSION}.zip" --format=zip $<TARGET_PDB_FILE:tf2_bot_detector>
	)
	add_custom_target(tf2_bot_detector_zips ALL DEPENDS tf2_bot_detector.zip symbols.zip)
else()
	add_custom_target(tf2_bot_detector_zips ALL DEPENDS tf2_bot_detector.zip)
endif()
