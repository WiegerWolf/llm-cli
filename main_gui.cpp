#include "gui_interface/gui_interface.h"
#include <stdexcept>
#include <cmath>
#include <iostream>
#include <vector>
#include <string>
#include <thread> // Added for Stage 4
#include <stop_token>   // For std::jthread and std::stop_token (C++20)
#include "chat_client.h" // Added for Stage 4
#include "database.h"    // Added for Issue #18 (DB Persistence)
#include <optional>     // Added for Issue #18 (DB Persistence)
#include <cstring>      // Added for Phase 3 (strlen)
#include <chrono>       // For timestamp operations (Model Dropdown Icons)
#include <sstream>      // For parsing timestamps (Model Dropdown Icons)
#include <iomanip>      // For std::get_time (Model Dropdown Icons)
#include <algorithm>    // For std::any_of or string searching (Model Dropdown Icons)
 
 // Include GUI library headers needed for the main loop
 #include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_internal.h> // Added to fix build errors from using internal ImGui structures
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

// --- Helper Functions for Model Dropdown Icons ---

// Parses timestamp strings like "YYYY-MM-DDTHH:MM:SSZ" or "YYYY-MM-DD HH:MM:SS"
static std::optional<std::chrono::system_clock::time_point> parse_timestamp(const std::string& ts_str) {
    std::tm t{};
    std::istringstream ss(ts_str);

    // Try ISO8601 format with 'T' and potentially 'Z'
    if (ts_str.find('T') != std::string::npos) {
        ss >> std::get_time(&t, "%Y-%m-%dT%H:%M:%S");
        // Note: The 'Z' (UTC) is not directly handled by get_time here.
        // For robust UTC handling, a more advanced parser or C++20 chrono features would be needed.
        // We proceed assuming mktime will give a comparable time_point for "recent" checks.
    } else {
        // Try "YYYY-MM-DD HH:MM:SS" format
        ss >> std::get_time(&t, "%Y-%m-%d %H:%M:%S");
    }

    if (ss.fail()) {
        // Attempt to parse if there was a trailing 'Z' after seconds for the first format
        if (ts_str.find('T') != std::string::npos && !ts_str.empty() && ts_str.back() == 'Z') {
            std::string SuffixlessStr = ts_str.substr(0, ts_str.length() -1);
            std::istringstream ss_retry(SuffixlessStr);
            ss_retry >> std::get_time(&t, "%Y-%m-%dT%H:%M:%S");
            if(ss_retry.fail()){
                 // std::cerr << "Failed to parse timestamp (after Z removal attempt): " << ts_str << std::endl; // Optional: reduce noise
                 return std::nullopt;
            }
        } else {
            // std::cerr << "Failed to parse timestamp: " << ts_str << std::endl; // Optional: reduce noise
            return std::nullopt;
        }
    }

    // std::mktime converts local time. This is a simplification.
    // If timestamps are strictly UTC, timegm (POSIX) or _mkgmtime (Windows) would be better,
    // or full C++20 timezone support.
    t.tm_isdst = -1; // Let mktime determine DST
    std::time_t time_c = std::mktime(&t);
    if (time_c == -1) {
        // This can happen if std::get_time partially succeeded but resulted in an invalid date/time for mktime
        // std::cerr << "Failed to convert std::tm to time_t for timestamp: " << ts_str << std::endl; // Optional: reduce noise
        return std::nullopt;
    }
    return std::chrono::system_clock::from_time_t(time_c);
}

static bool is_model_new_or_updated(const ModelData& model_data) {
    auto now = std::chrono::system_clock::now();
    auto seven_days_ago_tp = now - std::chrono::hours(24 * 7);
    bool is_recent = false;

    // Handle created_at_api (long long Unix timestamp)
    if (model_data.created_at_api != 0) { // Check if it's a valid timestamp (not default 0)
        auto created_at_tp = std::chrono::system_clock::from_time_t(static_cast<time_t>(model_data.created_at_api));
        if (created_at_tp > seven_days_ago_tp) {
            is_recent = true;
        }
    }

    if (is_recent) return true; // Early exit if already found to be recent

    // Handle last_updated_db (string timestamp)
    if (!model_data.last_updated_db.empty()) {
        auto updated_at_db_tp_opt = parse_timestamp(model_data.last_updated_db);
        if (updated_at_db_tp_opt && *updated_at_db_tp_opt > seven_days_ago_tp) {
            is_recent = true;
        }
    }
    return is_recent;
}

