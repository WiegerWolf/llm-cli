# High-Level Plan: Adding a GUI Frontend

## 1. Goal

The primary goal is to add a graphical user interface (GUI) as an alternative frontend to the existing command-line interface (CLI). Both interfaces should coexist and utilize the core application logic encapsulated in `ChatClient` and associated tools. The existing `UserInterface` abstraction will be leveraged to minimize changes to the core logic.

## 2. GUI Toolkit Choice

*   **Decision:** Use **ImGui** (Dear ImGui) for the UI elements and **GLFW** for windowing and input handling. An OpenGL backend (e.g., OpenGL 3.3+) will be used for rendering ImGui.
*   **Rationale:** Lightweight, low resource usage, simple integration, immediate mode rendering, good cross-platform support.

## 3. Development Stages

The implementation is broken down into the following stages. See the corresponding `stage-X.md` file for detailed steps and checklists for each stage.

*   **[Stage 1: Project Structure & Build System Refactoring](./stage-1.md)**
    *   Goal: Refactor CMake and project layout for separate CLI/GUI targets and a core library.
*   **[Stage 2: GUI Dependencies & Basic Window](./stage-2.md)**
    *   Goal: Integrate ImGui, GLFW, OpenGL; create the basic `GuiInterface` and window setup.
*   **[Stage 3: GUI Layout & Basic Interaction](./stage-3.md)**
    *   Goal: Implement ImGui layout (input, output, status, button) and handle input capture.
*   **[Stage 4: Threading & Core Logic Integration](./stage-4.md)**
    *   Goal: Implement worker thread, thread-safe communication, and connect GUI to `ChatClient`.
*   **[Stage 5: Packaging, Installation & Refinement](./stage-5.md)**
    *   Goal: Finalize build/installation, test thoroughly, and refine the implementation.

## 4. Overview of Key Technical Aspects (Details in Stages)

*   **Build System (CMake):** Refactor into `llm_core` (static library), `llm-cli` (executable), `llm-gui` (executable). Use `FetchContent` for dependencies like ImGui and potentially GLFW.
*   **Project Structure:** Introduce `gui_interface/` for `GuiInterface` implementation and `main_gui.cpp`. Rename `main.cpp` to `main_cli.cpp`.
*   **`GuiInterface` Implementation:** Derive from `UserInterface`. Implement methods for initialization, shutdown, and thread-safe display updates (`displayOutput`, `displayError`, `displayStatus`). `promptUserInput` will block the worker thread until the GUI thread provides input.
*   **Threading:** GUI thread runs the ImGui/GLFW loop. A separate worker thread runs `ChatClient::run` (or a similar loop calling `processTurn`). Communication uses thread-safe queues (`std::queue` + `std::mutex`) and signaling (`std::condition_variable`).
*   **Core Logic Integration:** `main_gui.cpp` starts the worker thread. GUI input is passed to the worker via a queue. Worker calls `GuiInterface` display methods, which queue updates for the GUI thread.
*   **Packaging:** Update installation (`CMakeLists.txt`, `install.sh`) for both executables and potential runtime dependencies (fonts, GLFW libs if needed).
