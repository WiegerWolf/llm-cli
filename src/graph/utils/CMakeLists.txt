add_library(graph_utils OBJECT)

target_include_directories(graph_utils
    PUBLIC  ${PROJECT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

# target_link_libraries(graph_utils PRIVATE ...)

# Placeholder for Phase-0: compile shared dummy until real utils sources exist
target_sources(graph_utils PRIVATE ${PROJECT_SOURCE_DIR}/src/dummy.cpp)
