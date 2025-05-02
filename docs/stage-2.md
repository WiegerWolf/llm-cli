# Stage 2: GUI Dependencies & Basic Window

## Goal

Integrate the chosen GUI libraries (ImGui, GLFW) and their dependencies (OpenGL) into the build system. Create the basic structure for the GUI interface (`GuiInterface`) and implement the initial window setup and shutdown logic.

## Prerequisites

*   Completion of Stage 1 (Project Structure & Build System Refactoring).

## Steps & Checklist

*   [ ] **Update `CMakeLists.txt`:**
    *   [ ] **Find/Fetch GUI Dependencies:**
        *   [ ] Add `find_package(OpenGL REQUIRED)`.
        *   [ ] Add `find_package(glfw3 REQUIRED)` or use `FetchContent` to get GLFW.
        *   [ ] Use `FetchContent` to get ImGui (including necessary backend files: `imgui_impl_glfw.cpp`, `imgui_impl_opengl3.cpp`). Ensure `imgui.cpp`, `imgui_draw.cpp`, `imgui_tables.cpp`, `imgui_widgets.cpp` are included.
        *   [ ] Define an `INTERFACE` library target for ImGui (e.g., `imgui_lib`) to manage its sources and include directories.
    *   [ ] **Add GUI Executable Target:**
        *   [ ] Define a new executable target `llm-gui`.
        *   [ ] Add placeholder source files `main_gui.cpp` and `gui_interface/gui_interface.cpp` to this target (files will be created below).
        *   [ ] Link `llm-gui` against `llm_core`, `imgui_lib`, `glfw`, `OpenGL::GL`, and `Threads::Threads`.
        *   [ ] Set necessary include directories for `llm-gui` (e.g., from `llm_core`, ImGui, GLFW).
    *   [ ] **Update Installation:**
        *   [ ] Add `llm-gui` to the `install(TARGETS ...)` command.
*   [ ] **Create GUI Interface Skeleton:**
    *   [ ] Create directory `gui_interface/`.
    *   [ ] Create `gui_interface/gui_interface.h`:
        *   [ ] Include `ui_interface.h`.
        *   [ ] Forward declare `GLFWwindow`.
        *   [ ] Define `GuiInterface` inheriting from `UserInterface`.
        *   [ ] Declare all virtual methods from `UserInterface` with `override`.
        *   [ ] Add a private member `GLFWwindow* window = nullptr;`.
        *   [ ] Add placeholder private members for thread-safe queues/data later (e.g., `std::mutex`, `std::queue`).
    *   [ ] Create `gui_interface/gui_interface.cpp`:
        *   [ ] Include `gui_interface.h`, `<stdexcept>`, `<GLFW/glfw3.h>`, `<imgui.h>`, `<backends/imgui_impl_glfw.h>`, `<backends/imgui_impl_opengl3.h>`.
        *   [ ] Implement `GuiInterface::initialize()`:
            *   [ ] Initialize GLFW (`glfwInit`).
            *   [ ] Set GLFW window hints (e.g., OpenGL version 3.3 Core Profile).
            *   [ ] Create GLFW window (`glfwCreateWindow`). Store handle in `window`.
            *   [ ] Make window's context current (`glfwMakeContextCurrent`).
            *   [ ] Initialize OpenGL loader (e.g., Glad, or rely on GLFW extensions if sufficient).
            *   [ ] Setup Dear ImGui context (`ImGui::CreateContext`).
            *   [ ] Setup ImGui style (e.g., `ImGui::StyleColorsDark`).
            *   [ ] Setup Platform/Renderer backends (`ImGui_ImplGlfw_InitForOpenGL`, `ImGui_ImplOpenGL3_Init`).
            *   [ ] Load fonts (e.g., `ImGui::GetIO().Fonts->AddFontDefault()`).
        *   [ ] Implement `GuiInterface::shutdown()`:
            *   [ ] Cleanup ImGui backends (`ImGui_ImplOpenGL3_Shutdown`, `ImGui_ImplGlfw_Shutdown`).
            *   [ ] Destroy ImGui context (`ImGui::DestroyContext`).
            *   [ ] Destroy GLFW window (`glfwDestroyWindow`).
            *   [ ] Terminate GLFW (`glfwTerminate`).
        *   [ ] Implement stub versions of other `UserInterface` methods (`promptUserInput`, `displayOutput`, `displayError`, `displayStatus`) - they don't need to do anything functional yet. `promptUserInput` can return `std::nullopt`.
*   [ ] **Create GUI Main Entry Point:**
    *   [ ] Create `main_gui.cpp`:
        *   [ ] Include `gui_interface.h`, `<stdexcept>`, `<iostream>`.
        *   [ ] `main` function:
            *   [ ] Instantiate `GuiInterface gui_ui;`.
            *   [ ] Wrap in `try...catch` block.
            *   [ ] Call `gui_ui.initialize()`.
            *   [ ] **Basic Render Loop:**
                *   [ ] `while (!glfwWindowShouldClose(window))` loop (get `window` via a public getter in `GuiInterface` or pass it).
                *   [ ] Poll events (`glfwPollEvents`).
                *   [ ] Start ImGui frame (`ImGui_ImplOpenGL3_NewFrame`, `ImGui_ImplGlfw_NewFrame`, `ImGui::NewFrame`).
                *   [ ] **Placeholder ImGui Window:** `ImGui::Begin("LLM-GUI"); ImGui::Text("Window Ready!"); ImGui::End();`
                *   [ ] Render ImGui (`ImGui::Render`, `glfwGetFramebufferSize`, `glViewport`, `glClearColor`, `glClear`, `ImGui_ImplOpenGL3_RenderDrawData`).
                *   [ ] Swap buffers (`glfwSwapBuffers`).
            *   [ ] Call `gui_ui.shutdown()`.
            *   [ ] Catch exceptions, display error (using `std::cerr` for now), return 1.
            *   [ ] Return 0 on success.
*   [ ] **Verify Build & Run:**
    *   [ ] Run CMake configuration (`cmake ..`).
    *   [ ] Build the project (`make` or `cmake --build .`).
    *   [ ] Confirm the `llm-gui` executable is created.
    *   [ ] Run `./llm-gui`. A window should appear with the text "Window Ready!". Closing the window should exit the application cleanly.
