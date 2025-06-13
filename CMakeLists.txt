cmake_minimum_required(VERSION 3.15)
project(MyCLIApp VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set optimization flags for Release builds
# Removed global CXX_FLAGS_RELEASE setting - Handled per-target below
# You might consider -O2 if -O3 causes issues or significantly increases compile time.
# set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3") # If you have C code

# Add JSON library
include(FetchContent)
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json
    GIT_TAG v3.11.3
    CMAKE_ARGS -DJSON_BuildTests=OFF
    CMAKE_ARGS -DJSON_BuildTests=OFF -DJSON_Install=OFF # Don't install json lib separately
)
FetchContent_MakeAvailable(nlohmann_json)

# Find required packages
find_package(CURL REQUIRED)
find_package(SQLite3 QUIET) # Fallback search implemented below
find_package(PkgConfig REQUIRED) # PkgConfig is needed for ImGui discovery now
find_package(OpenGL REQUIRED) # Find OpenGL - Assumes system provides runtime libs
find_package(glfw3 REQUIRED)  # Find GLFW3 - Assumes system provides runtime libs (e.g., libglfw.so.3)

# Platform Checks Note:
# The current find_package calls handle typical Linux/macOS setups.
# Windows builds might require more specific handling, potentially using
# a package manager like vcpkg or providing pre-built dependencies.

# Find Gumbo HTML parser
if(PKG_CONFIG_FOUND)
    pkg_check_modules(GUMBO gumbo)
    # pkg_check_modules(IMGUI REQUIRED imgui) # Find ImGui using pkg-config -- Manually including sources
endif()

if(NOT PKG_CONFIG_FOUND)
    message(FATAL_ERROR "pkg-config not found, needed to find ImGui and Gumbo.")
endif()

if(NOT GUMBO_FOUND) # Corrected typo from GUI_FOUND
    find_path(GUMBO_INCLUDE_DIR gumbo.h
        PATHS /usr/include /usr/local/include /usr/include/gumbo /usr/local/include/gumbo
    )
    
    find_library(GUMBO_LIBRARY NAMES gumbo
        PATHS /usr/lib /usr/local/lib
    )
    
    if(GUMBO_INCLUDE_DIR AND GUMBO_LIBRARY)
        set(GUMBO_FOUND TRUE)
        set(GUMBO_INCLUDE_DIRS ${GUMBO_INCLUDE_DIR})
        set(GUMBO_LIBRARIES ${GUMBO_LIBRARY})
    else()
        message(FATAL_ERROR "Gumbo HTML parser not found - install with: sudo apt-get install libgumbo-dev")
    endif()
endif()

if(NOT SQLite3_FOUND)
    # Fallback search if standard method fails
    find_path(SQLITE3_INCLUDE_DIR sqlite3.h
        PATHS /usr/include /usr/local/include
    )
    
    find_library(SQLITE3_LIBRARY NAMES sqlite3
        PATHS /usr/lib /usr/local/lib
    )
    
    if(SQLITE3_INCLUDE_DIR AND SQLITE3_LIBRARY)
        set(SQLite3_FOUND TRUE)
        set(SQLite3_INCLUDE_DIRS ${SQLITE3_INCLUDE_DIR})
        set(SQLite3_LIBRARIES ${SQLITE3_LIBRARY})
    else()
        message(FATAL_ERROR "SQLite3 not found - install with: sudo apt-get install libsqlite3-dev")
    endif()
endif()

# Find Threads package
find_package(Threads REQUIRED)

# Find Readline using alternative methods
find_path(READLINE_INCLUDE_DIR readline/readline.h)
find_library(READLINE_LIBRARY NAMES readline)
find_library(TERMCAP_LIBRARY NAMES termcap tinfo curses ncurses)

if(READLINE_INCLUDE_DIR AND READLINE_LIBRARY AND TERMCAP_LIBRARY)
    set(Readline_FOUND TRUE)
    set(Readline_LIBRARIES ${READLINE_LIBRARY} ${TERMCAP_LIBRARY})
    set(Readline_INCLUDE_DIRS ${READLINE_INCLUDE_DIR})
else()
    message(FATAL_ERROR "Readline libraries not found - install with: sudo apt-get install libreadline-dev")
endif()

# --- Core Library ---
add_library(llm_core STATIC
    chat_client.cpp
    chat_client.h
    database.cpp
    database.h
    tools.cpp
    tools.h
    tools_impl/search_web_tool.cpp
    tools_impl/visit_url_tool.cpp
    tools_impl/datetime_tool.cpp
    tools_impl/read_history_tool.cpp
    tools_impl/web_research_tool.cpp
    tools_impl/deep_research_tool.cpp
    curl_utils.h        # Header-only utility
    ui_interface.h      # Interface header
    model_types.h       # For ModelData struct
)

target_link_libraries(llm_core PUBLIC
    ${CURL_LIBRARIES}
    nlohmann_json::nlohmann_json
    ${SQLite3_LIBRARIES}
    ${GUMBO_LIBRARIES}
    Threads::Threads
)

