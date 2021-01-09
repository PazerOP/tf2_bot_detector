cmake_minimum_required(VERSION 3.17)

include(../cmake/init-preproject.cmake)
	project(tf2_bot_detector_winrt)
include(../cmake/init-postproject.cmake)

include(GenerateExportHeader)

if (VCPKG_LIBRARY_LINKAGE MATCHES static)
	set(TF2BD_WINRT_LIBRARY_LINKAGE STATIC)
else()
	set(TF2BD_WINRT_LIBRARY_LINKAGE MODULE)
endif()

message("TF2BD_WINRT_LIBRARY_LINKAGE = ${TF2BD_WINRT_LIBRARY_LINKAGE}")
add_library(tf2_bot_detector_winrt ${TF2BD_WINRT_LIBRARY_LINKAGE}
	"tf2_bot_detector_winrt.cpp"
	"tf2_bot_detector_winrt.h"
)

generate_export_header(tf2_bot_detector_winrt
	EXPORT_FILE_NAME "include/tf2_bot_detector_winrt_export.h"
)
target_include_directories(tf2_bot_detector_winrt PUBLIC "${CMAKE_CURRENT_BINARY_DIR}/include/")

execute_process(
	COMMAND_ECHO STDOUT
	RESULT_VARIABLE EXECUTE_PROCESS_RESULT
	COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/setup_winrt.bat
	WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

if (NOT EXECUTE_PROCESS_RESULT EQUAL 0)
	message(FATAL_ERROR "Failed to run setup_winrt.bat (exit code ${EXECUTE_PROCESS_RESULT})")
endif()

file(READ "${CMAKE_CURRENT_BINARY_DIR}/nuget/winrt_include_dir.txt" WINRT_INCLUDE_DIR)
string(STRIP "${WINRT_INCLUDE_DIR}" WINRT_INCLUDE_DIR)
target_include_directories(tf2_bot_detector_winrt SYSTEM BEFORE PUBLIC ${WINRT_INCLUDE_DIR})

target_include_directories(tf2_bot_detector_winrt PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

find_package(fmt CONFIG REQUIRED)

target_link_libraries(tf2_bot_detector_winrt PUBLIC
	tf2_bot_detector::common
	mh::stuff
	fmt::fmt
)

function(Include_TF2BD_WinRT _target)

	# Add include directories
	get_target_property(TF2BD_WINRT_INCLUDE_DIRECTORIES tf2_bot_detector_winrt INCLUDE_DIRECTORIES)
	target_include_directories(${_target} PUBLIC ${TF2BD_WINRT_INCLUDE_DIRECTORIES})

	# Copy DLL
	add_custom_command(
		TARGET tf2_bot_detector POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:tf2_bot_detector_winrt> $<TARGET_FILE_DIR:${_target}>
	)

	# Copy PDB
	add_custom_command(
		TARGET tf2_bot_detector POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_PDB_FILE:tf2_bot_detector_winrt> $<TARGET_FILE_DIR:${_target}>
	)

endfunction()
