# Plan for Issue 8: Styling Different Message Types in History Log

This document outlines the plan to implement distinct visual styling for different message types (user input, LLM response, status updates, errors) in the GUI's history log, addressing Issue #8 from `docs/issues.md`.

## Analysis Summary

*   **File to Modify:** `main_gui.cpp`
*   **Data Storage:** Messages are stored chronologically in `std::vector<std::string> output_history` within `main_gui.cpp`.
*   **Message Type Identification:** Based on current implementation and plans for Issue 1 and Issue 7, message types can be identified using string prefixes:
    *   **User Input:** Prefixed with `"User: "` (added in `main_gui.cpp` as per Issue 1 plan).
    *   **Status Message:** Prefixed with `"[STATUS] "` (added in `GuiInterface::processDisplayQueue` as per Issue 7 plan).
    *   **Error Message:** Prefixed with `"ERROR: "` (added in `GuiInterface::processDisplayQueue`).
    *   **LLM Response:** No prefix (default case, added in `GuiInterface::processDisplayQueue`).
*   **Current Rendering:** The loop in `main_gui.cpp` (around line 84) iterates through `output_history` and renders each string using `ImGui::TextWrapped` with the default text color.
*   **Styling Capabilities:** The ImGui library provides functions like `ImGui::PushStyleColor`, `ImGui::PopStyleColor`, and `ImGui::TextColored` suitable for applying different text colors.

## Implementation Plan

The core idea is to modify the rendering loop in `main_gui.cpp` to check the prefix of each message and apply a specific color before rendering it.

1.  **Locate Rendering Loop:**
    *   [ ] In `main_gui.cpp`, find the `for` loop that iterates over the `output_history` vector (currently around line 84).

2.  **Implement Conditional Styling:**
    *   [ ] Inside the loop, *before* the `ImGui::TextWrapped` call, add logic to check the beginning of the current `line` string for the known prefixes.
    *   [ ] Use `std::string::rfind("prefix", 0) == 0` or `strncmp` for prefix checking.
    *   [ ] Based on the detected prefix, use `ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(...))` to set the desired color.
    *   [ ] **Color Scheme:**
        *   `"User: "` : Green (e.g., `ImVec4(0.1f, 1.0f, 0.1f, 1.0f)`)
        *   `"[STATUS] "` : Yellow (e.g., `ImVec4(1.0f, 1.0f, 0.2f, 1.0f)`)
        *   `"ERROR: "` : Red (e.g., `ImVec4(1.0f, 0.2f, 0.2f, 1.0f)`)
        *   _(No prefix - LLM Response)_ : Default (No `PushStyleColor` needed, uses standard text color).
    *   [ ] After the `ImGui::TextWrapped` call, if a color was pushed, call `ImGui::PopStyleColor()` to restore the default color for subsequent UI elements.

3.  **Example Code Structure (Conceptual):**
    ```cpp
    // Inside the loop in main_gui.cpp (around line 84)
    for (const auto& line : output_history) {
        bool color_pushed = false;

        // Check for prefixes and push color if match found
        if (line.rfind("User: ", 0) == 0) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.1f, 1.0f, 0.1f, 1.0f)); // Green
            color_pushed = true;
        } else if (line.rfind("[STATUS] ", 0) == 0) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 0.2f, 1.0f)); // Yellow
            color_pushed = true;
        } else if (line.rfind("ERROR: ", 0) == 0) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.2f, 0.2f, 1.0f)); // Red
            color_pushed = true;
        }
        // No else needed - default color is used if no prefix matches

        // Render the text (always happens)
        ImGui::TextWrapped("%s", line.c_str());

        // Pop color if one was pushed
        if (color_pushed) {
            ImGui::PopStyleColor();
        }
    }
    ```

## Verification Steps

*   [ ] Compile and run the GUI application after implementing the changes.
*   [ ] Type and send a user message. Verify it appears in the log with the **Green** color.
*   [ ] Observe status messages during startup or operation (e.g., "Ready."). Verify they appear with the **Yellow** color.
*   [ ] If possible, trigger an action that generates an error message. Verify it appears with the **Red** color.
*   [ ] Receive a response from the LLM. Verify it appears with the **default text color**.
*   [ ] Check that other UI elements (input box, buttons) retain their standard colors and are not affected by the history log styling.