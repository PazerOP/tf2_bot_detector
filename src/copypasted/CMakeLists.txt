set(files "${CMAKE_CURRENT_LIST_DIR}/CSignature.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Netvar.cpp")
target_sources(cathook PRIVATE ${files})
list(REMOVE_ITEM ignore_files ${files})
set(ignore_files ${ignore_files} CACHE INTERNAL "")
