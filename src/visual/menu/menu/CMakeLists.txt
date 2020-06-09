file(GLOB_RECURSE files ${CMAKE_CURRENT_LIST_DIR}/*.cpp)

target_sources(cathook PRIVATE ${files})
list(REMOVE_ITEM ignore_files ${files})
set(ignore_files ${ignore_files} CACHE INTERNAL "")
