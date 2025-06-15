add_library(gui_views OBJECT)

target_include_directories(gui_views
    PUBLIC  ${PROJECT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

# target_link_libraries(gui_views PRIVATE ...)

# Temporary placeholder until real GUI view sources are migrated
target_sources(gui_views PRIVATE ${PROJECT_SOURCE_DIR}/src/dummy.cpp)
