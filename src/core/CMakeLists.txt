# Core domain object library
add_library(core OBJECT
    event_dispatch.cpp
    main_gui_core.cpp
)

target_include_directories(core
    PUBLIC  ${PROJECT_SOURCE_DIR}/include
    PUBLIC  ${PROJECT_SOURCE_DIR}         # allow #include "gui_interface/..."
    PUBLIC  ${PROJECT_SOURCE_DIR}/extern/imgui # For imgui.h
    PUBLIC  ${CMAKE_BINARY_DIR} # For generated config.h
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(core PUBLIC nlohmann_json::nlohmann_json)

# Install all public headers from include/core
install(DIRECTORY ${PROJECT_SOURCE_DIR}/include/core/
        DESTINATION include/core
        FILES_MATCHING PATTERN "*.h")
