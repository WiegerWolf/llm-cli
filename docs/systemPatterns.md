# System Patterns

This file documents recurring patterns and standards used in the project.
It is optional, but recommended to be updated as the project evolves.
*

## Coding Patterns

*   **C++20 Standard:** Utilizes modern C++ features (e.g., `<stop_token>`, RAII with `std::unique_ptr`).
*   **Pimpl Idiom:** Employed in `PersistenceManager` to hide SQLite implementation details.
*   **Thread-Safe Communication:** Uses standard library components (`std::mutex`, `std::condition_variable`, `std::atomic`, `std::queue`) for GUI/worker thread interaction.
*   **Library Usage:** Leverages external libraries: `nlohmann/json` (JSON), `libcurl` (HTTP), `libgumbo` (HTML parsing), `sqlite3` (database C API), `libreadline` (CLI input), Dear ImGui (vendored as a Git submodule in `extern/imgui`, compiled directly) + backends (GUI).
*   **Resource Embedding:** Embeds font data directly into source code (`resources/noto_sans_font.h`).
*   

## Architectural Patterns

*   **Persistence Layer:** `PersistenceManager` class encapsulates SQLite database operations, using the Pimpl idiom. Database stored at `~/.llm-cli-chat.db`.
*   **API Interaction Layer:** `ChatClient` class manages communication with the OpenRouter API (`libcurl`), handles both standard OpenAI `tool_calls` and custom `<function>` tag fallbacks, and performs secure history reconstruction.
*   **GUI Concurrency Model:** The GUI (Dear ImGui (vendored as a Git submodule in `extern/imgui`, compiled directly)/GLFW/OpenGL) runs on the main thread, while core chat logic (`ChatClient`) operates in a separate worker thread. Communication relies on thread-safe queues and synchronization primitives.
*   **Separation of Concerns:** Core logic (`chat_client`, `database`) is separated from UI (`cli_interface`, `gui_interface`).
*   **Interface Abstraction:** `ui_interface.h` provides a base class for different UIs.
*   **Modular Tools:** Tool implementations reside in `tools_impl/` and are registered centrally (`tools.cpp`).
*   **Build System:** CMake manages dependencies and build process (`CMakeLists.txt`, `build.sh`, `install.sh`).
    *   **Submodule Initialization:** After cloning the repository, you need to initialize and update the Git submodules (which includes Dear ImGui for the graphical interface). Run the following command in the project root directory:
        ```sh
        git submodule update --init --recursive
        ```

## Testing Patterns

*