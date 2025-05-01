# Refactoring Stage 4: Inject UI Dependency into ChatClient

**Goal:** Modify `ChatClient` to accept and store a reference to a `UserInterface` object via its constructor (Dependency Injection).

**Prerequisites:** Stage 1, 2, 3 completed. `main.cpp` instantiates `CliInterface`.

**Checklist:**

1.  [ ] **Modify `chat_client.h`:**
    *   Include `ui_interface.h` near the top.
    *   Remove `#include <readline/readline.h>` and `#include <readline/history.h>` if present.
    *   Add a private member variable: `UserInterface& ui;`.
    *   Change the constructor declaration from `ChatClient();` (or default) to `explicit ChatClient(UserInterface& ui_ref);`. (Using `explicit` is good practice for single-argument constructors).
2.  [ ] **Modify `chat_client.cpp`:**
    *   Include `ui_interface.h` if not already present (should be).
    *   Remove `#include <readline/readline.h>` and `#include <readline/history.h>` if present.
    *   Update the constructor definition:
        ```cpp
        ChatClient::ChatClient(UserInterface& ui_ref) : 
            db(),              // Initialize PersistenceManager
            toolManager(),     // Initialize ToolManager
            ui(ui_ref)         // Initialize the UI reference
        {
            // Constructor body (if any)
        }
        ```
3.  [ ] **Update `main.cpp`:**
    *   Ensure the `ChatClient` instantiation passes the `cli_ui` object: `ChatClient client(cli_ui);`. (This might have been done temporarily in Stage 3).
    *   Uncomment `client.run();` if it was commented out.
4.  [ ] **Build & Verify:**
    *   Run `./build.sh`.
    *   Ensure the application compiles successfully.
    *   Run the application (`./build/llm`). It should still function as before, but the UI dependency is now injected, ready for use in the next stage. Direct I/O calls within `ChatClient` are still active.
