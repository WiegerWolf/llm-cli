# Stage 1: Project Structure & Build System Refactoring

## Goal

Refactor the CMake build system and project structure to support separate CLI and GUI executables sharing a common core library. Rename the existing `main.cpp` and executable.

## Steps & Checklist

*   [x] **Rename `main.cpp`:**
    *   [x] Rename `main.cpp` to `main_cli.cpp`. (Done via `git mv`)
*   [x] **Update `CMakeLists.txt`:**
    *   [x] **Create Core Library Target:**
        *   [x] Define a new `STATIC` library target (`llm_core`).
        *   [x] Move core logic source files (`chat_client.cpp`, `database.cpp`, `tools.cpp`, `tools_impl/*.cpp`, relevant headers) to the `llm_core` target.
        *   [x] Ensure `llm_core` links against necessary dependencies (nlohmann_json, CURL, SQLite3, Gumbo, Threads).
        *   [x] Set necessary include directories for `llm_core` (`PUBLIC`).
    *   [x] **Update CLI Executable Target:**
        *   [x] Rename the existing executable target from `llm` to `llm-cli`.
        *   [x] Update its sources to include only `main_cli.cpp` and `cli_interface.cpp`.
        *   [x] Link `llm-cli` against the `llm_core` library.
        *   [x] Link `llm-cli` against CLI-specific libraries (Readline).
        *   [x] Ensure `llm-cli` includes necessary directories (implicitly via `llm_core` linkage).
    *   [x] **Update Installation:**
        *   [x] Modify the `install(TARGETS ...)` command to install `llm-cli` instead of `llm`.
*   [ ] **Verify Build:**
    *   [ ] Run CMake configuration (`./build.sh` or `cmake .. -B build`).
    *   [ ] Build the project (`make -C build` or `cmake --build build`).
    *   [ ] Confirm the `llm-cli` executable is created (`build/llm-cli`) and runs correctly.
    *   [ ] Confirm the `llm_core` static library is built (e.g., `build/libllm_core.a`).
