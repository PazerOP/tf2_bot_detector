cmake_minimum_required(VERSION 3.17)

option(TF2BD_ENABLE_DISCORD_INTEGRATION "Enable discord integration" on)
option(TF2BD_ENABLE_TESTS "Enable test compilation" off)

include(cmake/init-preproject.cmake)
	project(tf2_bot_detector)
include(cmake/init-postproject.cmake)

set(MH_STUFF_BUILD_SHARED_LIBS ON)
add_subdirectory(submodules/mh_stuff)

set_warning_level(3)
add_subdirectory(submodules/ValveFileVDF)
add_subdirectory(submodules/SourceRCON)
add_subdirectory(submodules/glad2)
add_subdirectory(submodules/imgui_desktop)

add_subdirectory(tf2_bot_detector_common)

if (WIN32)
	add_subdirectory(tf2_bot_detector_winrt)
endif()

# add_subdirectory(tf2_bot_detector_updater)
add_subdirectory(tf2_bot_detector)