static std::string get_modality_icon_str(const ModelData& model_data) {
    bool input_text = model_data.architecture_input_modalities.find("\"text\"") != std::string::npos;
    bool input_image = model_data.architecture_input_modalities.find("\"image\"") != std::string::npos;
    // Add other input modalities here if needed, e.g., "audio", "video"

    bool output_text = model_data.architecture_output_modalities.find("\"text\"") != std::string::npos;
    bool output_image = model_data.architecture_output_modalities.find("\"image\"") != std::string::npos;
    // Add other output modalities here

    int input_modal_count = (input_text ? 1 : 0) + (input_image ? 1 : 0); // Simple count for now
    int output_modal_count = (output_text ? 1 : 0) + (output_image ? 1 : 0); // Simple count for now

    if (input_text && output_text && input_modal_count == 1 && output_modal_count == 1) return "\U0001F4DD\u27A1\U0001F4DD"; // 📝➡️📝
    if (input_image && output_text && input_modal_count == 1 && output_modal_count == 1) return "\U0001F5BC\uFE0F\u27A1\U0001F4DD"; // 🖼️➡️📝
    if (input_text && output_image && input_modal_count == 1 && output_modal_count == 1) return "\U0001F4DD\u27A1\U0001F5BC\uFE0F"; // 📝➡️🖼️
    
    // If multiple input or output modalities are present (based on simple count)
    if (input_modal_count > 1 || output_modal_count > 1) return "\U0001F504"; // 🔄 (Generic multi-modal)

    // Fallback for single but not combined (e.g. text only, image only) or unhandled
    // Could add more specific icons like just "📝" or "🖼️" if desired
    return ""; // No icon if no specific match or complex multi-modal not covered above
}

// --- End Helper Functions for Model Dropdown Icons ---

// Helper function to format price per million tokens
static std::string format_price_per_million(const std::string& price_str) {
    if (price_str.empty() || price_str == "N/A") return "N/A";
    try {
        // Remove any potential currency symbols or non-numeric characters before conversion
        std::string numeric_price_str;
        for (char ch : price_str) {
            if (std::isdigit(ch) || ch == '.' || ch == '-') {
                numeric_price_str += ch;
            }
        }
        if (numeric_price_str.empty()) return "N/A";

        double price = std::stod(numeric_price_str);
        double price_per_million = price * 1000000.0;
        std::ostringstream oss;
        oss << "$" << std::fixed << std::setprecision(2) << price_per_million;
        return oss.str();
    } catch (const std::invalid_argument& ia) {
        // std::cerr << "Invalid argument for stod: " << price_str << std::endl; // Optional: for debugging
        return "N/A";
    } catch (const std::out_of_range& oor) {
        // std::cerr << "Out of range for stod: " << price_str << std::endl; // Optional: for debugging
        return "N/A";
    }
}

// Helper function to format context size
static std::string format_context_size(int context_length) {
    if (context_length <= 0) return "N/A";
    std::ostringstream oss;
    if (context_length < 1000) {
        oss << context_length;
    } else if (context_length < 1000000) {
        oss << static_cast<int>(std::round(static_cast<double>(context_length) / 1000.0)) << "k";
    } else {
        double millions = static_cast<double>(context_length) / 1000000.0;
        if (std::fabs(millions - std::round(millions)) < 0.05) {
            oss << static_cast<int>(std::round(millions)) << "M";
        } else {
            oss << std::fixed << std::setprecision(1) << millions << "M";
        }
    }
    return oss.str();
}
 
