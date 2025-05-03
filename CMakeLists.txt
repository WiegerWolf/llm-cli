cmake_minimum_required(VERSION 3.15)
project(MyCLIApp VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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

# Fetch ImGui using FetchContent
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.90.8 # Use a specific stable tag
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(imgui)

# Create an interface library for ImGui
add_library(imgui_lib INTERFACE)
target_sources(imgui_lib INTERFACE
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui_lib INTERFACE
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)
# Link ImGui library to GLFW and OpenGL (needed by backends)
target_link_libraries(imgui_lib INTERFACE glfw ${OpenGL_LIBRARIES})


# Find required packages
find_package(CURL REQUIRED)
find_package(SQLite3 QUIET)
find_package(PkgConfig QUIET)
find_package(OpenGL REQUIRED) # Find OpenGL
find_package(glfw3 REQUIRED)  # Find GLFW3

# Find Gumbo HTML parser
if(PKG_CONFIG_FOUND)
    pkg_check_modules(GUMBO gumbo)
endif()

if(NOT GUMBO_FOUND)
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


# --- GUI Executable ---
add_executable(llm-gui
    main_gui.cpp
    gui_interface/gui_interface.cpp
    gui_interface/gui_interface.h
)

target_link_libraries(llm-gui PRIVATE
    llm_core            # Link against the core library
    imgui_lib           # Link against our ImGui interface library
    glfw                # Link against GLFW
    OpenGL::GL          # Link against OpenGL
    Threads::Threads    # Link against Threads
)

# Include directories needed by the GUI executable
target_include_directories(llm-gui PRIVATE
    ${PROJECT_SOURCE_DIR} # For project headers like ui_interface.h
    # Add other necessary include paths if needed
)


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

# Installation setup
install(TARGETS llm-cli llm-gui # Install both executables
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Optional: Add tests
enable_testing()
# Update test command if needed, or keep using the installed name 'llm-cli'
# For now, assuming the test runs the built executable directly
add_test(NAME BasicTest COMMAND llm-cli)
