# Product Context

This file provides a high-level overview of the project and the expected product that will be created. Initially it is based upon projectBrief.md (if provided) and all other available project-related information in the working directory. This file is intended to be updated as the project evolves, and should be used to inform all other modes of the project's goals and context.

*

## Project Goal

* Create a C++ chat assistant (CLI and GUI) that connects to OpenRouter LLM APIs and provides advanced web research tools.

## Key Features

*   Chat with LLMs (via OpenRouter API using `libcurl`)
*   Command-Line Interface (`llm-cli` using `libreadline`)
*   Graphical User Interface (`llm-gui` via Dear ImGui + GLFW + OpenGL)
*   Web search (Brave Search, DuckDuckGo, Brave Search API - requires `libcurl`)
*   Visit URLs (content extraction using `libgumbo` and `libcurl`)
*   Web research (multi-step synthesis, potentially using other tools)
*   Deep research (sub-query breakdown, potentially using other tools)
*   Read conversation history (from SQLite database via `PersistenceManager`)
*   Tool call support (OpenAI standard `tool_calls` and custom `<function>` tag fallback parsing)
*   .env support and compile-time embedding for API keys (`config.h`)
*   Multi-threaded (specifically for GUI responsiveness)

## Overall Architecture

*   **Core Logic:** `llm_core` static library containing:
    *   `chat_client.cpp`/`.h`: LLM interaction (OpenRouter via `libcurl`), tool execution logic (standard & fallback), history management.
    *   `database.cpp`/`.h`: SQLite persistence layer (`PersistenceManager` using Pimpl).
    *   `tools.cpp`/`.h` & `tools_impl/`: Tool registration and implementation (using `libcurl`, `libgumbo`, etc.).
    *   `curl_utils.h`: Shared `libcurl` utilities.
*   **Interfaces:** `ui_interface.h` (abstract base class), `cli_interface.cpp`/`.h` (CLI impl using `libreadline`), `gui_interface.cpp`/`.h` (GUI impl using ImGui/GLFW/OpenGL, runs `ChatClient` in worker thread).
*   **Entry Points:** `main_cli.cpp` (CLI), `main_gui.cpp` (GUI - manages GLFW/ImGui loop and worker thread).
*   **Configuration:** `config.h.in` -> `config.h` (API keys), `.env` file support (via `getenv`).
*   **Build System:** CMake (`CMakeLists.txt`), `build.sh`, `install.sh`.