set(files "${CMAKE_CURRENT_LIST_DIR}/imgui_impl.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/imgui.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/imgui_draw.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/imgui_freetype.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/imgui_widgets.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/imgui_demo.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/imrenderer.cpp")
target_sources(cathook PRIVATE ${files})
list(REMOVE_ITEM ignore_files ${files})
set(ignore_files ${ignore_files} CACHE INTERNAL "")
