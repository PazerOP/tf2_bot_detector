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
