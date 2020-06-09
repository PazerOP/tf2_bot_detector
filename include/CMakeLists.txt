target_include_directories(cathook PRIVATE "${CMAKE_CURRENT_LIST_DIR}")

add_subdirectory(core)
add_subdirectory(classinfo)
add_subdirectory(copypasted)
add_subdirectory(hacks)
add_subdirectory(hooks)
add_subdirectory(reclasses)
add_subdirectory(sdk)
add_subdirectory(settings)

if(EnableVisuals)
    add_subdirectory(visual)
endif()

