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

// Message type colors
const ImVec4 USER_INPUT_COLOR = ImVec4(0.1f, 1.0f, 0.1f, 1.0f);
const ImVec4 STATUS_COLOR = ImVec4(1.0f, 1.0f, 0.2f, 1.0f);
const ImVec4 ERROR_COLOR = ImVec4(1.0f, 0.2f, 0.2f, 1.0f);
const ImVec4 DEFAULT_TEXT_COLOR = ImGui::GetStyleColorVec4(ImGuiCol_Text); // Use default text color

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
    std::vector<HistoryMessage> output_history; // Updated for Issue #8
    static bool initial_focus_set = false; // Added for Issue #5
    static bool request_input_focus = false;

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
        // Process messages from the worker thread by getting the drained queue
        std::vector<HistoryMessage> new_messages = gui_ui.processDisplayQueue();
        bool new_output_added = !new_messages.empty(); // Check if any messages were actually returned
        if (new_output_added) {
            // Append the new messages to the history using move iterators for efficiency
            output_history.insert(output_history.end(),
                                  std::make_move_iterator(new_messages.begin()),
                                  std::make_move_iterator(new_messages.end()));
      }
      // --- End Process Display Updates ---

      // --- Retrieve and Apply Scroll Offsets (Comment 1) ---
      ImVec2 scroll_offsets = gui_ui.getAndClearScrollOffsets();
      // --- End Retrieve and Apply Scroll Offsets ---

      // --- Main UI Layout (Stage 3 / Updated for Stage 4) ---
      const ImVec2 display_size = ImGui::GetIO().DisplaySize;
      const float input_height = 35.0f; // Height for the input text box + button
      // Calculate height needed for elements below the output area
      const float bottom_elements_height = input_height; // Only input height now

      // Create a full-window container
      ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
      ImGui::SetNextWindowSize(display_size);
      ImGui::Begin("Main", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus);

      // --- Output Area ---
      // Use negative height to automatically fill space minus the bottom elements
      ImGui::BeginChild("Output", ImVec2(0, -bottom_elements_height), true);

       // Implement touch scrolling by dragging within the child window
       // Check if the left mouse button is held down and the mouse is being dragged
       if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
           ImGuiIO& io = ImGui::GetIO();
           // Adjust scroll position based on mouse delta
           ImGui::SetScrollY(ImGui::GetScrollY() - io.MouseDelta.y);
           ImGui::SetScrollX(ImGui::GetScrollX() - io.MouseDelta.x);
       }

       // Iterate over HistoryMessage objects (Issue #8 Refactor)
       for (const auto& message : output_history) {
           bool color_pushed = false;
           std::string display_text;

           // Determine color and display text based on message type
           switch (message.type) {
               case MessageType::USER_INPUT:
                   ImGui::PushStyleColor(ImGuiCol_Text, USER_INPUT_COLOR); // Green
                   color_pushed = true;
                   display_text = "User: " + message.content;
                   break;
               case MessageType::STATUS:
                   ImGui::PushStyleColor(ImGuiCol_Text, STATUS_COLOR); // Yellow
                   color_pushed = true;
                   display_text = "[STATUS] " + message.content; // Add prefix for display only
                   break;
               case MessageType::ERROR:
                   ImGui::PushStyleColor(ImGuiCol_Text, ERROR_COLOR); // Red
                   color_pushed = true;
                   display_text = "ERROR: " + message.content; // Add prefix for display only
                   break;
               case MessageType::LLM_RESPONSE:
               default: // Default includes LLM_RESPONSE
                   display_text = message.content; // No prefix, default color
                   break;
           }

           // Use Selectable to enable text selection/copying
           // Use GetContentRegionAvail().x for width to wrap text within the child window
           if (ImGui::Selectable(display_text.c_str(), false, ImGuiSelectableFlags_AllowDoubleClick, ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
               // On click, copy text to clipboard
               ImGui::SetClipboardText(display_text.c_str());
           }

           // Pop color if one was pushed
           if (color_pushed) {
               ImGui::PopStyleColor();
           }
       }
       // Auto-scroll based on the flag set by processDisplayQueue
       if (new_output_added && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 10.0f) { // Only auto-scroll if near the bottom
           ImGui::SetScrollHereY(1.0f); // Scroll to bottom if new content added
           new_output_added = false; // Reset flag
       }

       // Apply accumulated scroll offsets (Comment 1)
       if (scroll_offsets.x != 0.0f || scroll_offsets.y != 0.0f) {
            ImGui::SetScrollY(ImGui::GetScrollY() + scroll_offsets.y);
            ImGui::SetScrollX(ImGui::GetScrollX() + scroll_offsets.x);
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

        // Request focus for the input field if the flag was set in the previous frame
        if (request_input_focus) {
            ImGui::SetKeyboardFocusHere(); // Target the *next* widget (InputText)
            request_input_focus = false;   // Reset the flag
        }

        // Set focus to the input field on the first frame (Issue #5)
        if (!initial_focus_set) {
            ImGui::SetKeyboardFocusHere(0); // Target the next widget (InputText)
            initial_focus_set = true;
        }

        ImGui::PushItemWidth(input_width);
        enter_pressed = ImGui::InputText("##Input", gui_ui.getInputBuffer(), gui_ui.getInputBufferSize(), ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::PopItemWidth();
        ImGui::SameLine();
        send_pressed = ImGui::Button("Send", ImVec2(button_width, 0));

        // --- Status Bar Removed ---

        // --- Input Handling (Stage 4 & 5) ---
        if (send_pressed || enter_pressed) {
            char* input_buf = gui_ui.getInputBuffer();
            if (input_buf[0] != '\0') {
                // Send input to the worker thread via GuiInterface
                gui_ui.sendInputToWorker(input_buf);
                
                // Add user input to history (Issue #8 Refactor)
// Add the user's message to the history for display
                output_history.push_back({MessageType::USER_INPUT, std::string(input_buf)});
                new_output_added = true; // Ensure the log scrolls down
                input_buf[0] = '\0';
                request_input_focus = true; // Set flag to request focus next frame
            }
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