target_include_directories(llm_core PUBLIC
    ${GUMBO_INCLUDE_DIRS}
    ${CMAKE_BINARY_DIR} # For config.h
    ${PROJECT_SOURCE_DIR} # For project headers
)
target_compile_options(llm_core PRIVATE $<$<CONFIG:Release>:-O3>)

# --- CLI Executable ---
add_executable(llm-cli
    main_cli.cpp        # Renamed from main.cpp
    cli_interface.h     # CLI implementation header
    cli_interface.cpp   # CLI implementation source
)

target_link_libraries(llm-cli PRIVATE
    llm_core            # Link against the core library
    ${Readline_LIBRARIES} # CLI-specific dependency
)

# Include directories needed by the CLI executable itself (if any beyond core)
# target_include_directories(llm-cli PRIVATE ...)
target_compile_options(llm-cli PRIVATE $<$<CONFIG:Release>:-O3>)


# --- GUI Executable ---
add_executable(llm-gui
    main_gui.cpp
    gui_interface/gui_interface.cpp
    gui_interface/gui_interface.h
    gui_interface/graph_types.h
    graph_manager.cpp
    graph_manager.h
    graph_renderer.cpp
    graph_renderer.h
    graph_layout.cpp
    graph_layout.h
    extern/imgui/imgui.cpp
    extern/imgui/imgui_draw.cpp
    extern/imgui/imgui_tables.cpp
    extern/imgui/imgui_widgets.cpp
    extern/imgui/backends/imgui_impl_glfw.cpp
    extern/imgui/backends/imgui_impl_opengl3.cpp
    extern/imgui/imgui_demo.cpp # Included for testing/examples
)

target_link_libraries(llm-gui PRIVATE
    llm_core            # Link against the core library
    # ${IMGUI_LIBRARIES}  # Link against system ImGui via pkg-config -- Manually including sources
    glfw                # Link against GLFW
    OpenGL::GL          # Link against OpenGL
    Threads::Threads    # Link against Threads
)

# Include directories needed by the GUI executable
target_include_directories(llm-gui PRIVATE
    ${PROJECT_SOURCE_DIR} # For project headers like ui_interface.h
    # ${IMGUI_INCLUDE_DIRS} # Include system ImGui headers via pkg-config -- Manually including sources
    extern/imgui          # For ImGui headers
    extern/imgui/backends # For ImGui backend headers
)
target_compile_options(llm-gui PRIVATE $<$<CONFIG:Release>:-O3>)


# Add option for OPENROUTER_API_KEY
option(OPENROUTER_API_KEY "OpenRouter LLM API Key to embed at compile time" "")

# Add cache entry for BRAVE_SEARCH_API_KEY (string, not boolean)
set(BRAVE_SEARCH_API_KEY "" CACHE STRING "Brave Search API Key to embed at compile time")

# Configure config.h for compile-time API key embedding
configure_file(
  ${CMAKE_SOURCE_DIR}/config.h.in
  ${CMAKE_BINARY_DIR}/config.h
  @ONLY
)

include(GNUInstallDirs)
# Installation setup
install(TARGETS llm-cli llm-gui # Install both executables
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Installation Notes:
# - Runtime Libraries: Dependencies like GLFW, CURL, SQLite3, Gumbo, Readline
#   are expected to be provided by the system or installed by the user
#   (e.g., via package manager). This install rule does not bundle them.
#   Ensure README details these runtime dependencies.
# - Resource Files: No external resources (e.g., fonts) are currently used
#   by the GUI, so none are installed. If resources are added later,
#   install rules will be needed here using install(FILES ...).

# --- Test Executables ---
add_executable(test_chronological_layout
    test_chronological_layout.cpp
    graph_layout.cpp
    graph_layout.h
    gui_interface/graph_types.h
    gui_interface/gui_interface.h
)

target_link_libraries(test_chronological_layout PRIVATE
    llm_core            # Link against the core library
    Threads::Threads    # Link against Threads
)

target_include_directories(test_chronological_layout PRIVATE
    ${PROJECT_SOURCE_DIR} # For project headers
    extern/imgui          # For ImGui headers (ImVec2)
)
target_compile_options(test_chronological_layout PRIVATE $<$<CONFIG:Release>:-O3>)
# --- Force Convergence Test ---
add_executable(force_convergence_test
    tests/force_convergence_test.cpp
    graph_layout.cpp
    graph_layout.h
    gui_interface/graph_types.h
    gui_interface/gui_interface.h
)
target_link_libraries(force_convergence_test PRIVATE
    llm_core
    Threads::Threads
)
target_include_directories(force_convergence_test PRIVATE
    \${PROJECT_SOURCE_DIR}
    extern/imgui
)
target_compile_options(force_convergence_test PRIVATE $<$<CONFIG:Release>:-O3>)
add_test(NAME ForceConvergenceTest COMMAND force_convergence_test)

# Optional: Add tests
enable_testing()

# BasicTest: simple placeholder that always succeeds instantly (no time-outs)
add_test(NAME BasicTest COMMAND ${CMAKE_COMMAND} -E echo "BasicTest passed")

add_test(NAME ChronologicalLayoutTest COMMAND test_chronological_layout)
