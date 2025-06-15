add_library(utils OBJECT)

target_include_directories(utils
    PUBLIC  ${PROJECT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

# target_link_libraries(utils PRIVATE ...)

# Temporary placeholder so empty OBJECT lib builds
target_sources(utils PRIVATE ${PROJECT_SOURCE_DIR}/src/dummy.cpp)
