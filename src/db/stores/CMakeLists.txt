add_library(db_stores OBJECT)

target_include_directories(db_stores
    PUBLIC  ${PROJECT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

# target_link_libraries(db_stores PRIVATE ...)
