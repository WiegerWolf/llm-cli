add_library(graph_layout OBJECT)

target_include_directories(graph_layout
    PUBLIC  ${PROJECT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(graph_layout PRIVATE core)

target_sources(graph_layout PRIVATE
    graph_layout.cpp
    force_directed_layout.cpp
    graph_layout_recursive.cpp
    spatial_hash.cpp
)
