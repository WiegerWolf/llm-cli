add_library(db_drivers OBJECT)

target_include_directories(db_drivers
    PUBLIC  ${PROJECT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

# target_link_libraries(db_drivers PRIVATE ...)
