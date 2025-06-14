#include "gui_interface/gui_interface.h"
#include "font_utils.h"
#include "theme_utils.h"
#include "chat_client.h"
#include "database.h"
#include "graph_manager.h"
#include "main_gui_views.h" // Interface to the drawing logic

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

#include <iostream>
#include <memory>
#include <thread>
#include <stop_token>
#include <vector>
#include <atomic>

// --- Global State ---
// This unique_ptr owns the GraphManager and is now defined in the core application logic.
// It is declared 'extern' in main_gui_views.cpp to be accessible for rendering.
std::unique_ptr<GraphManager> g_graph_manager;
ThemeType currentTheme = ThemeType::DARK;

// --- Main loop state variables, to be managed by the core loop ---
std::vector<HistoryMessage> output_history;
bool new_output_added = false;
bool request_input_focus = false;
std::atomic<bool> is_loading_models = false; // Used by both core and views

void handle_font_size_controls(GuiInterface& gui, Database& db) {
    ImGuiIO& io = ImGui::GetIO();
    if (io.KeyCtrl) {
        if (ImGui::IsKeyPressed(ImGuiKey_Equal, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadAdd, false)) {
            FontUtils::changeFontSize(gui, +1.0f, db);
        }
        else if (ImGui::IsKeyPressed(ImGuiKey_Minus, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadSubtract, false)) {
            FontUtils::changeFontSize(gui, -1.0f, db);
        }
        else if (ImGui::IsKeyPressed(ImGuiKey_0, false) || ImGui::IsKeyPressed(ImGuiKey_Keypad0, false)) {
            FontUtils::changeFontSize(gui, 18.0f - gui.getCurrentFontSize(), db);
        }
    }
}

void process_history_updates(GuiInterface& gui, Database& db) {
    std::vector<HistoryMessage> new_messages = gui.processDisplayQueue();
    new_output_added = !new_messages.empty();
    bool graph_needs_update = new_output_added;

    if (new_output_added) {
        auto const old_size = output_history.size();
        output_history.insert(output_history.end(),
                              std::make_move_iterator(new_messages.begin()),
                              std::make_move_iterator(new_messages.end()));
        
        auto first_new_message = std::next(output_history.begin(), old_size);
        for (auto it = first_new_message; it != output_history.end(); ++it) {
            g_graph_manager->HandleNewHistoryMessage(*it, g_graph_manager->GetSelectedNodeId(), db);
        }
        g_graph_manager->RestartLayoutAnimation();
    }
    
    // Automatic Graph Synchronization logic could be further refined here if needed
}

void update_graph_layout() {
    bool animation_running = g_graph_manager->IsLayoutRunning() && !g_graph_manager->isAnimationPaused();
    if (g_graph_manager->isGraphLayoutDirty() || animation_running) {
        if (!g_graph_manager->GetAllNodes().empty()) {
            g_graph_manager->UpdateLayout();
        }
    }
}

int main(int, char**) {
    Database db_manager;
    g_graph_manager = std::make_unique<GraphManager>(&db_manager);

    try {
        std::optional<std::string> theme_value = db_manager.loadSetting("theme");
        if (theme_value.has_value() && theme_value.value() == "WHITE") {
            currentTheme = ThemeType::WHITE;
        }
    } catch (const std::exception& e) {
        std::cerr << "Warning: Failed to load theme setting: " << e.what() << std::endl;
    }

    float initial_font_size = 18.0f;
    try {
        std::optional<std::string> font_size_value = db_manager.loadSetting("font_size");
        if (font_size_value.has_value()) {
            initial_font_size = std::stof(font_size_value.value());
            if (initial_font_size < 8.0f || initial_font_size > 72.0f) {
                 initial_font_size = 18.0f;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Warning: Failed to load font_size setting: " << e.what() << std::endl;
    }

    GuiInterface gui_ui(db_manager);
    FontUtils::setInitialFontSize(gui_ui, initial_font_size);

    try {
        gui_ui.initialize();
    } catch (const std::exception& e) {
        std::cerr << "GUI Initialization failed: " << e.what() << std::endl;
        return 1;
    }
    
    // Set initial theme after GUI is initialized
    ThemeUtils::setTheme(gui_ui, currentTheme);

    ChatClient client(gui_ui, db_manager);
    client.initialize_model_manager();

    std::jthread worker_thread([&client](std::stop_token st){
        try {
            client.run();
        } catch (const std::exception& e) {
            std::cerr << "Exception in worker thread: " << e.what() << std::endl;
        }
    });

    client.setActiveModel(gui_ui.getSelectedModelIdFromUI());

    GLFWwindow* window = gui_ui.getWindow();
    static bool initial_focus_set = false;

    // --- Main Render Loop ---
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        
        if (!initial_focus_set) {
            ImGui::SetKeyboardFocusHere(0);
            initial_focus_set = true;
            request_input_focus = true;
        }

        handle_font_size_controls(gui_ui, db_manager);
        process_history_updates(gui_ui, db_manager);
        is_loading_models = gui_ui.areModelsLoadingInUI();
        update_graph_layout();

        // Call the single drawing function that encapsulates all UI rendering.
        drawAllViews(*g_graph_manager, gui_ui);
        
        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        ImVec4 clear_color = ImGui::GetStyle().Colors[ImGuiCol_WindowBg];
        glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    // --- Cleanup ---
    gui_ui.requestShutdown();
    // jthread joins automatically
    
    try {
        db_manager.saveSetting("font_size", std::to_string(gui_ui.getCurrentFontSize()));
    } catch (const std::exception& e) {
        std::cerr << "Warning: Failed to save font_size: " << e.what() << std::endl;
    }
    
    gui_ui.shutdown();
    return 0;
}