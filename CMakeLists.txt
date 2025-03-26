cmake_minimum_required(VERSION 3.15)
project(MyCLIApp VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add FTXUI (Terminal UI library)
include(FetchContent)
FetchContent_Declare(
    ftxui
    GIT_REPOSITORY https://github.com/ArthurSonzogni/ftxui
    GIT_TAG v6.0.0
)
FetchContent_MakeAvailable(ftxui)

# Find required packages
find_package(CURL REQUIRED)

# Add executable and link libraries
add_executable(llm main.cpp)
target_link_libraries(llm PRIVATE ${CURL_LIBRARIES}
    ftxui::component ftxui::dom ftxui::screen)

# Installation setup
install(TARGETS llm
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Optional: Add tests
enable_testing()
add_test(NAME BasicTest COMMAND llm)
