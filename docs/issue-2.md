# Plan to Remove "Clear History" Button (Issue #2)

This document outlines the steps to remove the unnecessary "Clear History" button from the LLM Client GUI, addressing the second issue listed in `docs/issues.md`.

**Target File:** `main_gui.cpp`

**Steps:**

1.  **Remove Button Creation:**
    *   Locate and delete the line responsible for creating the "Clear History" button:
        ```cpp
        // Around line 113
        clear_pressed = ImGui::Button("Clear History", ImVec2(clear_button_width, 0));
        ```

2.  **Remove Preceding `SameLine` Call:**
    *   Delete the `ImGui::SameLine();` call that positions the "Clear History" button next to the "Send" button:
        ```cpp
        // Around line 112
        ImGui::SameLine();
        ```

3.  **Remove `clear_pressed` Variable:**
    *   Delete the boolean variable used to track if the button was pressed:
        ```cpp
        // Around line 98
        bool clear_pressed = false;
        ```

4.  **Remove Width Constant:**
    *   Delete the constant defining the button's width:
        ```cpp
        // Around line 100
        const float clear_button_width = 80.0f; // Width for Clear History button
        ```

5.  **Adjust Input Field Width Calculation:**
    *   Modify the calculation for `input_width` to account for the removal of the button and its spacing. Change this:
        ```cpp
        // Around lines 101-102
        float input_width = display_size.x - button_width - clear_button_width
                           - ImGui::GetStyle().ItemSpacing.x * 2; // Adjust width for both buttons
        ```
    *   To this:
        ```cpp
        // Around lines 101-102 (adjust line number if needed)
        float input_width = display_size.x - button_width - ImGui::GetStyle().ItemSpacing.x; // Adjust width for Send button only
        ```

6.  **Remove Event Handling Logic:**
    *   Delete the `else if` block that handles the action when the "Clear History" button is clicked:
        ```cpp
        // Around lines 131-136
        else if (clear_pressed) {
            // Clear the local history vector
            output_history.clear();
            // Optionally, add a message indicating history was cleared
            // output_history.push_back("--- History Cleared ---");
        }