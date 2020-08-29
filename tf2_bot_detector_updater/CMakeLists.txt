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

set_property(TARGET tf2_bot_detector_updater PROPERTY CXX_STANDARD 17)

find_package(fmt CONFIG REQUIRED)

target_link_libraries(tf2_bot_detector_updater PRIVATE
	tf2_bot_detector_common
	mh_stuff
	fmt::fmt
)
