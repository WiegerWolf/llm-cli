# High-Level Plan: Adding a GUI Frontend

## 1. Goal

The primary goal is to add a graphical user interface (GUI) as an alternative frontend to the existing command-line interface (CLI). Both interfaces should coexist and utilize the core application logic encapsulated in `ChatClient` and associated tools. The existing `UserInterface` abstraction will be leveraged to minimize changes to the core logic.

## 2. Key Steps

### 2.1. Choose a GUI Toolkit

*   **Decision:** Use **ImGui** (Dear ImGui) for the UI elements and **GLFW** for windowing and input handling. An OpenGL backend (e.g., OpenGL 3.3+) will be used for rendering ImGui.
*   **Rationale:** This combination is chosen for its lightweight nature, low runtime resource usage, relatively simple integration, immediate mode rendering paradigm (which can simplify state management), and good cross-platform support.

### 2.2. Update Project Structure

*   Create a new directory `gui_interface/` for GUI-specific code.
*   Implement a new class `GuiInterface` in `gui_interface/gui_interface.h` and `gui_interface/gui_interface.cpp`, derived from `UserInterface`.
*   Integrate ImGui and GLFW libraries, preferably using CMake's `FetchContent` or relying on system-provided packages found via `find_package`. Avoid git submodules.
*   Create two separate main entry points:
    *   `main_cli.cpp` (renamed from `main.cpp`): For the `llm-cli` executable, using `CliInterface`.
    *   `main_gui.cpp`: For the `llm-gui` executable, using `GuiInterface`.

### 2.3. Adapt Build System (CMake)

*   Modify `CMakeLists.txt` to:
    *   **Refactor Core Logic:** Create a static library target (e.g., `llm_core`). Move the core logic source files (`chat_client.cpp`, `database.cpp`, `tools.cpp`, `curl_utils.cpp` (if created), and all `tools_impl/*.cpp`) from the main executable definition to this library target. Header files (`.h`) should generally not be listed as sources for library or executable targets.
    *   **Update CLI Target:** Rename the existing executable target from `llm` to `llm-cli`. Update its sources to include only `main_cli.cpp` (renamed from `main.cpp`) and `cli_interface.cpp`. Link `llm-cli` against the `llm_core` library and necessary CLI libraries (Readline, etc.).
    *   **Add GUI Target:** Add a new executable target `llm-gui`. Its sources should include `main_gui.cpp` and `gui_interface/gui_interface.cpp`.
    *   **Find/Fetch GUI Dependencies:** Use `FetchContent` (preferred, similar to nlohmann_json) or `find_package` to locate/acquire **GLFW** and **ImGui**. Integrate ImGui sources (including necessary backends like `imgui_impl_glfw.cpp`, `imgui_impl_opengl3.cpp`).
    *   **Find OpenGL:** Find **OpenGL** libraries (e.g., `find_package(OpenGL REQUIRED)`).
    *   **Link GUI Target:** Link `llm-gui` against the `llm_core` library, ImGui, GLFW, OpenGL, and Threads. Ensure necessary include directories are set for both targets.

### 2.4. Implement `GuiInterface`

*   Implement all virtual methods defined in `UserInterface`:
    *   `initialize()`: Initialize GLFW, create a GLFW window, initialize the OpenGL context, initialize ImGui (including GLFW and OpenGL backends), load fonts. Store the GLFW window handle.
    *   `shutdown()`: Clean up ImGui backends, destroy the ImGui context, destroy the GLFW window, terminate GLFW.
    *   `promptUserInput()`: **This method will likely not be used directly in the GUI flow.** User input will be captured via an `ImGui::InputText` widget. A separate mechanism (e.g., a "Send" button click or Enter key press in the input widget) will trigger the processing of this input. See Section 2.5.
    *   `displayOutput(const std::string& output)`, `displayError(const std::string& error)`, `displayStatus(const std::string& status)`: These methods will be called *from the worker thread*. They must be implemented to be thread-safe. They should push the received strings onto thread-safe queues (one for output/error, one for status, or a single queue with message types). The GUI thread will then periodically check these queues (e.g., using a `std::mutex` for locking) and display the content in appropriate ImGui widgets (e.g., a scrolling text area for output/error, a status bar label for status).
