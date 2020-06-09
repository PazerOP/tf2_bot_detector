file(GLOB_RECURSE ModuleSources ${CMAKE_CURRENT_LIST_DIR} *.cpp)
target_sources(cathook PRIVATE ${ModuleSources})
