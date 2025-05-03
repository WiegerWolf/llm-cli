# Plan to Display User Messages in GUI Log (Issue #1)

**File to Modify:** `main_gui.cpp`

**Goal:** Ensure that messages typed by the user appear in the main output/log window immediately after they are sent.

**Analysis:**
User input is handled in the main render loop within `main_gui.cpp`. When the "Send" button or Enter key is pressed, the content of the `input_buf` is sent to the backend worker thread via `gui_ui.sendInputToWorker()`. However, this input is *not* currently added to the `output_history` vector, which is the data source for the log display area.

**Proposed Changes (Step-by-Step):**

1.  **Locate Input Handling Block:** In `main_gui.cpp`, find the `if (send_pressed || enter_pressed)` block (around line 120).
2.  **Add User Message to History:** Inside this `if` block, *before* the line `input_buf[0] = '\0';` (which clears the input buffer), add the following code:
    ```cpp
    // Add the user's message to the history for display
    output_history.push_back("User: " + std::string(input_buf));
    ```
    *   This takes the current input buffer content, converts it to a `std::string`, prepends "User: " for clarity, and adds it to the `output_history` vector.
3.  **Trigger Auto-Scroll:** Immediately after adding the message to `output_history`, ensure the log scrolls to show the new message. Add this line:
    ```cpp
    new_output_added = true; // Ensure the log scrolls down
    ```
    *   This sets the flag used by the output area rendering logic to trigger an auto-scroll if the user is already near the bottom.

**Verification:**
After implementing these changes, run the GUI client. Type a message and press Enter or click "Send". The message, prefixed with "User: ", should appear in the log window above the input box, and the log should scroll down if necessary. Subsequent responses from the LLM should appear below the user's message.