*   **Threading:** This is critical.
    *   The main thread runs the GLFW/ImGui event and rendering loop.
    *   All `ChatClient` operations (`processTurn`, API calls, tool execution via `ToolManager`) **must** run on a separate worker thread (`std::thread`) to avoid blocking the GUI.
    *   Use standard C++ synchronization primitives:
        *   `std::mutex` to protect shared data structures (like the output/status queues).
        *   `std::condition_variable` to signal between threads (e.g., GUI thread signals worker that new input is available, worker signals GUI that new output is ready - although polling the queues from the GUI thread might be simpler).
        *   Thread-safe queues for passing data (user input string, output/error strings, status strings). A simple queue protected by a `std::mutex` is often sufficient.
    *   The `GuiInterface` methods (`displayOutput`, `displayError`, `displayStatus`) are the bridge: called by the worker thread, they enqueue data for the GUI thread.

### 2.5. Adapt Core Logic Integration

*   The `ChatClient` constructor accepting `UserInterface&` remains valid for both interfaces.
*   The `llm-cli` executable (using `main_cli.cpp`) will instantiate `CliInterface` and call `ChatClient::run()` as it does now.
*   The `llm-gui` executable (using `main_gui.cpp`) will:
    *   Instantiate `GuiInterface`.
    *   Initialize the GUI environment (`GuiInterface::initialize()`).
    *   Start the worker thread for `ChatClient` operations.
    *   Enter the main GUI loop (GLFW/ImGui event loop):
        *   Poll GLFW events.
        *   Start a new ImGui frame.
        *   Render the ImGui UI (input box, output area, status bar, "Send" button).
    *   **On "Send" button click:**
        *   Retrieve text from the `ImGui::InputText` buffer.
        *   Clear the input buffer.
        *   Push the retrieved text onto the thread-safe queue for the worker thread.
    *   Check the thread-safe queues/buffers for new output/error/status messages from the worker thread and display them using ImGui widgets.
    *   End the ImGui frame and render it.
    *   Swap GLFW buffers.
*   The worker thread (launched by `main_gui.cpp` or `GuiInterface::initialize`) will:
    *   Instantiate `ChatClient`, passing the `GuiInterface` instance.
    *   Wait for input to appear on the input queue (sent from the GUI thread).
    *   When input is received, call `chatClient.processTurn(input)`.
    *   `processTurn` will execute, calling the `GuiInterface` methods (`displayOutput`, etc.) which will push results onto the output queues/buffers for the GUI thread to display.
    *   The `ChatClient::run()` method (which contains the main loop in the CLI version) will *not* be called by the GUI executable's worker thread. Instead, the worker thread will likely have its own loop that waits for input from the GUI thread's queue (using a condition variable or similar), and upon receiving input, calls `chatClient.processTurn(input)`.

### 2.6. Packaging and Installation

*   Update installation scripts (`install.sh`, CMake `install` commands) to:
    *   Include GLFW runtime libraries if they are dynamically linked and not expected to be system-provided.
    *   Include any necessary font files if not embedded.
    *   Ensure the build process correctly links OpenGL.

## 3. Next Steps

*   Implement the CMake changes (`CMakeLists.txt`) to support the `BUILD_GUI` option, find GLFW/OpenGL, and include ImGui.
*   Create the basic `gui_interface/` directory and skeleton `GuiInterface` class.
*   Implement the basic GLFW/ImGui window setup and main loop structure in `main.cpp` (conditionally compiled).
*   Set up the worker thread and basic thread-safe communication mechanism (e.g., simple queues with mutexes).
