# Refactoring Stage 2: Implement the CLI Interface

**Goal:** Create a concrete implementation of `UserInterface` for the command-line interface, moving existing CLI logic into it.

**Prerequisites:** Stage 1 completed (`ui_interface.h` exists).

**Checklist:**

1.  [ ] **Create `cli_interface.h`:**
    *   Add include guards.
    *   Include `ui_interface.h`.
    *   Include `<string>`, `<optional>`.
    *   Declare `class CliInterface : public UserInterface`.
    *   Declare overrides for all pure virtual methods from `UserInterface`.
    *   Declare the default constructor and destructor.
2.  [ ] **Create `cli_interface.cpp`:**
    *   Include `cli_interface.h`.
    *   Include `<iostream>` for `std::cout`, `std::cerr`.
    *   Include `<cstdlib>` for `free`.
    *   Include `<readline/readline.h>` and `<readline/history.h>`.
3.  [ ] **Implement `CliInterface::initialize()`:**
    *   (Optional) Add any readline setup if needed (e.g., `rl_initialize`). Currently, `readline()` itself handles initialization implicitly. Add placeholder if desired.
4.  [ ] **Implement `CliInterface::shutdown()`:**
    *   (Optional) Add any readline cleanup if needed (e.g., `rl_clear_history`). Add placeholder if desired.
5.  [ ] **Implement `CliInterface::promptUserInput()`:**
    *   Move the code from the *current* `ChatClient::promptUserInput()` here.
    *   Use `readline("> ")`.
    *   Handle `nullptr` return (Ctrl+D) by returning `std::nullopt`.
    *   Use `add_history()` for non-empty input.
    *   Use `free()` on the C-string returned by `readline`.
    *   Return the `std::string` input wrapped in `std::optional`.
6.  [ ] **Implement `CliInterface::displayOutput()`:**
    *   Use `std::cout << output << std::flush;`.
7.  [ ] **Implement `CliInterface::displayError()`:**
    *   Use `std::cerr << "Error: " << error << std::endl;` (Consider adding a prefix like "Error: ").
8.  [ ] **Implement `CliInterface::displayStatus()`:**
    *   Use `std::cout << status << std::endl;` (Using `endl` to ensure it appears on its own line and flushes).
9.  [ ] **Review:** Ensure the header and source files compile and the implementations correctly use `readline`, `std::cout`, and `std::cerr`.
