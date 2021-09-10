cmake_minimum_required(VERSION 3.17)

include(../cmake/init-preproject.cmake)
	project(tf2_bot_detector_common)
include(../cmake/init-postproject.cmake)

add_library(tf2_bot_detector_common STATIC)
add_library(tf2_bot_detector::common ALIAS tf2_bot_detector_common)

target_sources(tf2_bot_detector_common PRIVATE
	"include/ReleaseChannel.h"
	"include/Platform/PlatformCommon.h"
)

if (WIN32)
	target_sources(tf2_bot_detector_common PRIVATE
		"src/Platform/Windows/PlatformCommon.cpp"
	)
endif()

target_include_directories(tf2_bot_detector_common PUBLIC "include")

find_package(fmt CONFIG REQUIRED)
find_package(mh-stuff CONFIG REQUIRED)
target_link_libraries(tf2_bot_detector_common
	PUBLIC
		mh::mh-stuff
		fmt::fmt
)
