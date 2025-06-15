add_library(graph_render OBJECT)

target_include_directories(graph_render
    PUBLIC  ${PROJECT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(graph_render PRIVATE core graph_utils graph_layout)

target_sources(graph_render PRIVATE
    graph_renderer.cpp
    graph_renderer_core.cpp
    camera_utils.cpp
)
