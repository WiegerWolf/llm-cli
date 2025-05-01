# Refactoring Stage 8: Testing and Verification

**Goal:** Thoroughly test the refactored application to ensure all functionality works as expected and that the separation of concerns is effective.

**Prerequisites:** Stage 7 completed. The application compiles and runs with the UI abstraction in place.

**Checklist:**

1.  [ ] **Compile:** Run `./build.sh`. Ensure a clean build.
2.  [ ] **Run:** Execute `./build/llm`.
3.  [ ] **Basic Chat:**
    *   [ ] Send simple messages.
    *   [ ] Receive responses.
    *   [ ] Verify output formatting via `CliInterface`.
4.  [ ] **Tool Usage:** Trigger each tool explicitly or through natural language:
    *   [ ] `search_web`: Verify status message (`[Status] Searching...`) and results display.
    *   [ ] `visit_url`: Verify status message (`[Status] Visiting...`) and results display.
    *   [ ] `get_current_datetime`: Verify status message (`[Status] Getting...`) and results display.
    *   [ ] `read_history`: Verify status message (`[Status] Reading...`) and results display. Test different time ranges.
    *   [ ] `web_research`: Verify multi-step status messages (`[Status] Step 1...`, etc.) and final synthesized result display.
    *   [ ] `deep_research`: Verify multi-step status messages (`[Status] Step 1...`, etc.), parallel execution status, and final synthesized report display.
5.  [ ] **Error Handling:**
    *   [ ] **API Errors:** Simulate or trigger an API error (e.g., invalid API key temporarily). Verify error message is displayed via `ui.displayError()`.
    *   [ ] **Tool Argument Errors:** Call a tool with missing/invalid arguments (e.g., `search_web` without query). Verify error message is displayed via `ui.displayError()`.
    *   [ ] **Tool Execution Errors:**
        *   Simulate `visit_url` failing (e.g., invalid URL). Verify error message in tool result.
        *   Simulate `search_web` failing (e.g., network issue during search). Verify error message.
        *   Simulate `web_research` or `deep_research` sub-steps failing. Verify errors are reported correctly in the final output or via `ui.displayError()`.
    *   [ ] **Fallback Parser:** Test scenarios where the model might use `<function>` tags instead of `tool_calls`. Verify status (`[Status] Executing function...`) and results.
6.  [ ] **Input Handling:**
    *   [ ] Empty input (just press Enter). Should do nothing.
    *   [ ] Exit with Ctrl+D. Verify clean exit.
    *   [ ] History navigation (Up/Down arrows). Verify `readline` history works.
7.  [ ] **Database Interaction:**
    *   [ ] Check `~/.llm-cli-chat.db` after running. Verify user, assistant, and tool messages (including JSON content for tool messages) are saved correctly.
    *   [ ] Restart the application. Verify context is loaded correctly.
    *   [ ] Verify `cleanupOrphanedToolMessages` runs on startup (check logs/debug output if added, or manually inspect DB before/after).
8.  [ ] **Code Review (Self):**
    *   [ ] Look for any remaining direct `std::cout`/`cerr` calls that should go through the UI.
    *   [ ] Confirm no `readline` usage outside `cli_interface.cpp`.
    *   [ ] Verify `UserInterface&` is passed correctly through call chains.
9.  [ ] **Final Verification:** Confirm the application behavior is identical to the pre-refactoring state, except for the formatting/prefixing of status and error messages handled by `CliInterface`.
