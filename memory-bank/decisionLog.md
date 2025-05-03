# Decision Log

This file records architectural and implementation decisions using a list format.
2025-05-04 01:43:02 - Log of updates made.

*

## Decision
[2025-05-04 01:49:54] - Use C++20 as the primary language standard.
## Rationale
Leverages modern C++ features for better code structure, safety (RAII), and concurrency (`<thread>`, `<stop_token>`).
## Implementation Details
Set via `CMAKE_CXX_STANDARD 20` in `CMakeLists.txt`.

## Decision
[2025-05-04 01:49:54] - Use CMake as the build system generator.
## Rationale
Cross-platform, widely used, good dependency management (FetchContent, find_package).
## Implementation Details
`CMakeLists.txt` defines targets, dependencies, and build options. `build.sh` likely wraps CMake commands.

## Decision
[2025-05-04 01:49:54] - Use SQLite3 for conversation history persistence.
## Rationale
Lightweight, file-based, transactional, suitable for local single-user application history. No server needed.
## Implementation Details
Accessed via `sqlite3` C API encapsulated in `PersistenceManager`. Database file: `~/.llm-cli-chat.db`. WAL mode enabled.

## Decision
[2025-05-04 01:49:54] - Use `libcurl` for HTTP communication.
## Rationale
Mature, robust, cross-platform library for HTTP requests needed for OpenRouter API and web-based tools.
## Implementation Details
Used in `ChatClient` and potentially tool implementations. Linked via CMake `find_package(CURL REQUIRED)`.

## Decision
[2025-05-04 01:49:54] - Use `nlohmann/json` for JSON processing.
## Rationale
Popular, header-only (via FetchContent), easy-to-use C++ JSON library for API interactions and tool arguments/results.
## Implementation Details
Included via CMake `FetchContent`. Used extensively in `ChatClient` and `PersistenceManager` (for tool messages).

## Decision
[2025-05-04 01:49:54] - Use Dear ImGui with GLFW3/OpenGL3 backend for the GUI.
## Rationale
ImGui provides a flexible immediate-mode GUI suitable for developer tools. GLFW handles windowing/input cross-platform. OpenGL provides rendering.
## Implementation Details
Setup in `GuiInterface::initialize()`. Dependencies linked via CMake (`find_package`, `pkg-config`). Noto Sans font embedded.

## Decision
[2025-05-04 01:49:54] - Implement a multi-threaded GUI architecture.
## Rationale
Keeps the GUI responsive by offloading blocking operations (API calls, tool execution) to a worker thread (`ChatClient::run`).
## Implementation Details
Uses `std::thread`, `std::mutex`, `std::condition_variable`, `std::atomic`, `std::queue` for communication between the main GUI thread (`main_gui.cpp`) and the worker thread managed by `GuiInterface`.

## Decision
[2025-05-04 01:49:54] - Support both standard OpenAI `tool_calls` and fallback `<function>` tags.
## Rationale
Provides compatibility with standard tool-using models while offering a fallback for models that might embed calls differently. Increases robustness.
## Implementation Details
Parsing and execution logic implemented within `ChatClient::processTurn` and helper methods (`executeStandardToolCalls`, `executeFallbackFunctionTags`).

*
## Decision

*

## Rationale 

*

## Implementation Details

*