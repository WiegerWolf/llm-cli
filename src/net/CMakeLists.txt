add_library(net OBJECT)

target_include_directories(net
    PUBLIC  ${PROJECT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

# target_link_libraries(net PRIVATE ...)

# Temporary placeholder until networking sources are migrated
target_sources(net PRIVATE ${PROJECT_SOURCE_DIR}/src/dummy.cpp)
