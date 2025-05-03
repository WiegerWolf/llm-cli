#include "gui_interface/gui_interface.h"
#include <stdexcept>
#include <iostream>
#include <vector>
#include <string>

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

        // --- Main UI Layout (Stage 3) ---
        const ImVec2 display_size = ImGui::GetIO().DisplaySize;
        const float input_height = 35.0f; // Height for the input text box + button
        const float status_height = 25.0f; // Height for the status bar
        const float output_height = display_size.y - input_height - status_height; // Remaining height for output

        // Create a full-window container
        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
        ImGui::SetNextWindowSize(display_size);
        ImGui::Begin("Main", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus);

        // --- Output Area ---
        ImGui::BeginChild("Output", ImVec2(0, output_height), true, ImGuiWindowFlags_HorizontalScrollbar);
        const auto& history = gui_ui.getOutputHistory(); // Get history via getter
        for (const auto& line : history) {
            ImGui::TextUnformatted(line.c_str());
        }
        // Auto-scroll placeholder
        static size_t prev_history_size = 0;
        if (history.size() != prev_history_size) {
            if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 10.0f) // Only scroll if already near the bottom
            {
                 ImGui::SetScrollHereY(1.0f); // 1.0f scrolls to the bottom
            }
            prev_history_size = history.size();
        }
        ImGui::EndChild(); // End Output Area

        // --- Input Area ---
        bool enter_pressed = false;
        bool send_pressed = false;
        ImGui::PushItemWidth(display_size.x - 60.0f); // Leave space for the button (adjust 60.0f as needed)
        enter_pressed = ImGui::InputText("##Input", gui_ui.getInputBuffer(), gui_ui.getInputBufferSize(), ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::PopItemWidth();
        ImGui::SameLine();
        send_pressed = ImGui::Button("Send");

        // --- Status Bar ---
        ImGui::Separator();
        ImGui::TextUnformatted(gui_ui.getStatusText().c_str()); // Get status via getter

        // --- Input Handling ---
        if (send_pressed || enter_pressed) {
            char* input_buf = gui_ui.getInputBuffer();
            if (input_buf[0] != '\0') {
                // Placeholder: Just print to console for now
                std::cout << "Input captured: " << input_buf << std::endl;

                // TODO: In Stage 4, this input needs to be sent to the worker thread
                // gui_ui.submitInput(input_buf); // Example for Stage 4

                // Clear the buffer after processing
                input_buf[0] = '\0';
            }
            // Set focus back to the input field for the next input
            ImGui::SetKeyboardFocusHere(-1); // -1 means previous item (the InputText)
        }

        ImGui::End(); // End Main Window
        // --- End Main UI Layout ---


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
        // if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        // {
        //     GLFWwindow* backup_current_context = glfwGetCurrentContext();
        //     ImGui::UpdatePlatformWindows();
        //     ImGui::RenderPlatformWindowsDefault();
        //     glfwMakeContextCurrent(backup_current_context);
        // }


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
