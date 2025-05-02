# Stage 3: GUI Layout & Basic Interaction

## Goal

Implement the primary user interface elements within the ImGui window: a scrolling output area, a text input box, a status bar, and a "Send" button. Handle the "Send" button click or Enter key press in the input box to capture user input.

## Prerequisites

*   Completion of Stage 2 (GUI Dependencies & Basic Window).

## Steps & Checklist

*   [ ] **Refine `GuiInterface`:**
    *   [ ] Add private member variables to store:
        *   [ ] Input buffer: `char input_buf[1024] = "";` (or `std::string`)
        *   [ ] Output history: `std::vector<std::string> output_history;` (or similar structure like `ImGuiTextBuffer`)
        *   [ ] Status text: `std::string status_text = "Ready";`
    *   [ ] Add thread-safe mechanisms (placeholders for now, implementation in Stage 4):
        *   [ ] Mutex for output/status: `std::mutex display_mutex;`
        *   [ ] Queue for user input: `std::queue<std::string> input_queue;`
        *   [ ] Mutex for input queue: `std::mutex input_mutex;`
        *   [ ] Condition variable for input: `std::condition_variable input_cv;`
        *   [ ] Queue for display updates: `std::queue<std::pair<std::string, int>> display_queue;` (int could represent type: 0=output, 1=error, 2=status)
*   [ ] **Update `main_gui.cpp` Render Loop:**
    *   [ ] **Get Window Size:** Get the main viewport size: `ImGui::GetIO().DisplaySize`.
    *   [ ] **Main Window:** Use `ImGui::SetNextWindowPos` and `ImGui::SetNextWindowSize` to make the main window fill the application window. Use flags like `ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_MenuBar` (if a menu bar is desired later).
    *   [ ] **Output Area:**
        *   [ ] Create a child window (`ImGui::BeginChild`) for the scrolling output. Give it a fixed height (e.g., `window_height - input_height - status_height`).
        *   [ ] Iterate through `output_history` (or use `ImGuiTextBuffer::appendf`) and display each message using `ImGui::TextUnformatted`.
        *   [ ] Add `ImGui::SetScrollHereY(1.0f)` if new output was added to auto-scroll.
        *   [ ] `ImGui::EndChild()`.
    *   [ ] **Input Area:**
        *   [ ] Use `ImGui::InputText("Input", input_buf, IM_ARRAYSIZE(input_buf), ImGuiInputTextFlags_EnterReturnsTrue)`. Store the return value (true if Enter was pressed).
        *   [ ] Place a "Send" button next to it: `ImGui::SameLine(); bool send_pressed = ImGui::Button("Send");`.
    *   [ ] **Status Bar:**
        *   [ ] Use `ImGui::Separator()` above it.
        *   [ ] Display the `status_text` using `ImGui::Text()`.
    *   [ ] **Input Handling:**
        *   [ ] Check if `send_pressed` or the `InputText` returned true.
        *   [ ] If true and `input_buf` is not empty:
            *   [ ] **(Placeholder)** Print the input to `std::cout` for now: `std::cout << "Input captured: " << input_buf << std::endl;`. (Actual queuing happens in Stage 4).
            *   [ ] Clear the input buffer: `input_buf[0] = '\0';`.
            *   [ ] Set keyboard focus back to the input widget: `ImGui::SetKeyboardFocusHere(-1);` (call this *before* `InputText` in the next frame).
*   [ ] **(Placeholder) Update `GuiInterface` Display Methods:**
    *   [ ] Modify `displayOutput`, `displayError`, `displayStatus` to temporarily add the received string directly to `output_history` or update `status_text`. **Note:** This is NOT thread-safe yet and only for basic testing. Proper queuing will be implemented in Stage 4.
    *   [ ] Example (temporary, non-thread-safe):
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
*   [ ] **Verify Build & Run:**
    *   [ ] Build the project.
    *   [ ] Run `./llm-gui`.
    *   [ ] Verify the layout: scrolling output area, input box, send button, status bar.
    *   [ ] Type text in the input box and press Enter or click "Send". Verify the text is printed to the console (placeholder action) and the input box clears.
    *   [ ] (Optional) Manually call `gui_ui.displayOutput("Test output");` or `gui_ui.displayStatus("Test status");` from `main_gui.cpp` before the loop to see if they appear correctly in the GUI (this bypasses threading).
