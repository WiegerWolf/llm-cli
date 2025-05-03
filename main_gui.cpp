#include "gui_interface/gui_interface.h"
#include <stdexcept>
#include <iostream>
#include <vector>
#include <string>
#include <thread> // Added for Stage 4
#include "chat_client.h" // Added for Stage 4

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

    // --- Worker Thread Setup (Stage 4) ---
    ChatClient client(gui_ui); // Create the client, passing the UI interface
    // Use jthread with RAII: construct in-place with lambda, joins automatically on destruction
    // Pass the stop_token provided by jthread to the lambda and client.run()
    std::jthread worker_thread([&client](std::stop_token st){
        try {
            client.run(st); // Pass the stop token to the client's run loop
        } catch (const std::exception& e) {
            // Log exceptions from the worker thread if needed
            std::cerr << "Exception in worker thread: " << e.what() << std::endl;
            // Consider signaling the main thread or handling the error appropriately
        } catch (...) {
            std::cerr << "Unknown exception in worker thread." << std::endl;
        }
    });
    // --- End Worker Thread Setup ---


    GLFWwindow* window = gui_ui.getWindow(); // Get the window handle
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f); // Background color

    // --- Local GUI State (managed by main loop, updated from GuiInterface) ---
    std::vector<std::string> output_history;
    std::string status_text = "Initializing..."; // Initial status

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

        // --- Process Display Updates from Worker (Stage 4) ---
        bool new_output_added = gui_ui.processDisplayQueue(output_history, status_text);
        // --- End Process Display Updates ---


        // --- Main UI Layout (Stage 3 / Updated for Stage 4) ---
        const ImVec2 display_size = ImGui::GetIO().DisplaySize;
        const float input_height = 35.0f; // Height for the input text box + button
        const float status_height = 25.0f; // Height for the status bar
        const float output_height = display_size.y - input_height - status_height; // Remaining height for output

        // Create a full-window container
        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
        ImGui::SetNextWindowSize(display_size);
        ImGui::Begin("Main", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus);

        // --- Output Area ---
        ImGui::BeginChild("Output", ImVec2(0, output_height), true);
        // Use the local output_history vector updated by processDisplayQueue
        for (const auto& line : output_history) {
            // Use TextWrapped for better readability of long lines
            ImGui::TextWrapped("%s", line.c_str());
        }
        // Auto-scroll based on the flag set by processDisplayQueue
        if (new_output_added && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 10.0f) { // Only auto-scroll if near the bottom
            ImGui::SetScrollHereY(1.0f); // Scroll to bottom if new content added
            new_output_added = false; // Reset flag
        }
        ImGui::EndChild(); // End Output Area

        // --- Input Area ---
        bool enter_pressed = false;
        bool send_pressed = false;
        const float button_width = 60.0f; // Width for Send button
        float input_width = display_size.x - button_width - ImGui::GetStyle().ItemSpacing.x; // Adjust width for Send button only
        if (input_width < 50.f) {          // arbitrary minimum width
            input_width = 50.f;
        }

        ImGui::PushItemWidth(input_width);
        enter_pressed = ImGui::InputText("##Input", gui_ui.getInputBuffer(), gui_ui.getInputBufferSize(), ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::PopItemWidth();
        ImGui::SameLine();
        send_pressed = ImGui::Button("Send", ImVec2(button_width, 0));

        // --- Status Bar ---
        ImGui::Separator();
        ImGui::TextUnformatted(status_text.c_str()); // Use local status_text updated by processDisplayQueue

        // --- Input Handling (Stage 4 & 5) ---
        if (send_pressed || enter_pressed) {
            char* input_buf = gui_ui.getInputBuffer();
            if (input_buf[0] != '\0') {
                // Send input to the worker thread via GuiInterface
                gui_ui.sendInputToWorker(input_buf);

                // Clear the buffer after sending
// Add the user's message to the history for display
                output_history.push_back("User: " + std::string(input_buf));
                new_output_added = true; // Ensure the log scrolls down
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

        // Update and Render additional Platform Windows (Commented out as not used)
        // ImGuiIO& io = ImGui::GetIO();
        // if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) { ... }


        glfwSwapBuffers(window); // Swap the front and back buffers
    }

    // --- Cleanup (Stage 4) ---
    std::cout << "Requesting worker thread shutdown..." << std::endl;
    gui_ui.requestShutdown(); // Signal the worker thread to exit

    std::cout << "Joining worker thread..." << std::endl;
    // No explicit join needed - std::jthread handles this in its destructor (RAII)

    // Ensure GUI shutdown happens regardless of join success/failure/exception
    std::cout << "Shutting down GUI..." << std::endl;
    try {
        gui_ui.shutdown(); // Cleanup ImGui, GLFW
    } catch (const std::exception& e) {
        std::cerr << "GUI Shutdown failed: " << e.what() << std::endl;
        // Continue execution to ensure main returns, but report error.
    }
    std::cout << "Main function finished." << std::endl;

    return 0; // Success
}
