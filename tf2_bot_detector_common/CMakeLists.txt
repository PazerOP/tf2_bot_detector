cmake_minimum_required(VERSION 3.17)
project(tf2_bot_detector_common)

add_library(tf2_bot_detector_common INTERFACE)
add_library(tf2_bot_detector::common ALIAS tf2_bot_detector_common)

target_sources(tf2_bot_detector_common INTERFACE
	"include/ReleaseChannel.h"
)

target_include_directories(tf2_bot_detector_common INTERFACE "include")

target_link_libraries(tf2_bot_detector_common
	INTERFACE
		mh::stuff
)
