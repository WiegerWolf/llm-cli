# Stage 4: Threading & Core Logic Integration

## Goal

Implement the worker thread to run the `ChatClient` logic separately from the GUI thread. Establish thread-safe communication between the GUI and worker threads for input, output, errors, and status updates. Connect the GUI interactions (sending input) to the `ChatClient`.

## Prerequisites

*   Completion of Stage 3 (GUI Layout & Basic Interaction).

## Steps & Checklist

*   [ ] **Implement Thread-Safe Queues/Data Structures in `GuiInterface`:**
    *   [ ] Include necessary headers: `<thread>`, `<mutex>`, `<condition_variable>`, `<queue>`, `<atomic>`, `<string>`, `<vector>`, `<utility>`.
    *   [ ] **Input Queue:**
        *   [ ] `std::queue<std::string> input_queue;`
        *   [ ] `std::mutex input_mutex;`
        *   [ ] `std::condition_variable input_cv;`
        *   [ ] `std::atomic<bool> input_ready{false};`
        *   [ ] `std::atomic<bool> shutdown_requested{false};`
    *   [ ] **Display Queue:**
        *   [ ] Define message types: `enum class DisplayMessageType { OUTPUT, ERROR, STATUS };`
        *   [ ] `std::queue<std::pair<std::string, DisplayMessageType>> display_queue;`
        *   [ ] `std::mutex display_mutex;`
*   [ ] **Implement Thread-Safe `GuiInterface` Methods:**
    *   [ ] `displayOutput(const std::string& output)`:
        *   [ ] Lock `display_mutex`.
        *   [ ] Push `{output, DisplayMessageType::OUTPUT}` onto `display_queue`.
        *   [ ] Unlock mutex.
    *   [ ] `displayError(const std::string& error)`:
        *   [ ] Lock `display_mutex`.
        *   [ ] Push `{error, DisplayMessageType::ERROR}` onto `display_queue`.
        *   [ ] Unlock mutex.
    *   [ ] `displayStatus(const std::string& status)`:
        *   [ ] Lock `display_mutex`.
        *   [ ] Push `{status, DisplayMessageType::STATUS}` onto `display_queue`.
        *   [ ] Unlock mutex.
    *   [ ] `promptUserInput()`:
        *   [ ] This method is called by the *worker* thread (`ChatClient::run` or `ChatClient::processTurn` via `promptUserInput`).
        *   [ ] Use a `std::unique_lock<std::mutex> lock(input_mutex);`.
        *   [ ] Wait on `input_cv` until `input_ready` is true or `shutdown_requested` is true: `input_cv.wait(lock, [&]{ return input_ready.load() || shutdown_requested.load(); });`.
        *   [ ] If `shutdown_requested`, return `std::nullopt`.
        *   [ ] If `input_ready`:
            *   [ ] Get input from `input_queue.front()`.
            *   [ ] `input_queue.pop()`.
            *   [ ] Set `input_ready = false;`.
            *   [ ] Return the input string.
    *   [ ] Add a method `requestShutdown()`:
        *   [ ] Set `shutdown_requested = true;`.
        *   [ ] Notify `input_cv.notify_one();`.
    *   [ ] Add a method `sendInputToWorker(const std::string& input)`:
        *   [ ] Lock `input_mutex`.
        *   [ ] Push `input` onto `input_queue`.
        *   [ ] Set `input_ready = true;`.
        *   [ ] Unlock mutex.
        *   [ ] Notify `input_cv.notify_one();`.
*   [ ] **Update `main_gui.cpp`:**
    *   [ ] **Worker Thread Management:**
        *   [ ] Include `<thread>`.
        *   [ ] After `gui_ui.initialize()`, create `ChatClient client(gui_ui);`.
        *   [ ] Launch the worker thread: `std::thread worker_thread([&client]() { try { client.run(); } catch (const std::exception& e) { /* Handle worker thread exceptions - maybe push to display queue */ } });`.
    *   [ ] **GUI Loop - Input Handling:**
        *   [ ] In the "Send" button/Enter key handler:
            *   [ ] Instead of printing to console, call `gui_ui.sendInputToWorker(input_buf);`.
            *   [ ] Keep the input buffer clearing and focus setting logic.
    *   [ ] **GUI Loop - Display Update Handling:**
        *   [ ] Add a section in the loop (outside ImGui frame rendering logic, but before `ImGui::Render`):
        *   [ ] Lock `display_mutex` (using `gui_ui`'s mutex, maybe via a getter or friend class).
        *   [ ] While `display_queue` is not empty:
            *   [ ] Get the `pair<string, DisplayMessageType>` from the front.
            *   [ ] Pop the queue.
            *   [ ] Based on the `DisplayMessageType`, append to `output_history` (for OUTPUT/ERROR) or update `status_text` (for STATUS). Add prefixes like "Error: " for errors if desired.
            *   [ ] Set a flag `new_output_added = true;` if output/error was added.
        *   [ ] Unlock `display_mutex`.
        *   [ ] **Output Area Update:** If `new_output_added`, call `ImGui::SetScrollHereY(1.0f)` inside the output child window. Reset the flag.
    *   [ ] **Shutdown Handling:**
        *   [ ] Before calling `gui_ui.shutdown()`:
            *   [ ] Call `gui_ui.requestShutdown();`.
            *   [ ] Join the worker thread: `worker_thread.join();`.
*   [ ] **Adapt `ChatClient::run`:**
    *   [ ] Ensure `ChatClient::run` correctly uses `promptUserInput()` which now blocks and waits for the GUI thread via the `GuiInterface` implementation.
    *   [ ] Ensure `ChatClient::processTurn` calls `displayOutput`, `displayError`, `displayStatus` via the `ui` reference, which now queue messages for the GUI thread.
*   [ ] **Verify Build & Run:**
    *   [ ] Build the project.
    *   [ ] Run `./llm-gui`.
    *   [ ] Verify that typing input and sending it results in the `ChatClient` processing it (API calls should happen).
    *   [ ] Verify that output, errors, and status messages from the `ChatClient` (including tool execution status) appear correctly in the GUI's output area and status bar.
    *   [ ] Verify the application exits cleanly when the window is closed (worker thread joins).
