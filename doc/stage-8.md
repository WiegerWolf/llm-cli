# Refactoring Stage 8: Testing and Verification

**Goal:** Thoroughly test the refactored application to ensure all functionality works as expected and that the separation of concerns is effective.

**Prerequisites:** Stage 7 completed. The application compiles and runs with the UI abstraction in place.

**Checklist:**

1.  [x] **Compile:** Run `./build.sh`. Ensure a clean build.
2.  [x] **Run:** Execute `./build/llm`.
3.  [x] **Basic Chat:**
    *   [x] Send simple messages.
    *   [x] Receive responses.
    *   [x] Verify output formatting via `CliInterface`.
4.  [x] **Tool Usage:** Trigger each tool explicitly or through natural language:
    *   [x] `search_web`: Verify status message (`[Status] Searching...`) and results display.
    *   [x] `visit_url`: Verify status message (`[Status] Visiting...`) and results display.
    *   [x] `get_current_datetime`: Verify status message (`[Status] Getting...`) and results display.
    *   [x] `read_history`: Verify status message (`[Status] Reading...`) and results display. Test different time ranges.
    *   [x] `web_research`: Verify multi-step status messages (`[Status] Step 1...`, etc.) and final synthesized result display.
    *   [x] `deep_research`: Verify multi-step status messages (`[Status] Step 1...`, etc.), parallel execution status, and final synthesized report display.
5.  [x] **Error Handling:**
    *   [x] **API Errors:** Simulate or trigger an API error (e.g., invalid API key temporarily). Verify error message is displayed via `ui.displayError()`.
    *   [x] **Tool Argument Errors:** Call a tool with missing/invalid arguments (e.g., `search_web` without query). Verify error message is displayed via `ui.displayError()`.
    *   [x] **Tool Execution Errors:**
        *   Simulate `visit_url` failing (e.g., invalid URL). Verify error message in tool result.
        *   Simulate `search_web` failing (e.g., network issue during search). Verify error message.
        *   Simulate `web_research` or `deep_research` sub-steps failing. Verify errors are reported correctly in the final output or via `ui.displayError()`.
    *   [x] **Fallback Parser:** Test scenarios where the model might use `<function>` tags instead of `tool_calls`. Verify status (`[Status] Executing function...`) and results.
6.  [x] **Input Handling:**
    *   [x] Empty input (just press Enter). Should do nothing.
    *   [x] Exit with Ctrl+D. Verify clean exit.
    *   [x] History navigation (Up/Down arrows). Verify `readline` history works.
7.  [x] **Database Interaction:**
    *   [x] Check `~/.llm-cli-chat.db` after running. Verify user, assistant, and tool messages (including JSON content for tool messages) are saved correctly.
    *   [x] Restart the application. Verify context is loaded correctly.
    *   [x] Verify `cleanupOrphanedToolMessages` runs on startup (check logs/debug output if added, or manually inspect DB before/after).
8.  [x] **Code Review (Self):**
    *   [x] Look for any remaining direct `std::cout`/`cerr` calls that should go through the UI.
    *   [x] Confirm no `readline` usage outside `cli_interface.cpp`.
    *   [x] Verify `UserInterface&` is passed correctly through call chains.
9.  [x] **Final Verification:** Confirm the application behavior is identical to the pre-refactoring state, except for the formatting/prefixing of status and error messages handled by `CliInterface`.
