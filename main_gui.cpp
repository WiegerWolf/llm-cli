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

// --- Theme State (Issue #18) ---
static ThemeType currentTheme = ThemeType::DARK; // Default theme

// --- Theme-Dependent Message Colors (Issue #18 Fix) ---
const ImVec4 darkUserColor = ImVec4(0.0f, 1.0f, 0.0f, 1.0f); // Green
const ImVec4 darkStatusColor = ImVec4(1.0f, 1.0f, 0.0f, 1.0f); // Yellow
// Response/Error will use default theme text color via TextWrapped

const ImVec4 lightUserColor = ImVec4(0.0f, 0.5f, 0.0f, 1.0f); // Dark Green
const ImVec4 lightStatusColor = ImVec4(0.8f, 0.4f, 0.0f, 1.0f); // Orange/Brown
// Response/Error will use default theme text color via TextWrapped
// --- End Theme-Dependent Colors ---

int main(int, char**) {
    GuiInterface gui_ui; // Instantiate the GUI interface

    try {
        gui_ui.initialize(); // Initialize GLFW, ImGui, etc.
        // TODO: Load theme preference from config (Issue #18)
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
    // Apply initial theme (Issue #18)
    gui_ui.setTheme(currentTheme);
    // Background color will be set by the theme, but keep a default clear color
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

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

      // --- Main UI Layout (Stage 3 / Updated for Stage 4 & 18) ---
      const ImVec2 display_size = ImGui::GetIO().DisplaySize;
      const float input_height = 35.0f; // Height for the input text box + button
      // Calculate height needed for elements below the output area
      // Add space for the settings header if it's open
      float settings_height = 0.0f;
      // We need to estimate the settings height. This is tricky without rendering it first.
      // Let's approximate based on typical ImGui item heights.
      // A CollapsingHeader + Text + 2 RadioButtons + SameLine spacing.
      // This is just an estimate for layout calculation.
      const float estimated_settings_section_height = ImGui::GetTextLineHeightWithSpacing() * 3 + ImGui::GetStyle().FramePadding.y * 4;


      // Create a full-window container
      ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
      ImGui::SetNextWindowSize(display_size);
      ImGui::Begin("Main", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus);

      // --- Settings Area (Issue #18) ---
      if (ImGui::CollapsingHeader("Settings")) {
          settings_height = ImGui::GetItemRectSize().y; // Get actual height after rendering header
          ImGui::Indent();
          ImGui::Text("Theme:");
          ImGui::SameLine();
          if (ImGui::RadioButton("Dark", currentTheme == ThemeType::DARK)) {
              currentTheme = ThemeType::DARK;
              gui_ui.setTheme(currentTheme);
              // TODO: Save theme preference to config (Issue #18)
          }
          ImGui::SameLine();
          if (ImGui::RadioButton("White", currentTheme == ThemeType::WHITE)) {
              currentTheme = ThemeType::WHITE;
              gui_ui.setTheme(currentTheme);
              // TODO: Save theme preference to config (Issue #18)
          }
          ImGui::Unindent();
          settings_height += ImGui::GetItemRectSize().y; // Add height of the radio button line
          settings_height += ImGui::GetStyle().ItemSpacing.y; // Add spacing
      } else {
          settings_height = ImGui::GetItemRectSize().y; // Height of the collapsed header
      }
      // --- End Settings Area ---

      // Calculate height for the output area dynamically
      const float bottom_elements_height = input_height + settings_height + ImGui::GetStyle().ItemSpacing.y; // Add spacing between settings and input

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

       // Iterate over HistoryMessage objects (Issue #8 Refactor / Issue #18 Color Fix)
       for (const auto& message : output_history) {
           std::string display_text; // Temporary buffer for formatted text

           if (message.type == MessageType::USER_INPUT) {
               ImVec4 color = (currentTheme == ThemeType::DARK) ? darkUserColor : lightUserColor;
               // Format text before passing to TextColored
               display_text = "User: " + message.content;
               ImGui::PushStyleColor(ImGuiCol_Text, color); // Push color for Selectable highlighting
               if (ImGui::Selectable(display_text.c_str(), false, ImGuiSelectableFlags_AllowDoubleClick, ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
                   ImGui::SetClipboardText(display_text.c_str());
               }
               ImGui::PopStyleColor(); // Pop color after Selectable
           } else if (message.type == MessageType::STATUS) {
               ImVec4 color = (currentTheme == ThemeType::DARK) ? darkStatusColor : lightStatusColor;
               // Format text before passing to TextColored
               display_text = "[STATUS] " + message.content;
               ImGui::PushStyleColor(ImGuiCol_Text, color); // Push color for Selectable highlighting
               if (ImGui::Selectable(display_text.c_str(), false, ImGuiSelectableFlags_AllowDoubleClick, ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
                   ImGui::SetClipboardText(display_text.c_str());
               }
               ImGui::PopStyleColor(); // Pop color after Selectable
           } else { // LLM_RESPONSE or ERROR - use default theme text color and wrapping
               if (message.type == MessageType::ERROR) {
                   display_text = "ERROR: " + message.content;
               } else {
                   display_text = message.content;
               }
               // Use TextWrapped for automatic wrapping and default theme color
               // Selectable still works with TextWrapped content if needed for copy
               ImGui::PushTextWrapPos(ImGui::GetContentRegionAvail().x); // Enable wrapping
               if (ImGui::Selectable(display_text.c_str(), false, ImGuiSelectableFlags_AllowDoubleClick)) {
                   ImGui::SetClipboardText(display_text.c_str());
               }
               ImGui::PopTextWrapPos();
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
        // Calculate input width dynamically, considering the button and spacing
        float input_width = ImGui::GetContentRegionAvail().x - button_width - ImGui::GetStyle().ItemSpacing.x;
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
        // Get the current background color from the theme
        clear_color = ImGui::GetStyle().Colors[ImGuiCol_WindowBg];
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
