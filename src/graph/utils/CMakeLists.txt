add_library(graph_utils OBJECT)

target_include_directories(graph_utils
    PUBLIC  ${PROJECT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(graph_utils PRIVATE core)

target_sources(graph_utils PRIVATE
    graph_drawing_utils.cpp
    graph_manager.cpp
)
