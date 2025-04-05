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
)
FetchContent_MakeAvailable(nlohmann_json)

# Find required packages
find_package(CURL REQUIRED)
find_package(SQLite3 QUIET)
find_package(PkgConfig QUIET)

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

# Add executable and link libraries
add_executable(llm 
    main.cpp
    database.cpp
    database.h
)
target_link_libraries(llm PRIVATE 
    ${CURL_LIBRARIES} 
    nlohmann_json::nlohmann_json
    ${Readline_LIBRARIES}
    ${SQLite3_LIBRARIES}
    ${GUMBO_LIBRARIES}
)

target_include_directories(llm PRIVATE ${GUMBO_INCLUDE_DIRS})

# Installation setup
install(TARGETS llm
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Optional: Add tests
enable_testing()
add_test(NAME BasicTest COMMAND llm)
