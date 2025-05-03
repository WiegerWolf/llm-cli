# Stage 3: GUI Layout & Basic Interaction

## Goal

Implement the primary user interface elements within the ImGui window: a scrolling output area, a text input box, a status bar, and a "Send" button. Handle the "Send" button click or Enter key press in the input box to capture user input.

## Prerequisites

*   Completion of Stage 2 (GUI Dependencies & Basic Window).

## Steps & Checklist

*   [x] **Refine `GuiInterface`:**
    *   [x] Add private member variables to store:
        *   [x] Input buffer: `char input_buf[1024] = "";` (or `std::string`)
        *   [x] Output history: `std::vector<std::string> output_history;` (or similar structure like `ImGuiTextBuffer`)
        *   [x] Status text: `std::string status_text = "Ready";`
    *   [x] Add thread-safe mechanisms (placeholders for now, implementation in Stage 4):
        *   [x] Mutex for output/status: `std::mutex display_mutex;`
        *   [x] Queue for user input: `std::queue<std::string> input_queue;`
        *   [x] Mutex for input queue: `std::mutex input_mutex;`
        *   [x] Condition variable for input: `std::condition_variable input_cv;`
        *   [x] Queue for display updates: `std::queue<std::pair<std::string, int>> display_queue;` (int could represent type: 0=output, 1=error, 2=status)
*   [x] **Update `main_gui.cpp` Render Loop:**
    *   [x] **Get Window Size:** Get the main viewport size: `ImGui::GetIO().DisplaySize`.
    *   [x] **Main Window:** Use `ImGui::SetNextWindowPos` and `ImGui::SetNextWindowSize` to make the main window fill the application window. Use flags like `ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_MenuBar` (if a menu bar is desired later).
    *   [x] **Output Area:**
        *   [x] Create a child window (`ImGui::BeginChild`) for the scrolling output. Give it a fixed height (e.g., `window_height - input_height - status_height`).
        *   [x] Iterate through `output_history` (or use `ImGuiTextBuffer::appendf`) and display each message using `ImGui::TextUnformatted`.
        *   [x] Add `ImGui::SetScrollHereY(1.0f)` if new output was added to auto-scroll.
        *   [x] `ImGui::EndChild()`.
    *   [x] **Input Area:**
        *   [x] Use `ImGui::InputText("Input", input_buf, IM_ARRAYSIZE(input_buf), ImGuiInputTextFlags_EnterReturnsTrue)`. Store the return value (true if Enter was pressed).
        *   [x] Place a "Send" button next to it: `ImGui::SameLine(); bool send_pressed = ImGui::Button("Send");`.
    *   [x] **Status Bar:**
        *   [x] Use `ImGui::Separator()` above it.
        *   [x] Display the `status_text` using `ImGui::Text()`.
    *   [x] **Input Handling:**
        *   [x] Check if `send_pressed` or the `InputText` returned true.
        *   [x] If true and `input_buf` is not empty:
            *   [x] **(Placeholder)** Print the input to `std::cout` for now: `std::cout << "Input captured: " << input_buf << std::endl;`. (Actual queuing happens in Stage 4).
            *   [x] Clear the input buffer: `input_buf[0] = '\0';`.
            *   [x] Set keyboard focus back to the input widget: `ImGui::SetKeyboardFocusHere(-1);` (call this *before* `InputText` in the next frame).
*   [x] **(Placeholder) Update `GuiInterface` Display Methods:**
    *   [x] Modify `displayOutput`, `displayError`, `displayStatus` to temporarily add the received string directly to `output_history` or update `status_text`. **Note:** This is NOT thread-safe yet and only for basic testing. Proper queuing will be implemented in Stage 4.
    *   [x] Example (temporary, non-thread-safe):
        ```cpp
        void GuiInterface::displayOutput(const std::string& output) {
            // NOT THREAD SAFE - Just for basic layout testing
            // std::lock_guard<std::mutex> lock(display_mutex); // Will add in Stage 4
            output_history.push_back(output);
        }
        void GuiInterface::displayStatus(const std::string& status) {
            // NOT THREAD SAFE - Just for basic layout testing
            // std::lock_guard<std::mutex> lock(display_mutex); // Will add in Stage 4
            status_text = status;
        }
        ```
*   [x] **Verify Build & Run:**
    *   [x] Build the project.
    *   [x] Run `./llm-gui`.
    *   [x] Verify the layout: scrolling output area, input box, send button, status bar.
    *   [x] Type text in the input box and press Enter or click "Send". Verify the text is printed to the console (placeholder action) and the input box clears.
    *   [x] (Optional) Manually call `gui_ui.displayOutput("Test output");` or `gui_ui.displayStatus("Test status");` from `main_gui.cpp` before the loop to see if they appear correctly in the GUI (this bypasses threading).
