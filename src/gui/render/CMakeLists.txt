add_library(gui_render OBJECT)

target_include_directories(gui_render
    PUBLIC  ${PROJECT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

# target_link_libraries(gui_render PRIVATE ...)
