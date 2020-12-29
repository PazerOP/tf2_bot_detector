cmake_minimum_required(VERSION 3.17)
project(tf2_bot_detector_common)

add_library(tf2_bot_detector_common STATIC)
add_library(tf2_bot_detector::common ALIAS tf2_bot_detector_common)

target_sources(tf2_bot_detector_common PRIVATE
	"include/ReleaseChannel.h"
	"src/Placeholder.cpp"
)

target_include_directories(tf2_bot_detector_common PUBLIC "include")

target_link_libraries(tf2_bot_detector_common
	PUBLIC
		mh::stuff
)
