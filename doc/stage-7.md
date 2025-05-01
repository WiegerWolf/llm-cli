# Refactoring Stage 7: Finalize Entry Point and Cleanup

**Goal:** Ensure `main.cpp` correctly manages the UI lifecycle and remove any remaining unused includes related to direct I/O.

**Prerequisites:** Stage 6 completed. All direct user-facing I/O goes through the `UserInterface`.

**Checklist:**

1.  [ ] **Review `main.cpp`:**
    *   Verify `CliInterface cli_ui;` is created before `ChatClient`.
    *   Verify `cli_ui.initialize();` is called before `client.run()`.
    *   Verify `ChatClient client(cli_ui);` correctly injects the UI.
    *   Verify `client.run();` is called within the `try` block.
    *   Verify `cli_ui.shutdown();` is called before the program exits (e.g., after the `try`/`catch` block or before `return 0`).
    *   Verify `catch` blocks use `std::cerr` for *fatal* errors that prevent the UI from working, or potentially `cli_ui.displayError` if the UI is still functional enough. (Current use of `cerr` for fatal errors seems appropriate).
    *   Remove `#include <iostream>` if only used for the final "Exiting..." message and replace that message with `cli_ui.displayOutput("\nExiting...\n");` if desired. (Let's keep `cerr` for fatal errors and `cout` for the final exit message for now, as it's outside the main client loop).
2.  [ ] **Review Includes Across Project:**
    *   Double-check all modified `.cpp` files (`main.cpp`, `chat_client.cpp`, `tools.cpp`, `tools_impl/*.cpp`, `cli_interface.cpp`) for any remaining `#include <iostream>` that are no longer necessary (i.e., all `std::cout`/`cerr` replaced by `ui` calls or deemed acceptable like fatal errors in `main`).
    *   Double-check for stray `readline` or `cstdlib` includes where they aren't needed.
3.  [ ] **Review `CMakeLists.txt`:**
    *   Confirm all necessary source files (`cli_interface.cpp`, etc.) are listed.
    *   Confirm necessary libraries (`Readline`, `Threads`, `CURL`, etc.) are linked.
    *   Confirm include directories are correctly set.
4.  [ ] **Build & Verify:**
    *   Run `./build.sh`. Ensure clean compilation without warnings about unused variables or includes if possible.
    *   Run `./build/llm`. Perform a quick smoke test to ensure basic functionality remains.
