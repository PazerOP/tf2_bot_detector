
add_library(tf2_bot_detector_common INTERFACE)

target_sources(tf2_bot_detector_common INTERFACE
	"include/ReleaseChannel.h"
)

target_include_directories(tf2_bot_detector_common INTERFACE include)
