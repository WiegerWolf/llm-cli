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
    CMAKE_ARGS
      -DFTXUI_BUILD_EXAMPLES=OFF -DFTXUI_BUILD_DOCS=OFF -DFTXUI_ENABLE_INSTALL=OFF
)
FetchContent_MakeAvailable(ftxui)

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

# Add executable and link libraries
add_executable(llm main.cpp)
target_link_libraries(llm PRIVATE ${CURL_LIBRARIES} 
    ftxui::component ftxui::dom ftxui::screen nlohmann_json::nlohmann_json)

# Installation setup
install(TARGETS llm
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Optional: Add tests
enable_testing()
add_test(NAME BasicTest COMMAND llm)
