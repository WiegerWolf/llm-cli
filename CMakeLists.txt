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
find_package(SQLite3 REQUIRED)

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
add_executable(llm main.cpp)
target_link_libraries(llm PRIVATE 
    ${CURL_LIBRARIES} 
    nlohmann_json::nlohmann_json
    ${Readline_LIBRARIES}
    SQLite::SQLite3
)

# Installation setup
install(TARGETS llm
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Optional: Add tests
enable_testing()
add_test(NAME BasicTest COMMAND llm)
