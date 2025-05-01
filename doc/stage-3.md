# Refactoring Stage 3: Integrate UI into Build System and Entry Point

**Goal:** Update the build system (`CMakeLists.txt`) to include the new UI files and modify the application entry point (`main.cpp`) to instantiate and manage the UI lifecycle.

**Prerequisites:** Stage 1 and Stage 2 completed (`ui_interface.h`, `cli_interface.h`, `cli_interface.cpp` exist).

**Checklist:**

1.  [X] **Modify `CMakeLists.txt`:**
    *   Find the `add_executable(llm ...)` command.
    *   Add `cli_interface.cpp` to the list of source files.
    *   Verify that `Readline_INCLUDE_DIRS` is added to `target_include_directories(llm PRIVATE ...)` (already present).
    *   Verify that `${Readline_LIBRARIES}` is added to `target_link_libraries(llm PRIVATE ...)` (already present).
2.  [X] **Modify `main.cpp`:**
    *   Include `cli_interface.h`.
    *   Inside `main()` function, before creating `ChatClient`:
        *   Instantiate the CLI UI: `CliInterface cli_ui;`.
        *   Call `cli_ui.initialize();`.
    *   **Temporarily Adapt `ChatClient` Instantiation:**
        *   The `ChatClient` constructor doesn't accept the UI yet. Either:
            *   Comment out the `ChatClient client;` and `client.run();` lines for now.
            *   *OR* Temporarily modify `ChatClient`'s constructor in `chat_client.h`/`.cpp` to accept `UserInterface&` (even if unused) to allow compilation. This change will be finalized in Stage 4. **(Recommended approach for continuous compilation)**. **DONE** (Modified constructor in `chat_client.h/.cpp`)
    *   Inside `main()`, just before `return 0;` (or after the `try` block if `client.run()` is active):
        *   Call `cli_ui.shutdown();`. **DONE**
    *   Ensure the `catch` blocks remain to handle potential errors. **DONE** (Modified catch blocks to use UI)
3.  [X] **Build & Verify:**
    *   Run `./build.sh`. **DONE**
    *   Ensure the application compiles successfully. **DONE**
    *   If `client.run()` was commented out, the program should just initialize/shutdown the UI and exit.
    *   If `ChatClient` was temporarily adapted, the program might run but UI calls won't work correctly yet.
