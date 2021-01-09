cmake_minimum_required(VERSION 3.17)

set(VCPKG_CRT_LINKAGE static)
set(VCPKG_LIBRARY_LINKAGE static)

include(../cmake/init-preproject.cmake)
	project(tf2_bot_detector_updater)
include(../cmake/init-postproject.cmake)

add_executable(tf2_bot_detector_updater
	"Platform/Platform.h"
	"Common.h"
	"main.cpp"
	"Update_Portable.cpp"
	"Update_Portable.h"
)

target_include_directories(tf2_bot_detector_updater PRIVATE .)

if (WIN32)
	configure_file(Resources.base.rc Resources.rc)
	target_sources(tf2_bot_detector_updater PRIVATE
		"Platform/Windows/Platform_Windows.cpp"
		"Resources.rc"
		"Update_MSIX.cpp"
		"Update_MSIX.h"
	)

	add_subdirectory(../tf2_bot_detector_winrt "${PROJECT_BINARY_DIR}/tf2_bot_detector_winrt")
	target_link_libraries(tf2_bot_detector_updater PUBLIC
		tf2_bot_detector::winrt
	)
endif()

target_compile_features(tf2_bot_detector_updater PUBLIC cxx_std_20)

set_property(GLOBAL PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

message("CMAKE_VS_PLATFORM_NAME = ${CMAKE_VS_PLATFORM_NAME}")
message("CMAKE_VS_PLATFORM_NAME_DEFAULT = ${CMAKE_VS_PLATFORM_NAME_DEFAULT}")
message("CMAKE_SYSTEM_PROCESSOR = ${CMAKE_SYSTEM_PROCESSOR}")
message("CMAKE_GENERATOR_PLATFORM = ${CMAKE_GENERATOR_PLATFORM}")
message("VCPKG_TARGET_TRIPLET = ${VCPKG_TARGET_TRIPLET}")
message("MSVC_RUNTIME_LIBRARY = ${MSVC_RUNTIME_LIBRARY}")

find_package(fmt CONFIG REQUIRED)
add_subdirectory(../submodules/mh_stuff "${PROJECT_BINARY_DIR}/submodules/mh_stuff")
add_subdirectory(../tf2_bot_detector_common "${PROJECT_BINARY_DIR}/tf2_bot_detector_common")

find_package(OpenSSL REQUIRED)

target_link_libraries(tf2_bot_detector_updater PUBLIC
	tf2_bot_detector::common
	mh::stuff
	fmt::fmt
	OpenSSL::SSL # cpp-httplib requires openssl
)

find_path(HTTPLIB_PATH NAMES httplib.h)
target_include_directories(tf2_bot_detector_updater PRIVATE ${HTTPLIB_PATH})
