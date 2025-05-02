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
    *   Define the existing executable target as `llm-cli`, using `main_cli.cpp` (renamed from `main.cpp`) and linking only necessary CLI libraries (Readline, etc.).
    *   Add a new executable target `llm-gui`.
    *   Use `FetchContent` or `find_package` to locate/acquire **GLFW** and **ImGui**. Integrate ImGui sources (including necessary backends like `imgui_impl_glfw.cpp`, `imgui_impl_opengl3.cpp`).
    *   Find **OpenGL** libraries (`find_package(OpenGL REQUIRED)`).
    *   Add `main_gui.cpp` and `gui_interface/gui_interface.cpp` to the `llm-gui` target's sources.
    *   Link `llm-gui` against the core logic library (see below), `GuiInterface`, ImGui, GLFW, OpenGL, and Threads.
    *   **Recommendation:** Refactor the core logic (`ChatClient`, `ToolManager`, `PersistenceManager`, tool implementations) into a static or shared library (`llm_core`) that both `llm-cli` and `llm-gui` can link against. This avoids compiling the core logic twice and promotes modularity.

### 2.4. Implement `GuiInterface`

*   Implement all virtual methods defined in `UserInterface`:
    *   `initialize()`: Initialize GLFW, create a GLFW window, initialize the OpenGL context, initialize ImGui (including GLFW and OpenGL backends), load fonts. Store the GLFW window handle.
    *   `shutdown()`: Clean up ImGui backends, destroy the ImGui context, destroy the GLFW window, terminate GLFW.
    *   `promptUserInput()`: **This method will likely not be used directly in the GUI flow.** User input will be captured via an `ImGui::InputText` widget. A separate mechanism (e.g., a "Send" button) will trigger the processing of this input. See Section 2.5.
    *   `displayOutput(const std::string& output)`: Append the `output` string to a thread-safe buffer (e.g., using `std::mutex`). The GUI render loop will read from this buffer and display it in an ImGui widget (e.g., a scrolling `ImGui::BeginChild` window with `ImGui::TextUnformatted`).
    *   `displayError(const std::string& error)`: Append the `error` string to a separate thread-safe buffer or the main output buffer, possibly prefixed or styled differently. Display in the GUI render loop.
    *   `displayStatus(const std::string& status)`: Update a thread-safe string variable. The GUI render loop will read this variable and display it in a designated status area (e.g., using `ImGui::Text`).
*   **Threading:** This is critical.
    *   The main thread will run the GLFW/ImGui event and rendering loop.
    *   All `ChatClient` operations (`processTurn`, API calls, tool execution) **must** run on a separate worker thread to avoid blocking the GUI.
    *   Use thread-safe queues (e.g., a custom implementation or a library like `moodycamel::ConcurrentQueue`) or condition variables with mutexes to pass data between threads:
        *   **GUI -> Worker:** Send user input strings.
        *   **Worker -> GUI:** Send output strings, error messages, and status updates.
    *   The `GuiInterface` methods (`displayOutput`, `displayError`, `displayStatus`) will be called *from the worker thread* and must safely place data into shared buffers/queues for the GUI thread to consume.

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
    *   The `ChatClient::run()` method itself will *not* be called by the GUI executable's worker thread; `processTurn` is invoked directly based on GUI events.

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
