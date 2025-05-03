# Plan to Fix Issue #3: Dynamic Text Wrapping in Log Window

**Issue:** The log window content does not wrap dynamically when the window is resized, causing a horizontal scrollbar to appear. Text should wrap to fit the window width.

**Analysis:**
*   The log window is implemented using `ImGui::BeginChild` in `main_gui.cpp`.
*   The flag `ImGuiWindowFlags_HorizontalScrollbar` is currently enabled for this child window. This forces the horizontal scrollbar and prevents the intended text wrapping behavior.
*   Text rendering uses `ImGui::TextWrapped`, which is the correct function for achieving wrapping, but its effect is overridden by the scrollbar flag.

**Plan:**

*   [ ] **Modify `main_gui.cpp`:**
    *   Locate the line `ImGui::BeginChild("Output", ...);` (around line 82).
    *   Remove the `ImGuiWindowFlags_HorizontalScrollbar` flag from the `ImGui::BeginChild` call for the "Output" area. The line should look similar to this:
        ```cpp
        ImGui::BeginChild("Output", ImVec2(0, output_height), true); // Removed ImGuiWindowFlags_HorizontalScrollbar
        ```
*   [ ] **Verify `ImGui::TextWrapped`:**
    *   Confirm that `ImGui::TextWrapped` is still being used to render each line within the "Output" child window (around line 86).
*   [ ] **Test:**
    *   Compile and run the application.
    *   Add several long lines of text to the log (e.g., by sending messages or triggering output).
    *   Resize the main application window horizontally.
    *   Verify that the text within the log window wraps dynamically to fit the new width.
    *   Verify that the horizontal scrollbar no longer appears for the log content.