# Stage 1: Project Structure & Build System Refactoring

## Goal

Refactor the CMake build system and project structure to support separate CLI and GUI executables sharing a common core library. Rename the existing `main.cpp` and executable.

## Steps & Checklist

*   [ ] **Rename `main.cpp`:**
    *   [ ] Rename `main.cpp` to `main_cli.cpp`.
*   [ ] **Update `CMakeLists.txt`:**
    *   [ ] **Create Core Library Target:**
        *   [ ] Define a new `STATIC` library target (e.g., `llm_core`).
        *   [ ] Move core logic source files (`chat_client.cpp`, `database.cpp`, `tools.cpp`, `curl_utils.cpp` (if exists), `tools_impl/*.cpp`) from the main executable definition to the `llm_core` target.
        *   [ ] Ensure `llm_core` links against necessary dependencies (e.g., nlohmann_json, CURL, SQLite3, Gumbo, Threads).
        *   [ ] Set necessary include directories for `llm_core`.
    *   [ ] **Update CLI Executable Target:**
        *   [ ] Rename the existing executable target from `llm` to `llm-cli`.
        *   [ ] Update its sources to include only `main_cli.cpp` and `cli_interface.cpp`.
        *   [ ] Link `llm-cli` against the `llm_core` library.
        *   [ ] Link `llm-cli` against CLI-specific libraries (Readline).
        *   [ ] Ensure `llm-cli` includes necessary directories (e.g., from `llm_core`).
    *   [ ] **Update Installation:**
        *   [ ] Modify the `install(TARGETS ...)` command to install `llm-cli` instead of `llm`.
*   [ ] **Verify Build:**
    *   [ ] Run CMake configuration (`cmake ..`).
    *   [ ] Build the project (`make` or `cmake --build .`).
    *   [ ] Confirm the `llm-cli` executable is created and runs correctly.
    *   [ ] Confirm the `llm_core` static library is built (e.g., `libllm_core.a`).
