add_library(gui_render OBJECT)

target_include_directories(gui_render
    PUBLIC  ${PROJECT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link libraries as needed later
# target_link_libraries(gui_render PRIVATE ...)

# -------------------------------------------------------------------
# Temporary: add placeholder source so the empty OBJECT library builds
# This dummy file will be removed once real sources are migrated here.
# -------------------------------------------------------------------
target_sources(gui_render PRIVATE ${PROJECT_SOURCE_DIR}/src/dummy.cpp)
