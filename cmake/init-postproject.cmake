include_guard(GLOBAL)

message("CMAKE_CURRENT_LIST_DIR = ${CMAKE_CURRENT_LIST_DIR}")
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR})

include(warning_level)

message("TF2BD CMAKE_BUILD_TYPE = ${CMAKE_BUILD_TYPE}")

if ("${TF2BD_VERSION_BUILD}" STREQUAL "")
	set(TF2BD_VERSION_BUILD 0)
endif()

set(TF2BD_VERSION_NOBUILD "${TF2BD_VERSION_MAJOR}.${TF2BD_VERSION_MINOR}.${TF2BD_VERSION_PATCH}" CACHE STRING "TF2BD version without the build number" FORCE)
set(TF2BD_VERSION "${TF2BD_VERSION_NOBUILD}.${TF2BD_VERSION_BUILD}" CACHE STRING "Full TF2BD version string" FORCE)

# Prevent CMAKE_PROJECT_VERSION from getting out of sync with the individual components
set(CMAKE_PROJECT_VERSION_MAJOR ${TF2BD_VERSION_MAJOR} CACHE STRING "Loaded from TF2BD_VERSION_MAJOR." FORCE)
set(CMAKE_PROJECT_VERSION_MINOR ${TF2BD_VERSION_MINOR} CACHE STRING "Loaded from TF2BD_VERSION_MINOR." FORCE)
set(CMAKE_PROJECT_VERSION_PATCH ${TF2BD_VERSION_PATCH} CACHE STRING "Loaded from TF2BD_VERSION_PATCH." FORCE)
set(CMAKE_PROJECT_VERSION_TWEAK ${TF2BD_VERSION_BUILD} CACHE STRING "Loaded from TF2BD_VERSION_BUILD." FORCE)

option(TF2BD_IS_CI_COMPILE "Set to true if this is a compile on a CI service. Used to help determine if user has made modifications to the source code." off)
if (TF2BD_IS_CI_COMPILE)
	add_compile_definitions(TF2BD_IS_CI_COMPILE=1)
else()
	add_compile_definitions(TF2BD_IS_CI_COMPILE=0)
	set(CMAKE_PROJECT_VERSION_TWEAK "65535" CACHE STRING "Loaded from TF2BD_VERSION_BUILD." FORCE)
endif()

set(CMAKE_PROJECT_VERSION "${CMAKE_PROJECT_VERSION_MAJOR}.${CMAKE_PROJECT_VERSION_MINOR}.${CMAKE_PROJECT_VERSION_PATCH}.${CMAKE_PROJECT_VERSION_TWEAK}" CACHE STRING "Loaded from TF2BD_VERSION." FORCE)
message("TF2BD CMAKE_PROJECT_VERSION = ${CMAKE_PROJECT_VERSION}")

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

	if ((CMAKE_BUILD_TYPE MATCHES "Release"))
		set(TF2BD_RESOURCE_FILEFLAGS "0")
	else()
		set(TF2BD_RESOURCE_FILEFLAGS "VS_FF_DEBUG")
	endif()

	# TODO: Find a way to do this locally
	add_compile_options(
		/WX
		/experimental:external   # enable /external command line switches, see https://devblogs.microsoft.com/cppblog/broken-warnings-theory/
		/external:W1             # Warning level 1 on external headers
		/external:anglebrackets  # Consider anything with #include <something> to be "external"

		# /w34061 # enumerator 'identifier' in a switch of enum 'enumeration' is not explicitly handled by a case label
		/w34062 # enumerator 'identifier' in a switch of enum 'enumeration' is not handled
		/w44103 # 'filename' : alignment changed after including header, may be due to missing #pragma pack(pop)
		        # Windows headers trigger this
	)
endif()
