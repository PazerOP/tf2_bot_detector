project(tf2_bot_detector_updater)

add_executable(tf2_bot_detector_updater
	"main.cpp"
	"Common.h"
)

if (WIN32)
	target_sources(tf2_bot_detector_updater PRIVATE
		"Update_MSIX.cpp"
		"Update_MSIX.h"
	)
endif()

set_target_properties(tf2_bot_detector_updater PROPERTIES
	CXX_STANDARD 17
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${PROJECT_NAME}"
	RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/${PROJECT_NAME}"
	RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/${PROJECT_NAME}"
)

find_package(fmt CONFIG REQUIRED)

target_link_libraries(tf2_bot_detector_updater PRIVATE
	tf2_bot_detector_common
	mh_stuff
	fmt::fmt
)
