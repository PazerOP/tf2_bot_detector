cmake_minimum_required(VERSION 3.17.2)
cmake_policy(SET CMP0091 NEW) # Enable [CMAKE_]MSVC_RUNTIME_LIBRARY
project(tf2_bot_detector VERSION 1.1.0)

include(ExternalProject)

message("TF2BD CMAKE_BUILD_TYPE = ${CMAKE_BUILD_TYPE}")

# We use this as the build number.
message("TF2BD CMAKE_PROJECT_VERSION_TWEAK = ${CMAKE_PROJECT_VERSION_TWEAK}")
if ("${CMAKE_PROJECT_VERSION_TWEAK}" STREQUAL "")
	set(CMAKE_PROJECT_VERSION_TWEAK 0)
endif()

option(TF2BD_IS_CI_COMPILE "Set to true if this is a compile on a CI service. Used to help determine if user has made modifications to the source code." off)
if (TF2BD_IS_CI_COMPILE)
	add_compile_definitions(TF2BD_IS_CI_COMPILE=1)
else()
	add_compile_definitions(TF2BD_IS_CI_COMPILE=0)
endif()

if (VCPKG_CRT_LINKAGE MATCHES "static")
	set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

set_property(GLOBAL PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE true)

if (MSVC)
	message("CMAKE_VS_PLATFORM_NAME = ${CMAKE_VS_PLATFORM_NAME}")
	message("CMAKE_VS_PLATFORM_NAME_DEFAULT = ${CMAKE_VS_PLATFORM_NAME_DEFAULT}")
	message("CMAKE_SYSTEM_PROCESSOR = ${CMAKE_SYSTEM_PROCESSOR}")
	message("CMAKE_GENERATOR_PLATFORM = ${CMAKE_GENERATOR_PLATFORM}")
	message("VCPKG_TARGET_TRIPLET = ${VCPKG_TARGET_TRIPLET}")

	# Enable inlining of functions marked "inline" even in debug builds
	string(REPLACE "/Ob0" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /Ob1")

	# Improve build performance when running without ninja
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")

	# Generate PDBs for release builds - RelWithDebInfo is NOT a Release build!
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Zi")
	set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /DEBUG")
	set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /DEBUG")

	if (CMAKE_BUILD_TYPE MATCHES "Release")
		add_link_options(/OPT:REF /OPT:ICF)
	endif()

	add_definitions(/await)
endif()

add_subdirectory(submodules/ValveFileVDF)
add_subdirectory(submodules/SourceRCON)
add_subdirectory(submodules/imgui_desktop)
add_subdirectory(submodules/mh_stuff)
add_subdirectory(tf2_bot_detector_common)
# add_subdirectory(tf2_bot_detector_updater)
add_subdirectory(tf2_bot_detector)

set(ENABLE_EXAMPLES off CACHE BOOL "Build examples" FORCE)
add_subdirectory("submodules/cppcoro")

option(TF2BD_ENABLE_DISCORD_INTEGRATION "Enable discord integration" on)
option(TF2BD_ENABLE_TESTS "Enable test compilation" off)

# TODO: Find a way to do this locally
if(MSVC)
	target_compile_options(tf2_bot_detector PRIVATE /WX)
endif()

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
