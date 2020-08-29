cmake_minimum_required(VERSION 3.17.2)
project(tf2_bot_detector VERSION 1.1.0)

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

# Generate PDBs for release builds - RelWithDebInfo is NOT a Release build!
if (MSVC)
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Zi")
	set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /DEBUG")
	set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /DEBUG")
endif()

set_property(GLOBAL PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE true)
if (MSVC AND (CMAKE_BUILD_TYPE MATCHES "Release"))
	add_link_options(/OPT:REF /OPT:ICF)
endif()

add_subdirectory(submodules/ValveFileVDF)
add_subdirectory(submodules/SourceRCON)
add_subdirectory(submodules/imgui_desktop)
add_subdirectory(submodules/mh_stuff)
add_subdirectory(tf2_bot_detector_common)
add_subdirectory(tf2_bot_detector_updater)
add_subdirectory(tf2_bot_detector)

set(ENABLE_EXAMPLES off CACHE BOOL "Build examples" FORCE)
add_subdirectory("submodules/cppcoro")

option(TF2BD_ENABLE_DISCORD_INTEGRATION "Enable discord integration" on)
option(TF2BD_ENABLE_TESTS "Enable test compilation" off)

# TODO: Find a way to do this locally
if(MSVC)
	target_compile_options(tf2_bot_detector PRIVATE /WX)
endif()

if (MSVC)
	add_definitions(/await)
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
