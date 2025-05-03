# Stage 2: GUI Dependencies & Basic Window

## Goal

Integrate the chosen GUI libraries (ImGui, GLFW) and their dependencies (OpenGL) into the build system. Create the basic structure for the GUI interface (`GuiInterface`) and implement the initial window setup and shutdown logic.

## Prerequisites

*   Completion of Stage 1 (Project Structure & Build System Refactoring).

## Steps & Checklist

*   [x] **Update `CMakeLists.txt`:**
    *   [x] **Find/Fetch GUI Dependencies:**
        *   [x] Add `find_package(OpenGL REQUIRED)`.
        *   [x] Add `find_package(glfw3 REQUIRED)` or use `FetchContent` to get GLFW.
        *   [x] Use `FetchContent` to get ImGui (including necessary backend files: `imgui_impl_glfw.cpp`, `imgui_impl_opengl3.cpp`). Ensure `imgui.cpp`, `imgui_draw.cpp`, `imgui_tables.cpp`, `imgui_widgets.cpp` are included.
        *   [x] Define an `INTERFACE` library target for ImGui (e.g., `imgui_lib`) to manage its sources and include directories.
    *   [x] **Add GUI Executable Target:**
        *   [x] Define a new executable target `llm-gui`.
        *   [x] Add placeholder source files `main_gui.cpp` and `gui_interface/gui_interface.cpp` to this target (files will be created below).
        *   [x] Link `llm-gui` against `llm_core`, `imgui_lib`, `glfw`, `OpenGL::GL`, and `Threads::Threads`.
        *   [x] Set necessary include directories for `llm-gui` (e.g., from `llm_core`, ImGui, GLFW).
    *   [x] **Update Installation:**
        *   [x] Add `llm-gui` to the `install(TARGETS ...)` command.
*   [x] **Create GUI Interface Skeleton:**
    *   [x] Create directory `gui_interface/`.
    *   [x] Create `gui_interface/gui_interface.h`:
        *   [x] Include `ui_interface.h`.
        *   [x] Forward declare `GLFWwindow`.
        *   [x] Define `GuiInterface` inheriting from `UserInterface`.
        *   [x] Declare all virtual methods from `UserInterface` with `override`.
        *   [x] Add a private member `GLFWwindow* window = nullptr;`.
        *   [x] Add placeholder private members for thread-safe queues/data later (e.g., `std::mutex`, `std::queue`).
    *   [x] Create `gui_interface/gui_interface.cpp`:
        *   [x] Include `gui_interface.h`, `<stdexcept>`, `<GLFW/glfw3.h>`, `<imgui.h>`, `<backends/imgui_impl_glfw.h>`, `<backends/imgui_impl_opengl3.h>`.
        *   [x] Implement `GuiInterface::initialize()`:
            *   [x] Initialize GLFW (`glfwInit`).
            *   [x] Set GLFW window hints (e.g., OpenGL version 3.3 Core Profile).
            *   [x] Create GLFW window (`glfwCreateWindow`). Store handle in `window`.
            *   [x] Make window's context current (`glfwMakeContextCurrent`).
            *   [x] Initialize OpenGL loader (e.g., Glad, or rely on GLFW extensions if sufficient).
            *   [x] Setup Dear ImGui context (`ImGui::CreateContext`).
            *   [x] Setup ImGui style (e.g., `ImGui::StyleColorsDark`).
            *   [x] Setup Platform/Renderer backends (`ImGui_ImplGlfw_InitForOpenGL`, `ImGui_ImplOpenGL3_Init`).
            *   [x] Load fonts (e.g., `ImGui::GetIO().Fonts->AddFontDefault()`).
        *   [x] Implement `GuiInterface::shutdown()`:
            *   [x] Cleanup ImGui backends (`ImGui_ImplOpenGL3_Shutdown`, `ImGui_ImplGlfw_Shutdown`).
            *   [x] Destroy ImGui context (`ImGui::DestroyContext`).
            *   [x] Destroy GLFW window (`glfwDestroyWindow`).
            *   [x] Terminate GLFW (`glfwTerminate`).
        *   [x] Implement stub versions of other `UserInterface` methods (`promptUserInput`, `displayOutput`, `displayError`, `displayStatus`) - they don't need to do anything functional yet. `promptUserInput` can return `std::nullopt`.
*   [x] **Create GUI Main Entry Point:**
    *   [x] Create `main_gui.cpp`:
        *   [x] Include `gui_interface.h`, `<stdexcept>`, `<iostream>`.
        *   [x] `main` function:
            *   [x] Instantiate `GuiInterface gui_ui;`.
            *   [x] Wrap in `try...catch` block.
            *   [x] Call `gui_ui.initialize()`.
            *   [x] **Basic Render Loop:**
                *   [x] `while (!glfwWindowShouldClose(window))` loop (get `window` via a public getter in `GuiInterface` or pass it).
                *   [x] Poll events (`glfwPollEvents`).
                *   [x] Start ImGui frame (`ImGui_ImplOpenGL3_NewFrame`, `ImGui_ImplGlfw_NewFrame`, `ImGui::NewFrame`).
                *   [x] **Placeholder ImGui Window:** `ImGui::Begin("LLM-GUI"); ImGui::Text("Window Ready!"); ImGui::End();`
                *   [x] Render ImGui (`ImGui::Render`, `glfwGetFramebufferSize`, `glViewport`, `glClearColor`, `glClear`, `ImGui_ImplOpenGL3_RenderDrawData`).
                *   [x] Swap buffers (`glfwSwapBuffers`).
            *   [x] Call `gui_ui.shutdown()`.
            *   [x] Catch exceptions, display error (using `std::cerr` for now), return 1.
            *   [x] Return 0 on success.
*   [x] **Verify Build & Run:**
    *   [x] Run CMake configuration (`cmake ..`).
    *   [x] Build the project (`make` or `cmake --build .`).
    *   [x] Confirm the `llm-gui` executable is created.
    *   [x] Run `./llm-gui`. A window should appear with the text "Window Ready!". Closing the window should exit the application cleanly.
