# Plan to Set Initial Input Field Focus (Issue #5)

This document outlines the steps to ensure the text input field receives keyboard focus automatically when the LLM Client GUI starts.

## Analysis

*   The main GUI loop is located in `main_gui.cpp`.
*   The input field is created using `ImGui::InputText("##Input", ...)` within this loop.
*   Focus is currently set back to the input field *after* a message is sent using `ImGui::SetKeyboardFocusHere(-1);`.
*   There is no existing logic to set the focus when the application initially launches and renders the first frame.
*   The appropriate ImGui function to set focus programmatically is `ImGui::SetKeyboardFocusHere()`.

## Implementation Plan

- [ ] **Declare State Flag:** In `main_gui.cpp`, before the main `while (!glfwWindowShouldClose(window))` loop (e.g., around line 50), declare a static boolean variable to track if the initial focus has been set:
    ```cpp
    static bool initial_focus_set = false;
    ```
- [ ] **Set Focus on First Frame:** Inside the main `while` loop in `main_gui.cpp`, immediately *before* the `ImGui::InputText("##Input", ...)` call (around line 108), add the following logic:
    ```cpp
    // Set focus to the input field on the first frame
    if (!initial_focus_set) {
        ImGui::SetKeyboardFocusHere(0); // Target the next widget (InputText)
        initial_focus_set = true;
    }

    // Existing InputText call follows immediately after
    ImGui::PushItemWidth(input_width); // Keep existing PushItemWidth
    enter_pressed = ImGui::InputText("##Input", gui_ui.getInputBuffer(), gui_ui.getInputBufferSize(), ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::PopItemWidth(); // Keep existing PopItemWidth
    ```
- [ ] **Verification:** Build and run the application. Verify that the input field has the blinking cursor and accepts keyboard input immediately upon startup without requiring a click.