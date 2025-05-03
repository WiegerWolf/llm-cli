# Issue 4: Input Field Loses Focus After Sending

## Summary

The text input field in the GUI loses keyboard focus after the user sends a message (either by clicking "Send" or pressing Enter). Focus should remain in the input field to allow for immediate typing of the next message without requiring an extra click.

## Analysis

*   The main GUI loop is handled in `main_gui.cpp`.
*   The input field is rendered using `ImGui::InputText("##Input", ...)` around line 108.
*   Input handling (detecting Enter press or "Send" button click) occurs around lines 120-130.
*   Currently, after sending the input to the worker thread (`gui_ui.sendInputToWorker`) and clearing the input buffer, there is an attempt to restore focus using `ImGui::SetKeyboardFocusHere(-1);` on line 130.
*   This existing call targets the *previous* widget, which should be the `InputText` widget. However, this method might be unreliable due to the timing within ImGui's frame processing. Focus requests typically apply to the *next* frame, and the state might change before the focus is actually set.

## Proposed Solution

Implement a more explicit focus management strategy using a flag:

1.  Set a boolean flag when a message is successfully sent.
2.  In the *next* frame, *before* rendering the `InputText` widget, check this flag.
3.  If the flag is set, call `ImGui::SetKeyboardFocusHere()` (with no arguments, targeting the *next* submitted widget) and reset the flag.

This ensures the focus request is made immediately before the target widget is submitted to ImGui for the frame where focus should be active.

## Implementation Plan

Modify `main_gui.cpp`:

1.  **Declare Flag:** Add a static boolean flag within the `main` function scope, before the main loop:
    ```cpp
    // Inside main(), before the while loop
    static bool request_input_focus = false;
    ```

2.  **Modify Input Handling Block:** In the `if (send_pressed || enter_pressed)` block (around lines 120-130):
    *   Remove the existing `ImGui::SetKeyboardFocusHere(-1);` call (line 130).
    *   After clearing the input buffer (line 127), set the flag:
        ```cpp
        // Inside the if (send_pressed || enter_pressed) block...
        if (input_buf[0] != '\0') {
            gui_ui.sendInputToWorker(input_buf);
            input_buf[0] = '\0';
            request_input_focus = true; // Set flag to request focus next frame
        }
        // Remove: ImGui::SetKeyboardFocusHere(-1);
        ```

3.  **Add Focus Request Before Input Widget:** Before rendering the input text widget (before line 107 `ImGui::PushItemWidth(input_width);`), add the conditional focus request:
    ```cpp
    // --- Input Area ---
    // ... (calculate input_width etc.)

    // Request focus for the input field if the flag was set in the previous frame
    if (request_input_focus) {
        ImGui::SetKeyboardFocusHere(); // Target the *next* widget (InputText)
        request_input_focus = false;   // Reset the flag
    }

    ImGui::PushItemWidth(input_width);
    enter_pressed = ImGui::InputText("##Input", gui_ui.getInputBuffer(), gui_ui.getInputBufferSize(), ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::PopItemWidth();
    // ... rest of input area (buttons etc.)
    ```

## Verification

1.  Compile and run the GUI application.
2.  Type a message into the input field and press Enter or click "Send".
3.  Verify that the input field immediately regains focus (the cursor should be blinking inside it) without requiring an additional click.
4.  Test sending multiple messages consecutively.