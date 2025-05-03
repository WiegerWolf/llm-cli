#include "gui_interface/gui_interface.h"
#include <stdexcept>
#include <iostream>

// Include GUI library headers needed for the main loop
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <stdio.h> // For glClearColor

int main(int, char**) {
    GuiInterface gui_ui; // Instantiate the GUI interface

    try {
        gui_ui.initialize(); // Initialize GLFW, ImGui, etc.
    } catch (const std::exception& e) {
        std::cerr << "Initialization failed: " << e.what() << std::endl;
        return 1;
    }

    GLFWwindow* window = gui_ui.getWindow(); // Get the window handle
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f); // Background color

    // --- Main Render Loop ---
    while (!glfwWindowShouldClose(window)) {
        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        glfwPollEvents();

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // --- Placeholder ImGui Window ---
        // Simple example window. Replace with actual UI in later stages.
        ImGui::Begin("LLM-GUI");                          // Create a window called "LLM-GUI" and append into it.
        ImGui::Text("Window Ready!");                     // Display some text
        ImGui::End();
        // --- End Placeholder ---


        // Rendering
        ImGui::Render(); // End the ImGui frame and prepare draw data

        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h); // Get window size
        glViewport(0, 0, display_w, display_h); // Set OpenGL viewport
        glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT); // Clear the screen

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData()); // Render ImGui draw data

        // Update and Render additional Platform Windows
        // (If docking/multi-viewports are enabled)
        ImGuiIO& io = ImGui::GetIO();
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            GLFWwindow* backup_current_context = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(backup_current_context);
        }


        glfwSwapBuffers(window); // Swap the front and back buffers
    }

    // --- Cleanup ---
    try {
        gui_ui.shutdown(); // Cleanup ImGui, GLFW
    } catch (const std::exception& e) {
        std::cerr << "Shutdown failed: " << e.what() << std::endl;
        // Continue execution to ensure main returns, but report error.
    }

    return 0; // Success
}
