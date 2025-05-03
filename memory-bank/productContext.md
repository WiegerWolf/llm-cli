# Product Context

This file provides a high-level overview of the project and the expected product that will be created. Initially it is based upon projectBrief.md (if provided) and all other available project-related information in the working directory. This file is intended to be updated as the project evolves, and should be used to inform all other modes of the project's goals and context.
2025-05-04 01:42:36 - Log of updates made will be appended as footnotes to the end of this file.

*

## Project Goal

* Create a C++ chat assistant (CLI and GUI) that connects to OpenRouter LLM APIs and provides advanced web research tools.

## Key Features

*   Chat with LLMs (via OpenRouter)
*   Command-Line Interface (`llm-cli`)
*   Graphical User Interface (`llm-gui` via ImGui)
*   Web search (Brave Search, DuckDuckGo, Brave Search API)
*   Visit URLs (content extraction)
*   Web research (multi-step synthesis)
*   Deep research (sub-query breakdown)
*   Read conversation history (SQLite)
*   Tool call support (OpenAI function-calling)
*   .env support for API keys
*   Multi-threaded

## Overall Architecture

*   **Core Logic:** `chat_client.cpp` (LLM interaction, tool execution), `database.cpp` (SQLite history)
*   **Interfaces:** `cli_interface.cpp` (CLI), `gui_interface.cpp` (GUI), `ui_interface.h` (base class)
*   **Entry Points:** `main_cli.cpp` (CLI), `main_gui.cpp` (GUI)
*   **Tools:** Implementations in `tools_impl/`, registration/definition in `tools.cpp`/`.h`.
*   **Build System:** CMake, `build.sh`, `install.sh`