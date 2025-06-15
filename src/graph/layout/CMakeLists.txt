add_library(graph_layout OBJECT)

target_include_directories(graph_layout
    PUBLIC  ${PROJECT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

# target_link_libraries(graph_layout PRIVATE ...)

# Temporary placeholder to satisfy CMake until real layout sources arrive
target_sources(graph_layout PRIVATE ${PROJECT_SOURCE_DIR}/src/dummy.cpp)
