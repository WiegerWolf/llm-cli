# Stage 4: Threading & Core Logic Integration

## Goal

Implement the worker thread to run the `ChatClient` logic separately from the GUI thread. Establish thread-safe communication between the GUI and worker threads for input, output, errors, and status updates. Connect the GUI interactions (sending input) to the `ChatClient`.

## Prerequisites

*   Completion of Stage 3 (GUI Layout & Basic Interaction).

## Steps & Checklist

*   [x] **Implement Thread-Safe Queues/Data Structures in `GuiInterface`:**
    *   [x] Include necessary headers: `<thread>`, `<mutex>`, `<condition_variable>`, `<queue>`, `<atomic>`, `<string>`, `<vector>`, `<utility>`.
    *   [x] **Input Queue:**
        *   [x] `std::queue<std::string> input_queue;`
        *   [x] `std::mutex input_mutex;`
        *   [x] `std::condition_variable input_cv;`
        *   [x] `std::atomic<bool> input_ready{false};`
        *   [x] `std::atomic<bool> shutdown_requested{false};`
    *   [x] **Display Queue:**
        *   [x] Define message types: `enum class DisplayMessageType { OUTPUT, ERROR, STATUS };`
        *   [x] `std::queue<std::pair<std::string, DisplayMessageType>> display_queue;`
        *   [x] `std::mutex display_mutex;`
*   [x] **Implement Thread-Safe `GuiInterface` Methods:**
    *   [x] `displayOutput(const std::string& output)`:
        *   [x] Lock `display_mutex`.
        *   [x] Push `{output, DisplayMessageType::OUTPUT}` onto `display_queue`.
        *   [x] Unlock mutex.
    *   [x] `displayError(const std::string& error)`:
        *   [x] Lock `display_mutex`.
        *   [x] Push `{error, DisplayMessageType::ERROR}` onto `display_queue`.
        *   [x] Unlock mutex.
    *   [x] `displayStatus(const std::string& status)`:
        *   [x] Lock `display_mutex`.
        *   [x] Push `{status, DisplayMessageType::STATUS}` onto `display_queue`.
        *   [x] Unlock mutex.
    *   [x] `promptUserInput()`:
        *   [x] This method is called by the *worker* thread (`ChatClient::run` or `ChatClient::processTurn` via `promptUserInput`).
        *   [x] Use a `std::unique_lock<std::mutex> lock(input_mutex);`.
        *   [x] Wait on `input_cv` until `input_ready` is true or `shutdown_requested` is true: `input_cv.wait(lock, [&]{ return input_ready.load() || shutdown_requested.load(); });`.
        *   [x] If `shutdown_requested`, return `std::nullopt`.
        *   [x] If `input_ready`:
            *   [x] Get input from `input_queue.front()`.
            *   [x] `input_queue.pop()`.
            *   [x] Set `input_ready = false;`.
            *   [x] Return the input string.
    *   [x] Add a method `requestShutdown()`:
        *   [x] Set `shutdown_requested = true;`.
        *   [x] Notify `input_cv.notify_one();`.
    *   [x] Add a method `sendInputToWorker(const std::string& input)`:
        *   [x] Lock `input_mutex`.
        *   [x] Push `input` onto `input_queue`.
        *   [x] Set `input_ready = true;`.
        *   [x] Unlock mutex.
        *   [x] Notify `input_cv.notify_one();`.
*   [x] **Update `main_gui.cpp`:**
    *   [x] **Worker Thread Management:**
        *   [x] Include `<thread>`.
        *   [x] After `gui_ui.initialize()`, create `ChatClient client(gui_ui);`.
        *   [x] Launch the worker thread: `std::thread worker_thread([&client]() { try { client.run(); } catch (const std::exception& e) { /* Handle worker thread exceptions - maybe push to display queue */ } });`.
    *   [x] **GUI Loop - Input Handling:**
        *   [x] In the "Send" button/Enter key handler:
            *   [x] Instead of printing to console, call `gui_ui.sendInputToWorker(input_buf);`.
            *   [x] Keep the input buffer clearing and focus setting logic.
    *   [x] **GUI Loop - Display Update Handling:**
        *   [x] Add a section in the loop (outside ImGui frame rendering logic, but before `ImGui::Render`):
        *   [x] Lock `display_mutex` (using `gui_ui`'s mutex, maybe via a getter or friend class).
        *   [x] While `display_queue` is not empty:
            *   [x] Get the `pair<string, DisplayMessageType>` from the front.
            *   [x] Pop the queue.
            *   [x] Based on the `DisplayMessageType`, append to `output_history` (for OUTPUT/ERROR) or update `status_text` (for STATUS). Add prefixes like "Error: " for errors if desired.
            *   [x] Set a flag `new_output_added = true;` if output/error was added.
        *   [x] Unlock `display_mutex`.
        *   [x] **Output Area Update:** If `new_output_added`, call `ImGui::SetScrollHereY(1.0f)` inside the output child window. Reset the flag.
    *   [x] **Shutdown Handling:**
        *   [x] Before calling `gui_ui.shutdown()`:
            *   [x] Call `gui_ui.requestShutdown();`.
            *   [x] Join the worker thread: `worker_thread.join();`.
*   [x] **Adapt `ChatClient::run`:**
    *   [x] Ensure `ChatClient::run` correctly uses `promptUserInput()` which now blocks and waits for the GUI thread via the `GuiInterface` implementation.
    *   [x] Ensure `ChatClient::processTurn` calls `displayOutput`, `displayError`, `displayStatus` via the `ui` reference, which now queue messages for the GUI thread.
*   [x] **Verify Build & Run:**
    *   [x] Build the project.
    *   [x] Run `./llm-gui`.
    *   [x] Verify that typing input and sending it results in the `ChatClient` processing it (API calls should happen).
    *   [x] Verify that output, errors, and status messages from the `ChatClient` (including tool execution status) appear correctly in the GUI's output area and status bar.
    *   [x] Verify the application exits cleanly when the window is closed (worker thread joins).
