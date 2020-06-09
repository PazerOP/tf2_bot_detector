set(files "${CMAKE_CURRENT_LIST_DIR}/aimbot.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/antiaim.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/bhop.cpp")
target_sources(cathook PRIVATE ${files})
list(REMOVE_ITEM ignore_files ${files})
set(ignore_files ${ignore_files} CACHE INTERNAL "")
