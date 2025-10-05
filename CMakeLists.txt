cmake_minimum_required(VERSION 3.15)
project(MyCLIApp VERSION 1.0.0 LANGUAGES C CXX)

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

# Add Gumbo HTML parser library
FetchContent_Declare(
    gumbo
    GIT_REPOSITORY https://github.com/google/gumbo-parser.git
    GIT_TAG master
)
FetchContent_MakeAvailable(gumbo)

# Build gumbo library
add_library(gumbo STATIC
    ${gumbo_SOURCE_DIR}/src/attribute.c
    ${gumbo_SOURCE_DIR}/src/char_ref.c
    ${gumbo_SOURCE_DIR}/src/error.c
    ${gumbo_SOURCE_DIR}/src/parser.c
    ${gumbo_SOURCE_DIR}/src/string_buffer.c
    ${gumbo_SOURCE_DIR}/src/string_piece.c
    ${gumbo_SOURCE_DIR}/src/tag.c
    ${gumbo_SOURCE_DIR}/src/tokenizer.c
    ${gumbo_SOURCE_DIR}/src/utf8.c
    ${gumbo_SOURCE_DIR}/src/util.c
    ${gumbo_SOURCE_DIR}/src/vector.c
)
set_target_properties(gumbo PROPERTIES LINKER_LANGUAGE C)
target_include_directories(gumbo PUBLIC ${gumbo_SOURCE_DIR}/src)

# Find required packages
find_package(CURL REQUIRED)
find_package(SQLite3 REQUIRED)

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
    # Database module (new modular structure)
    database/database_core.cpp
    database/database_core.h
    database/message_repository.cpp
    database/message_repository.h
    database/model_repository.cpp
    database/model_repository.h
    # Utility modules
    tools.cpp
    tools.h
    model_manager.cpp
    model_manager.h
    api_client.cpp
    api_client.h
    tool_executor.cpp
    tool_executor.h
    command_handler.cpp
    command_handler.h
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
    SQLite::SQLite3
    gumbo
    Threads::Threads
)

target_include_directories(llm_core PUBLIC
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
install(TARGETS llm-cli
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