// --- Helper Function for Coordinate Mapping (Phase 3 - Placeholder) ---
// Helper function to map screen coordinates to text indices within wrapped text
// --- Helper Function for Coordinate Mapping (Phase 3 - Placeholder) ---
// Helper function to map screen coordinates to text indices within wrapped text
// Returns true if valid indices were found, false otherwise.
// TODO: Implement the actual mapping logic here. This is complex and currently a placeholder.
// Helper function to map screen coordinates to text indices within wrapped text.
// Returns true if valid indices were found (overlap exists), false otherwise.
// Calculates bounding boxes for each character considering wrapping and finds the
// first and last character indices whose boxes overlap the selection rectangle.
// --- Helper Function for Coordinate Mapping (Phase 3 - Placeholder / Phase 5 Update) ---
// Helper function to map screen coordinates to text indices within wrapped text.
// Returns true if valid indices were found (overlap exists), false otherwise.
// Calculates bounding boxes for each character considering wrapping and finds the
// first and last character indices whose boxes overlap the selection rectangle.
// Also outputs the calculated character bounding boxes.
bool MapScreenCoordsToTextIndices(
    const char* text,
    float wrap_width,
    const ImVec2& selectable_min, // Top-left of the text block's drawing area
    const ImVec2& selection_rect_min, // Top-left of the *clamped* selection rectangle
    const ImVec2& selection_rect_max, // Bottom-right of the *clamped* selection rectangle
    int& out_start_index,
    int& out_end_index,
    std::vector<ImRect>& out_char_rects) // Phase 5: Output parameter for char rects
{
    out_start_index = -1;
    out_end_index = -1;
    out_char_rects.clear(); // Phase 5: Clear output vector
    if (!text || text[0] == '\0' || wrap_width <= 0) {
        return false;
    }

    // Basic check: If the selection rectangle is invalid, return false
    if (selection_rect_min.x >= selection_rect_max.x || selection_rect_min.y >= selection_rect_max.y) {
        return false;
    }

    ImGuiContext& g = *GImGui;
    // Use FontSize directly for height calculation, as TextWrapped doesn't add ItemSpacing.y vertically between lines itself.
    // Line spacing is handled by the cursor advancement during layout.
    const float line_height = g.FontSize; // Use font size as the primary line height determinant
    const float line_spacing = g.Style.ItemSpacing.y; // Get vertical spacing between lines/widgets

    ImVec2 cursor_pos = selectable_min;
    int text_len = static_cast<int>(strlen(text));
    // std::vector<ImRect> char_rects; // Phase 5: Removed, using out_char_rects instead
    out_char_rects.reserve(text_len); // Phase 5: Reserve space in the output vector

    int current_char_index = 0;
    const char* current_char_ptr = text;
    const char* text_end = text + text_len;

    // --- Simulate text layout and store character bounding boxes ---
    while (current_char_ptr < text_end) {
        // Correctly advance to the next UTF-8 character
        unsigned int codepoint; // To store the decoded Unicode codepoint.
        int char_byte_count = ImTextCharFromUtf8(&codepoint, current_char_ptr, text_end);
        const char* next_char_ptr;

        if (char_byte_count > 0) {
            // Successfully decoded a UTF-8 character.
            next_char_ptr = current_char_ptr + char_byte_count;
            // Ensure next_char_ptr does not exceed text_end.
            // ImTextCharFromUtf8 is expected to respect text_end, so char_byte_count should be appropriate.
            // This check is an additional safeguard.
            if (next_char_ptr > text_end) {
                 next_char_ptr = text_end;
            }
        } else {
            // ImTextCharFromUtf8 returned 0 or a non-positive value, indicating:
            // - End of input string (e.g., *current_char_ptr == '\0' and current_char_ptr < text_end)
            // - Invalid UTF-8 sequence
            // - current_char_ptr >= text_end (though the outer loop `while (current_char_ptr < text_end)` should prevent this)
            // In such cases, advance by one byte to ensure progress and prevent infinite loops.
            next_char_ptr = current_char_ptr + 1;
            // Final clamp to ensure we absolutely do not go past text_end.
            if (next_char_ptr > text_end) {
                next_char_ptr = text_end;
            }
        }

        // Calculate size of the current character
        // Use CalcTextSize without wrapping for individual characters.
        ImVec2 char_size = ImGui::CalcTextSize(current_char_ptr, next_char_ptr, false, 0.0f);

        // Handle line wrapping *before* placing the character
        // Check if this character *would* exceed the wrap width, but only if it's not the first char on the line.
        if (cursor_pos.x > selectable_min.x && (cursor_pos.x + char_size.x) > (selectable_min.x + wrap_width)) {
            cursor_pos.x = selectable_min.x;
            // Advance Y by font size + spacing for the new line
            cursor_pos.y += line_height; // Corrected: ItemSpacing.y is not added between wrapped lines of the same text block
        }

        // Store the bounding box for this character
        // The height of the box should be the line height (FontSize)
        ImRect char_rect = ImRect(cursor_pos, ImVec2(cursor_pos.x + char_size.x, cursor_pos.y + line_height));
        out_char_rects.push_back(char_rect); // Phase 5: Add to output vector

        // Advance cursor position for the next character horizontally
        cursor_pos.x += char_size.x;

        // Move to the next character in the input string
        current_char_ptr = next_char_ptr;
        current_char_index++;
    }

    // --- Find start and end indices based on selection rectangle overlap ---
    int first_intersecting_idx = -1;
    int last_intersecting_idx = -1;

    for (int k = 0; k < out_char_rects.size(); ++k) { // Phase 5: Iterate using out_char_rects
        // Check for intersection between character rect and selection rect
        // Use a slightly expanded check vertically to be more lenient with mouse Y position
        ImRect selection_imrect(selection_rect_min, selection_rect_max);
        if (out_char_rects[k].Overlaps(selection_imrect)) { // Phase 5: Check using out_char_rects
             if (first_intersecting_idx == -1) {
                first_intersecting_idx = k; // Record the first character that overlaps
            }
            last_intersecting_idx = k; // Always update to the last character that overlaps
        }
    }

    // If any intersection was found
    if (first_intersecting_idx != -1) {
        out_start_index = first_intersecting_idx;
        // The end index should be *after* the last selected character for substr
        out_end_index = last_intersecting_idx + 1;
        return true; // Indicate success
    }

    // If no direct overlap, consider finding the closest character (more complex, omitted for now)
    // For instance, find the character whose center is closest to selection_rect_min/max.

    return false; // No overlap found
}
// --- End Helper Function ---


