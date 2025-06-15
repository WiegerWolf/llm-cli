add_library(cli OBJECT)

target_include_directories(cli
    PUBLIC  ${PROJECT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

# target_link_libraries(cli PRIVATE ...)

# Placeholder until CLI sources are migrated (Phase-5)
target_sources(cli PRIVATE ${PROJECT_SOURCE_DIR}/src/dummy.cpp)
