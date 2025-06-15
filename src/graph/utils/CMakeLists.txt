add_library(graph_utils OBJECT)

target_include_directories(graph_utils
    PUBLIC  ${PROJECT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

# target_link_libraries(graph_utils PRIVATE ...)