int main(int, char**) {
    // --- Database Initialization (Issue #18 DB Persistence) ---
    PersistenceManager db_manager; // Instantiate DB manager first
    try {
        // Load theme preference from database
        std::optional<std::string> theme_value = db_manager.loadSetting("theme");
        if (theme_value.has_value()) {
            if (theme_value.value() == "WHITE") {
                currentTheme = ThemeType::WHITE;
            } else {
                currentTheme = ThemeType::DARK; // Default to DARK if value is unexpected
            }
        } // Else: keep the default DARK theme if setting not found
    } catch (const std::exception& e) {
        std::cerr << "Warning: Failed to load theme setting from database: " << e.what() << std::endl;
        // Continue with default theme
    }
 
    // --- Load Font Size (Issue #19 Persistence) ---
    float initial_font_size = 18.0f; // Default font size
    try {
        std::optional<std::string> font_size_value = db_manager.loadSetting("font_size");
        if (font_size_value.has_value()) {
            try {
                initial_font_size = std::stof(font_size_value.value());
                // Add basic validation for loaded font size
                if (initial_font_size < 8.0f || initial_font_size > 72.0f) {
                     std::cerr << "Warning: Loaded font size (" << initial_font_size << ") out of reasonable bounds (8-72). Resetting to default." << std::endl;
                     initial_font_size = 18.0f;
                }
            } catch (const std::invalid_argument& ia) {
                std::cerr << "Warning: Invalid font size value in database: '" << font_size_value.value() << "'. Using default." << std::endl;
                initial_font_size = 18.0f;
            } catch (const std::out_of_range& oor) {
                std::cerr << "Warning: Font size value in database out of range: '" << font_size_value.value() << "'. Using default." << std::endl;
                initial_font_size = 18.0f;
            }
        } // Else: keep the default font size if setting not found
    } catch (const std::exception& e) {
        std::cerr << "Warning: Failed to load font_size setting from database: " << e.what() << std::endl;
        // Continue with default font size
    }
 
    // --- GUI Initialization ---
    GuiInterface gui_ui(db_manager); // MODIFIED: Pass db_manager to constructor
    gui_ui.setInitialFontSize(initial_font_size); // Apply loaded/default font size BEFORE init

    try {
        gui_ui.initialize(); // Initialize GLFW, ImGui, etc.
        // Theme is loaded above, before GUI init potentially uses it
        // Model ID is loaded within gui_ui.initialize()
    } catch (const std::exception& e) {
        std::cerr << "GUI Initialization failed: " << e.what() << std::endl;
        return 1;
    }

    // --- Worker Thread Setup (Stage 4 / Issue #18 DB Persistence) ---
    ChatClient client(gui_ui, db_manager); // Pass DB manager reference
    // Initialize model manager. This is blocking for the initial load.
    // GuiInterface will be updated with loading states and final model list.
    client.initialize_model_manager();

    // Use jthread with RAII: construct in-place with lambda, joins automatically on destruction
    // The stop_token is implicitly available to the lambda in C++20.
    std::jthread worker_thread([&client](std::stop_token st){
        try {
            client.run(); // The client's run loop should handle the stop_token internally if needed
        } catch (const std::exception& e) {
            // Log exceptions from the worker thread if needed
            std::cerr << "Exception in worker thread: " << e.what() << std::endl;
            // Consider signaling the main thread or handling the error appropriately
        } catch (...) {
            std::cerr << "Unknown exception in worker thread." << std::endl;
        }
    });
    // --- End Worker Thread Setup ---

    // --- Model Selection GUI State (Part III GUI Changes / Part V Update) ---
    static std::vector<GuiInterface::ModelEntry> available_models_list;
    static std::string current_gui_selected_model_id;
    static int current_gui_selected_model_idx = -1;

    // Initial population of model list for the GUI, after ChatClient has initialized them
    // Use the new GuiInterface methods that are mutex-protected
    available_models_list = gui_ui.getAvailableModelsForUI();
    current_gui_selected_model_id = gui_ui.getSelectedModelIdFromUI();

    current_gui_selected_model_idx = -1; // Reset before searching
    for (int i = 0; i < available_models_list.size(); ++i) {
        if (available_models_list[i].id == current_gui_selected_model_id) {
            current_gui_selected_model_idx = i;
            break;
        }
    }
    // If current_gui_selected_model_id (possibly from DB or default) wasn't in the list,
    // or if the list was empty, GuiInterface::updateModelsList and getSelectedModelIdFromUI
    // should have handled fallback to a valid model (e.g., DEFAULT_MODEL_ID or first available).
    // This logic ensures the GUI's current_gui_selected_model_idx matches.
    if (current_gui_selected_model_idx == -1 && !available_models_list.empty()) {
        current_gui_selected_model_idx = 0; // Default to first in the list
        current_gui_selected_model_id = available_models_list[0].id;
        // GuiInterface should already have this as its current_selected_model_id_in_ui
        // and persisted it. We are just aligning the local GUI index.
    } else if (current_gui_selected_model_idx == -1 && available_models_list.empty()) {
        // This case means no models are available at all, not even a default.
        // GuiInterface should provide a fallback (e.g. DEFAULT_MODEL_ID entry).
        // If available_models_list is truly empty, combo box won't show.
        // current_gui_selected_model_id would be DEFAULT_MODEL_ID from GuiInterface.
    }
    
    // Ensure ChatClient's active model is synchronized with GUI's initial state
    // (which should be the one loaded/selected by GuiInterface and ChatClient::initialize_model_manager)
    client.setActiveModel(current_gui_selected_model_id);
    // --- End Model Selection GUI State ---


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

        // --- Process Deferred Font Rebuild (Issue #19 Fix) ---
        // Font rebuilding is now handled internally by GuiInterface when font size changes.
        // The explicit call to processFontRebuildRequest() is no longer needed here.
        // --- End Process Deferred Font Rebuild ---
 
        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
// --- Font Size Control Handling (Issue #19) ---
        ImGuiIO& io = ImGui::GetIO();

        // Check if Ctrl is pressed. Allow font resizing even if a widget has keyboard focus.
        if (io.KeyCtrl) {
            // Increase font size (Ctrl + '+')
            // Use 'false' for repeat parameter in IsKeyPressed if continuous resizing on hold is desired,
            // otherwise 'true' (default) or omit for single press detection.
            if (ImGui::IsKeyPressed(ImGuiKey_Equal, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadAdd, false)) {
                gui_ui.changeFontSize(+1.0f);
            }
            // Decrease font size (Ctrl + '-')
            else if (ImGui::IsKeyPressed(ImGuiKey_Minus, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadSubtract, false)) {
                gui_ui.changeFontSize(-1.0f);
            }
            // Reset font size (Ctrl + '0')
            else if (ImGui::IsKeyPressed(ImGuiKey_0, false) || ImGui::IsKeyPressed(ImGuiKey_Keypad0, false)) {
                constexpr float kDefaultSize = 18.0f;
                float delta = kDefaultSize - gui_ui.getCurrentFontSize();
                if (std::fabs(delta) > 0.01f) {
                     gui_ui.changeFontSize(delta);   // Reset via public API
                }
            }
        }
        // --- End Font Size Control Handling ---

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
  
      // Get model loading state for disabling UI elements
      bool is_loading_models = gui_ui.areModelsLoadingInUI();
  
      // --- Main UI Layout (Stage 3 / Updated for Stage 4 & 18 / Part V Update) ---
      const ImVec2 display_size = ImGui::GetIO().DisplaySize;
      const float input_height = 35.0f; // Height for the input text box + button
      float settings_height = 0.0f;
  
  
      // Create a full-window container
      ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
      ImGui::SetNextWindowSize(display_size);
      ImGui::Begin("Main", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus);
  
      // --- Settings Area (Issue #18 / Part V Update) ---
      if (ImGui::CollapsingHeader("Settings")) {
          settings_height = ImGui::GetItemRectSize().y; // Get actual height after rendering header
          ImGui::Indent();
          ImGui::Text("Theme:");
          ImGui::SameLine();
          if (ImGui::RadioButton("Dark", currentTheme == ThemeType::DARK)) {
              currentTheme = ThemeType::DARK;
              gui_ui.setTheme(currentTheme);
              try {
                  db_manager.saveSetting("theme", "DARK");
              } catch (const std::exception& e) {
                  std::cerr << "Warning: Failed to save theme setting: " << e.what() << std::endl;
              }
          }
          ImGui::SameLine();
          if (ImGui::RadioButton("White", currentTheme == ThemeType::WHITE)) {
              currentTheme = ThemeType::WHITE;
              gui_ui.setTheme(currentTheme);
              try {
                  db_manager.saveSetting("theme", "WHITE");
              } catch (const std::exception& e) {
                  std::cerr << "Warning: Failed to save theme setting: " << e.what() << std::endl;
              }
          }
          ImGui::Unindent();
          settings_height += ImGui::GetItemRectSize().y; // Add height of the radio button line
          
          // --- Model Selection Combo Box (Part III GUI Changes / Part V Update) ---
          ImGui::Indent();
          if (is_loading_models) {
              ImGui::Text("Loading models..."); // Display loading message
              settings_height += ImGui::GetTextLineHeightWithSpacing();
          } else {
              // Refresh local model list and selection state from GuiInterface
              // This ensures the dropdown reflects the latest state after loading or if changed by ChatClient
              available_models_list = gui_ui.getAvailableModelsForUI();
              current_gui_selected_model_id = gui_ui.getSelectedModelIdFromUI();
              current_gui_selected_model_idx = -1; // Reset index before searching
              for (int i = 0; i < available_models_list.size(); ++i) {
                  if (available_models_list[i].id == current_gui_selected_model_id) {
                      current_gui_selected_model_idx = i;
                      break;
                  }
              }
              // If the selected ID from gui_ui is somehow not in its list (should not happen if logic is correct)
              // or if the list is empty, handle gracefully.
              if (current_gui_selected_model_idx == -1 && !available_models_list.empty()) {
                  current_gui_selected_model_idx = 0; // Default to first if current selection not found
                  current_gui_selected_model_id = available_models_list[0].id;
                  // It's important that GuiInterface is the source of truth.
                  // If a mismatch occurs, it implies GuiInterface's state is what we should reflect.
                  // No need to call setSelectedModelInUI here unless we are forcing a change *from* main_gui.
              }
  
  
              if (available_models_list.empty()) {
                  ImGui::Text("No models available."); // Or "Using default model: [ID]"
                  settings_height += ImGui::GetTextLineHeightWithSpacing();
              } else {
                  ImGui::BeginDisabled(is_loading_models); // This will be false here, but good for future async reloads
                  const char* combo_preview_value = (current_gui_selected_model_idx >= 0 && current_gui_selected_model_idx < available_models_list.size())
                                                      ? available_models_list[current_gui_selected_model_idx].name.c_str()
                                                      : "Select a Model";
                  if (ImGui::BeginCombo("Active Model", combo_preview_value)) {
                      // Get fonts from GuiInterface instance (gui_ui)
                      ImFont* main_font = gui_ui.GetMainFont(); // Assuming gui_ui is accessible
                      ImFont* small_font = gui_ui.GetSmallFont();
                      float available_width_for_text = ImGui::GetContentRegionAvail().x - ImGui::GetStyle().FramePadding.x * 2.0f;


                      for (int i = 0; i < available_models_list.size(); ++i) {
                          const GuiInterface::ModelEntry& current_entry = available_models_list[i];
                          const bool is_selected = (current_gui_selected_model_idx == i);

                          // Attempt to fetch full model data for detailed display
                          std::optional<ModelData> model_data_opt = db_manager.getModelById(current_entry.id);

                          // Use a temporary string for the selectable label to handle multi-line structure
                          // The actual rendering will be done manually to control fonts.
                          std::string selectable_label = "##model_selectable_" + current_entry.id;
                          
                          // Calculate the height of the three lines of text.
                          // Line 1 (main_font), Line 2 (small_font), Line 3 (small_font)
                          float line1_height = main_font ? main_font->FontSize : ImGui::GetTextLineHeight();
                          float line2_height = small_font ? small_font->FontSize : ImGui::GetTextLineHeightWithSpacing() * 0.8f;
                          float line3_height = small_font ? small_font->FontSize : ImGui::GetTextLineHeightWithSpacing() * 0.8f;
                          // Adjusted for 2 lines of text: Name (line1) and Details (line3)
                          float total_text_height = line1_height + line3_height + ImGui::GetStyle().ItemSpacing.y * 1;


                          if (ImGui::Selectable(selectable_label.c_str(), is_selected, 0, ImVec2(0, total_text_height))) {
                              current_gui_selected_model_idx = i;
                              current_gui_selected_model_id = current_entry.id;
                              gui_ui.setSelectedModelInUI(current_gui_selected_model_id);
                              client.setActiveModel(current_gui_selected_model_id);
                          }
                          if (is_selected) { ImGui::SetItemDefaultFocus(); }

                          // Custom rendering within the selectable area
                          ImVec2 item_start_pos = ImGui::GetItemRectMin();
                          ImDrawList* draw_list = ImGui::GetWindowDrawList();

                          if (model_data_opt) {
                              const ModelData& model = *model_data_opt;
                              std::string line1_text, line2_text, line3_text;

                              std::string icons_string;
                              if (is_model_new_or_updated(model)) icons_string += " \u2728";
                              if (model.top_provider_is_moderated) icons_string += " \U0001F512";
                              std::string modality_icon = get_modality_icon_str(model);
                              if (!modality_icon.empty()) icons_string += " " + modality_icon;

                              line1_text = model.name + icons_string;
                              
                              // Model description (line2_text) is removed.

                              // Format context string using helper
                              std::string context_str = "Context: " + format_context_size(model.context_length);
                              
                              // Format pricing string using helper
                              std::string prompt_price_formatted = format_price_per_million(model.pricing_prompt);
                              std::string completion_price_formatted = format_price_per_million(model.pricing_completion);
                              std::string pricing_str;
                              if (prompt_price_formatted == "N/A" && completion_price_formatted == "N/A") {
                                  pricing_str = "Price: N/A";
                              } else {
                                  pricing_str = "P: " + prompt_price_formatted + "/1M | C: " + completion_price_formatted + "/1M tokens";
                              }
                              line3_text = context_str + " | " + pricing_str;

                              // Render Line 1
                              if (main_font) ImGui::PushFont(main_font);
                              draw_list->AddText(item_start_pos, ImGui::GetColorU32(ImGuiCol_Text), line1_text.c_str());
                              if (main_font) ImGui::PopFont();
                              item_start_pos.y += line1_height + ImGui::GetStyle().ItemSpacing.y;

                              // Line 2 (Description) is removed.
                              
                              // Render Line 3 (Details: Context & Price)
                              if (small_font) ImGui::PushFont(small_font);
                              draw_list->AddText(item_start_pos, ImGui::GetColorU32(ImGuiCol_Text), line3_text.c_str());
                              if (small_font) ImGui::PopFont();

                          } else {
                              // Fallback rendering
                              if (main_font) ImGui::PushFont(main_font);
                              draw_list->AddText(item_start_pos, ImGui::GetColorU32(ImGuiCol_Text), current_entry.name.c_str());
                              if (main_font) ImGui::PopFont();
                              item_start_pos.y += line1_height + ImGui::GetStyle().ItemSpacing.y;

                              // Fallback Line 2 (Details N/A)
                              if (small_font) ImGui::PushFont(small_font);
                              draw_list->AddText(item_start_pos, ImGui::GetColorU32(ImGuiCol_Text), "(Details N/A)");
                              if (small_font) ImGui::PopFont();
                          }
                      }
                      ImGui::EndCombo();
                  }
                  ImGui::EndDisabled();
                  settings_height += ImGui::GetItemRectSize().y; // Add height of combo box
              }
          }
          ImGui::Unindent();
          // --- End Model Selection Combo Box ---
  
          settings_height += ImGui::GetStyle().ItemSpacing.y; // Add spacing
      } else {
          settings_height = ImGui::GetItemRectSize().y; // Height of the collapsed header
      }
      // --- End Settings Area ---
  
      // Calculate height for the output area dynamically
      // Add spacing between settings and input, ensure it's positive
      float spacing_between_settings_input = ImGui::GetStyle().ItemSpacing.y > 0 ? ImGui::GetStyle().ItemSpacing.y : 8.0f;
      const float bottom_elements_height = input_height + settings_height + spacing_between_settings_input;
  
  
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

       // --- Selection State (Issue #26 / Phase 3 Update) ---
       static bool is_selecting = false;
       static int selecting_message_index = -1;
       static ImVec2 selection_start_pos; // Store where selection drag started
       static int selection_start_char_index = -1; // Index of the first selected character
       static int selection_end_char_index = -1;   // Index of the character AFTER the last selected one

       // Iterate over HistoryMessage objects (Issue #8 Refactor / Issue #18 Color Fix / Issue #26 Wrap+Select Fix)
       for (int i = 0; i < output_history.size(); ++i) {
           const auto& message = output_history[i];
           std::string display_text;
           ImVec4 text_color = ImGui::GetStyleColorVec4(ImGuiCol_Text); // Default color
           bool use_color = false;

           // Determine text and color based on message type
           if (message.type == MessageType::USER_INPUT) {
               text_color = (currentTheme == ThemeType::DARK) ? darkUserColor : lightUserColor;
               display_text = "User: " + message.content;
               use_color = true;
           } else if (message.type == MessageType::STATUS) {
               text_color = (currentTheme == ThemeType::DARK) ? darkStatusColor : lightStatusColor;
               display_text = "[STATUS] " + message.content;
               use_color = true;
           } else if (message.type == MessageType::ERROR) {
                // Keep default text color for errors
                display_text = "ERROR: " + message.content;
           } else if (message.type == MessageType::LLM_RESPONSE) {
                // Keep default text color for responses
                std::string prefix = "Assistant: "; // Default prefix
                // Check if model_id exists and fetch name/use ID
                if (message.model_id.has_value()) {
                    const std::string& actual_model_id = message.model_id.value();
                    if (actual_model_id == "UNKNOWN_LEGACY_MODEL_ID") {
                        prefix = "Assistant (Legacy Model): ";
                    } else {
                        std::optional<std::string> model_name_opt = db_manager.getModelNameById(actual_model_id);
                        if (model_name_opt.has_value() && !model_name_opt.value().empty()) {
                            prefix = "Assistant (" + model_name_opt.value() + "): "; // Use model name
                        } else {
                            prefix = "Assistant (" + actual_model_id + "): "; // Fallback to model ID
                        }
                    }
                }
                display_text = prefix + message.content;
           } else {
                // Handle potential unknown message types gracefully
                display_text = "[Unknown Type] " + message.content;
           }

           // Calculate selectable height based on wrapped text
           float wrap_width = ImGui::GetContentRegionAvail().x;
           ImVec2 text_size = ImGui::CalcTextSize(display_text.c_str(), NULL, false, wrap_width);
           // Use text_size.y directly, Selectable doesn't need extra FramePadding like InputTextMultiline
           float calculated_height = text_size.y;
            // Add a minimum height to prevent zero-height selectables for empty messages
           if (calculated_height < ImGui::GetTextLineHeight()) {
               calculated_height = ImGui::GetTextLineHeight();
           }


           // Create a unique ID for the selectable
           std::string selectable_id = "##msg_" + std::to_string(i);

           // Store cursor position before selectable to position text later
           ImVec2 text_pos = ImGui::GetCursorScreenPos();

           // Render the selectable area
           ImGui::Selectable(selectable_id.c_str(),
                             is_selecting && selecting_message_index == i, // Highlight if it's the one being selected
                             ImGuiSelectableFlags_AllowItemOverlap,        // Allow text to be drawn over it
                             ImVec2(wrap_width, calculated_height));

           // --- Input Handling for Selection (Phase 1: Drag Detection) ---
           if (ImGui::IsItemHovered()) {
               // Start selection on drag *within* this selectable
               if (ImGui::IsMouseDragging(0) && !is_selecting) { // Start drag only if not already selecting
                   is_selecting = true;
                   selecting_message_index = i;
                   selection_start_pos = ImGui::GetMousePos(); // Record start position
                   selection_start_char_index = -1; // Reset indices on new selection start
                   selection_end_char_index = -1;
                   // Prevent parent window scroll while dragging *within* the selectable
                   ImGui::SetScrollY(ImGui::GetScrollY());
               }
           }

           // Stop selection on mouse release (anywhere)
           if (!ImGui::IsMouseDown(0) && is_selecting) {
               // --- Clipboard Copy Logic (Phase 3) ---
               if (selecting_message_index == i && // Ensure this is the message that was being selected
                   selection_start_char_index != -1 &&
                   selection_end_char_index != -1 &&
                   selection_start_char_index < selection_end_char_index)
               {
                   // Extract the substring
                   std::string selected_substring = display_text.substr(
                       selection_start_char_index,
                       selection_end_char_index - selection_start_char_index
                   );

                   // Copy to clipboard
                   if (!selected_substring.empty()) {
                       ImGui::SetClipboardText(selected_substring.c_str());
                   }
               }
               // --- End Clipboard Copy Logic ---

               // Reset selection state *after* potential clipboard copy
               is_selecting = false;
               selecting_message_index = -1;
               selection_start_char_index = -1;
               selection_end_char_index = -1;
           }
           // --- End Input Handling ---

           // --- Selection Rendering & Coordinate Mapping (Phase 2 & 3 / Phase 5 Update) ---
           if (is_selecting && selecting_message_index == i) {
               ImVec2 current_mouse_pos = ImGui::GetMousePos();
               ImVec2 selectable_min = ImGui::GetItemRectMin(); // Bounds of the *last* item (the Selectable)
               ImVec2 selectable_max = ImGui::GetItemRectMax();

               // Determine selection rectangle corners, ensuring min is top-left and max is bottom-right
               ImVec2 rect_min = ImVec2(std::min(selection_start_pos.x, current_mouse_pos.x),
                                        std::min(selection_start_pos.y, current_mouse_pos.y));
               ImVec2 rect_max = ImVec2(std::max(selection_start_pos.x, current_mouse_pos.x),
                                        std::max(selection_start_pos.y, current_mouse_pos.y));

               // Clamp the selection rectangle to the bounds of the current selectable item
               rect_min.x = std::max(rect_min.x, selectable_min.x);
               rect_min.y = std::max(rect_min.y, selectable_min.y);
               rect_max.x = std::min(rect_max.x, selectable_max.x);
               rect_max.y = std::min(rect_max.y, selectable_max.y);

               // Only draw and map if the clamped rectangle is valid (min < max)
               if (rect_min.x < rect_max.x && rect_min.y < rect_max.y) {
                   // Phase 5: Declare vector to store character rects for this message
                   std::vector<ImRect> current_char_rects;

                   // --- Coordinate Mapping (Phase 3 / Phase 5 Update) ---
                   // Call the helper function to map screen coords to text indices AND get char rects
                   bool indices_found = MapScreenCoordsToTextIndices(
                       display_text.c_str(),
                       wrap_width,
                       selectable_min, // Pass the top-left corner of the selectable
                       rect_min,       // Pass the clamped selection rectangle
                       rect_max,
                       selection_start_char_index, // Update state variables
                       selection_end_char_index,
                       current_char_rects);        // Phase 5: Get char rects
                   // --- End Coordinate Mapping ---

                   // --- Selection Rendering (Phase 5: Per-Character Highlighting) ---
                   if (indices_found && selection_start_char_index != -1 && selection_end_char_index != -1 && selection_start_char_index < selection_end_char_index) {
                       ImDrawList* draw_list = ImGui::GetForegroundDrawList(); // Draw on top
                       for (int k = selection_start_char_index; k < selection_end_char_index; ++k) {
                           // Ensure index is valid before accessing (safety check)
                           if (k >= 0 && k < current_char_rects.size()) {
                                draw_list->AddRectFilled(current_char_rects[k].Min, current_char_rects[k].Max, ImGui::GetColorU32(ImGuiCol_TextSelectedBg));
                           }
                       }
                   }
                   // --- End Selection Rendering ---

               } else {
                    // If the rectangle becomes invalid (e.g., mouse moved outside), reset indices
                    selection_start_char_index = -1;
                    selection_end_char_index = -1;
               }
           }
           // --- End Selection Rendering & Coordinate Mapping ---

           // Render the text *over* the selectable area
           ImGui::SetCursorScreenPos(text_pos); // Reset cursor to where it was before the Selectable
           if (use_color) {
               ImGui::PushStyleColor(ImGuiCol_Text, text_color);
           }
           ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + wrap_width); // Ensure TextWrapped respects the width
           ImGui::TextWrapped("%s", display_text.c_str());
           ImGui::PopTextWrapPos();
           if (use_color) {
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

        ImGui::BeginDisabled(is_loading_models); // Disable input area if models are loading
        ImGui::PushItemWidth(input_width);
        enter_pressed = ImGui::InputText("##Input", gui_ui.getInputBuffer(), gui_ui.getInputBufferSize(), ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::PopItemWidth();
        ImGui::SameLine();
        send_pressed = ImGui::Button("Send", ImVec2(button_width, 0));
        ImGui::EndDisabled(); // End disabling input area

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
        glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
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

    // --- Save Font Size (Issue #19 Persistence) ---
    try {
        float final_font_size = gui_ui.getCurrentFontSize();
        // Convert float to string for saving
        std::string font_size_str = std::to_string(final_font_size);
        // Remove trailing zeros after decimal point for cleaner storage
        font_size_str.erase(font_size_str.find_last_not_of('0') + 1, std::string::npos);
        if (font_size_str.back() == '.') {
            font_size_str.pop_back(); // Remove trailing decimal point if it exists (e.g., "18.")
        }
        db_manager.saveSetting("font_size", font_size_str);
    } catch (const std::exception& e) {
        std::cerr << "Warning: Failed to save font_size setting: " << e.what() << std::endl;
    }
    // --- End Save Font Size ---
 
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
