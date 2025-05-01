# Refactoring Plan: Separating UI from Core Logic

## 1. Goal

Refactor the `llm-cli` application to separate the command-line interface (CLI) presentation logic from the core application (business) logic.

## 2. Motivation

*   **Modularity:** Decouple the core chat functionality from the specific way it interacts with the user via the terminal.
*   **Testability:** Allow for easier unit/integration testing of the core logic without needing a running terminal.
*   **Extensibility:** Enable the creation of alternative user interfaces (e.g., GUI, web interface, SDL) in the future by swapping out the UI component without major changes to the core.
*   **Maintainability:** Improve code organization and make it easier to understand and modify different parts of the application independently.

## 3. Current Architecture Issues

*   **Mixed Concerns:** The `ChatClient` class currently handles both core logic (API calls, tool execution, state management via `PersistenceManager`) and presentation logic (reading input via `readline`/`add_history` in `promptUserInput`, printing output/errors/status via `std::cout`/`std::cerr` in `run`, `handleApiError`, `executeStandardToolCalls`, `executeFallbackFunctionTags`, `printAndSaveAssistantContent`).
*   **Direct Output:** `ToolManager::execute_tool` and tool implementations (`tools_impl/web_research_tool.cpp`, `tools_impl/deep_research_tool.cpp`) print status messages directly to `std::cout`, bypassing the main application flow.
*   **CLI Dependencies:** `ChatClient` directly depends on `readline` headers and functions.

## 4. Proposed Architecture

We will introduce a clear boundary between the core application and the user interface using an abstraction layer.

*   **`UserInterface` (Abstract Base Class/Interface):**
    *   Defines the contract for how the core logic interacts with the user.
    *   Located in a new file (e.g., `ui_interface.h`).
    *   Declares pure virtual methods like:
        *   `virtual std::optional<std::string> promptUserInput() = 0;`
        *   `virtual void displayOutput(const std::string& output) = 0;`
        *   `virtual void displayError(const std::string& error) = 0;`
        *   `virtual void displayStatus(const std::string& status) = 0;`
        *   `virtual void initialize() = 0;`
        *   `virtual void shutdown() = 0;`
        *   `virtual ~UserInterface() = default;` // Virtual destructor

*   **`CliInterface` (Concrete Implementation):**
    *   Implements the `UserInterface` interface for the command line.
    *   Located in new files (e.g., `cli_interface.h`, `cli_interface.cpp`).
    *   Uses `readline`, `std::cout`, `std::cerr` to fulfill the interface contract.
    *   Contains all CLI-specific dependencies and logic.

*   **`ChatClient` (Core Logic):**
    *   Modified to hold a `UserInterface&` (passed via constructor - Dependency Injection).
    *   All direct console I/O (`readline`, `free`, `add_history`, `std::cout`, `std::cerr`) will be replaced with calls to the `UserInterface` methods (e.g., `ui.promptUserInput()`, `ui.displayOutput()`, `ui.displayError()`, `ui.displayStatus()`).
    *   Will no longer include `readline` headers or call its functions directly.
    *   Will pass the `UserInterface&` to methods like `executeAndPrepareToolResult` and `executeFallbackFunctionTags` so it can be propagated to `ToolManager`.

*   **`ToolManager` / Tool Implementations:**
    *   `ToolManager::execute_tool` signature changed to accept `UserInterface& ui`.
    *   Tool implementation functions that print status (e.g., `perform_web_research`, `perform_deep_research`) signatures changed to accept `UserInterface& ui`.
    *   The `UserInterface&` will be passed from `ChatClient` -> `executeAndPrepareToolResult`/`executeFallbackFunctionTags` -> `ToolManager::execute_tool` -> specific tool implementations (`perform_web_research`, `perform_deep_research`).
    *   Direct `std::cout` calls for status messages in `ToolManager::execute_tool` and relevant tool implementations will be replaced with calls to `ui.displayStatus(...)`.
    *   Direct `std::cerr` calls for user-facing errors within tools (if any) should likely become `ui.displayError(...)`.

*   **`main.cpp` (Entry Point):**
    *   Instantiates the concrete `CliInterface`.
    *   Instantiates the concrete `CliInterface` (e.g., `CliInterface cli_ui;`).
    *   Instantiates `ChatClient`, injecting the `CliInterface` instance (e.g., `ChatClient client(cli_ui);`).
    *   Calls `cli_ui.initialize()`, `client.run()`, and `cli_ui.shutdown()`.

## 5. Refactoring Steps (High-Level)

1.  **Define `UserInterface`:** Create `ui_interface.h` with the abstract base class and its pure virtual methods.
2.  **Create `CliInterface`:** Create `cli_interface.h` and `cli_interface.cpp`. Implement the `UserInterface` methods using existing CLI logic (move code from `ChatClient`, etc.).
3.  **Update Build System:** Add the new `ui_interface.h`, `cli_interface.h`, and `cli_interface.cpp` to `CMakeLists.txt`. Ensure dependencies are correctly handled (e.g., `readline` linked only where `CliInterface` is built, if possible, or kept linked to the main executable for simplicity initially).
4.  **Inject UI into `ChatClient`:** Modify `ChatClient`'s constructor to accept a `UserInterface&`. Store it as a member.
5.  **Refactor `ChatClient` I/O:** Replace all `readline`, `std::cout`, `std::cerr` calls in `ChatClient` with calls to the corresponding `UserInterface` methods via the stored reference.
6.  **Refactor `ToolManager`/Tools:** Modify `ToolManager::execute_tool` and relevant tool implementations (`search_web`, `visit_url`, etc.) to accept the `UserInterface&` and use `ui.displayStatus()` instead of `std::cout`.
7.  **Update `main.cpp`:** Modify `main` to create `CliInterface`, inject it into `ChatClient`, and manage the UI lifecycle (`initialize`, `shutdown`).
8.  **Testing:** Build and test the application thoroughly to ensure no functionality is broken.

## 6. Potential Challenges & Considerations

*   **Status Message Propagation:** Passing the `UserInterface&` down through `ChatClient` -> `ToolManager` -> specific tool functions requires careful modification of function signatures as outlined in the steps.
*   **Error Handling Granularity:** Differentiate between errors that should just be displayed to the user (use `ui.displayError()`) versus those that are fatal. Internal debug messages (`std::cerr`) might remain if not intended for the end-user, or a proper logging framework could be introduced later.
*   **Build System Complexity:** Ensuring `readline` headers/libraries are only linked where `CliInterface` is defined might require creating a separate library/object file for `CliInterface` or adjusting include paths. Linking `readline` to the main executable remains the simpler initial approach.
*   **Incremental Changes:** This refactoring can be done incrementally. A good order might be: 1) Define interfaces, create `CliInterface`, update `main.cpp`. 2) Refactor `ChatClient` I/O. 3) Refactor `ToolManager` and tool implementations.

This plan provides a roadmap for the refactoring. We can proceed step-by-step, refining details as needed.
