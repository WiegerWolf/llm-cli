#include <gui/views/main_gui_views.h>
#include <gui/views/gui_interface.h>
#include <graph/utils/graph_manager.h>
#include <graph/render/graph_renderer.h>
#include <gui/render/font_utils.h>
#include <gui/render/theme_utils.h>
#include <core/id_types.h>

#include <imgui.h>
#include <imgui_internal.h>

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include <memory>

// This global is required by both the core (for initialization) and views (for rendering).
extern std::unique_ptr<GraphManager> g_graph_manager;

// This is used when creating user messages directly in the UI before sending to the worker.
static std::atomic<NodeIdType> g_next_message_id {1};

// --- View-specific state ---
static bool s_is_graph_view_visible = true;
static float s_animation_speed = 1.0f;

// `currentTheme` is managed in the core loop but needed for rendering decisions.
extern ThemeType currentTheme;
const ImVec4 darkUserColor = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);
const ImVec4 darkStatusColor = ImVec4(1.0f, 1.0f, 0.0f, 1.0f);
const ImVec4 lightUserColor = ImVec4(0.0f, 0.5f, 0.0f, 1.0f);
const ImVec4 lightStatusColor = ImVec4(0.8f, 0.4f, 0.0f, 1.0f);

// These are managed by the core loop, but views need to read them.
extern std::vector<HistoryMessage> output_history;
extern bool new_output_added;
extern bool request_input_focus;
extern std::atomic<bool> is_loading_models;


// --- Helper Functions for UI Rendering (Copied from original main_gui.cpp) ---

std::string FormatMessageForGraph(const HistoryMessage& msg, GraphManager& graph_manager) {
    std::string content_prefix;
    switch (msg.type) {
        case MessageType::USER_INPUT: content_prefix = "User: "; break;
        case MessageType::LLM_RESPONSE: {
            std::string prefix = "Assistant: ";
            if (msg.model_id.has_value()) {
                const std::string& actual_model_id = msg.model_id.value();
                if (actual_model_id == "UNKNOWN_LEGACY_MODEL_ID") {
                    prefix = "Assistant (Legacy Model): ";
                } else {
                    std::string model_name = graph_manager.getModelName(actual_model_id);
                    if (!model_name.empty()) prefix = "Assistant (" + model_name + "): ";
                    else prefix = "Assistant (" + actual_model_id + "): ";
                }
            }
            content_prefix = prefix;
            break;
        }
        case MessageType::STATUS: content_prefix = "[STATUS] "; break;
        case MessageType::ERROR: content_prefix = "ERROR: "; break;
        case MessageType::USER_REPLY: content_prefix = "Reply: "; break;
        default: content_prefix = "[Unknown Type] "; break;
    }
    return content_prefix + msg.content;
}

static std::optional<std::chrono::system_clock::time_point> parse_timestamp(const std::string& ts_str) {
    std::tm t{};
    std::istringstream ss(ts_str);
    if (ts_str.find('T') != std::string::npos) { ss >> std::get_time(&t, "%Y-%m-%dT%H:%M:%S"); }
    else { ss >> std::get_time(&t, "%Y-%m-%d %H:%M:%S"); }

    if (ss.fail()) {
        if (ts_str.find('T') != std::string::npos && !ts_str.empty() && ts_str.back() == 'Z') {
            std::string SuffixlessStr = ts_str.substr(0, ts_str.length() -1);
            std::istringstream ss_retry(SuffixlessStr);
            ss_retry >> std::get_time(&t, "%Y-%m-%dT%H:%M:%S");
            if(ss_retry.fail()){ return std::nullopt; }
        } else { return std::nullopt; }
    }
    t.tm_isdst = -1;
    std::time_t time_c = std::mktime(&t);
    if (time_c == -1) { return std::nullopt; }
    return std::chrono::system_clock::from_time_t(time_c);
}

static bool is_model_new_or_updated(const ModelData& model_data) {
    auto now = std::chrono::system_clock::now();
    auto seven_days_ago_tp = now - std::chrono::hours(24 * 7);
    if (model_data.created_at_api != 0) {
        auto created_at_tp = std::chrono::system_clock::from_time_t(static_cast<time_t>(model_data.created_at_api));
        if (created_at_tp > seven_days_ago_tp) return true;
    }
    if (!model_data.last_updated_db.empty()) {
        auto updated_at_db_tp_opt = parse_timestamp(model_data.last_updated_db);
        if (updated_at_db_tp_opt && *updated_at_db_tp_opt > seven_days_ago_tp) return true;
    }
    return false;
}

static std::string get_modality_icon_str(const ModelData& model_data) {
    bool input_text = model_data.architecture_input_modalities.find("\"text\"") != std::string::npos;
    bool input_image = model_data.architecture_input_modalities.find("\"image\"") != std::string::npos;
    bool output_text = model_data.architecture_output_modalities.find("\"text\"") != std::string::npos;
    bool output_image = model_data.architecture_output_modalities.find("\"image\"") != std::string::npos;
    int input_modal_count = (input_text ? 1 : 0) + (input_image ? 1 : 0);
    int output_modal_count = (output_text ? 1 : 0) + (output_image ? 1 : 0);
    if (input_text && output_text && input_modal_count == 1 && output_modal_count == 1) return "\U0001F4DD\u27A1\U0001F4DD";
    if (input_image && output_text && input_modal_count == 1 && output_modal_count == 1) return "\U0001F5BC\uFE0F\u27A1\U0001F4DD";
    if (input_text && output_image && input_modal_count == 1 && output_modal_count == 1) return "\U0001F4DD\u27A1\U0001F5BC\uFE0F";
    if (input_modal_count > 1 || output_modal_count > 1) return "\U0001F504";
    return "";
}

static std::string format_price_per_million(const std::string& price_str) {
    if (price_str.empty() || price_str == "N/A") return "N/A";
    try {
        std::string numeric_price_str;
        for (char ch : price_str) { if (std::isdigit(ch) || ch == '.' || ch == '-') { numeric_price_str += ch; } }
        if (numeric_price_str.empty()) return "N/A";
        double price = std::stod(numeric_price_str);
        double price_per_million = price * 1000000.0;
        std::ostringstream oss;
        oss << "$" << std::fixed << std::setprecision(2) << price_per_million;
        return oss.str();
    } catch (const std::exception&) { return "N/A"; }
}

static std::string format_context_size(int context_length) {
    if (context_length <= 0) return "N/A";
    std::ostringstream oss;
    if (context_length < 1000) { oss << context_length; }
    else if (context_length < 1000000) { oss << static_cast<int>(std::round(static_cast<double>(context_length) / 1000.0)) << "k"; }
    else {
        double millions = static_cast<double>(context_length) / 1000000.0;
        if (std::fabs(millions - std::round(millions)) < 0.05) oss << static_cast<int>(std::round(millions)) << "M";
        else oss << std::fixed << std::setprecision(1) << millions << "M";
    }
    return oss.str();
}

bool MapScreenCoordsToTextIndices(
    const char* text, float wrap_width, const ImVec2& selectable_min,
    const ImVec2& selection_rect_min, const ImVec2& selection_rect_max,
    int& out_start_index, int& out_end_index, std::vector<ImRect>& out_char_rects)
{
    out_start_index = -1; out_end_index = -1; out_char_rects.clear();
    if (!text || text[0] == '\0' || wrap_width <= 0) return false;
    if (selection_rect_min.x >= selection_rect_max.x || selection_rect_min.y >= selection_rect_max.y) return false;

    ImGuiContext& g = *GImGui; const float line_height = g.FontSize;
    ImVec2 cursor_pos = selectable_min;
    int text_len = static_cast<int>(strlen(text));
    out_char_rects.reserve(text_len);

    const char* current_char_ptr = text; const char* text_end = text + text_len;
    while (current_char_ptr < text_end) {
        unsigned int c; int char_bytes = ImTextCharFromUtf8(&c, current_char_ptr, text_end);
        const char* next_char_ptr = (char_bytes > 0) ? (current_char_ptr + char_bytes) : (current_char_ptr + 1);
        if (next_char_ptr > text_end) next_char_ptr = text_end;
        ImVec2 char_size = ImGui::CalcTextSize(current_char_ptr, next_char_ptr, false, 0.0f);
        if (cursor_pos.x > selectable_min.x && (cursor_pos.x + char_size.x) > (selectable_min.x + wrap_width)) {
            cursor_pos.x = selectable_min.x; cursor_pos.y += line_height;
        }
        out_char_rects.push_back(ImRect(cursor_pos, ImVec2(cursor_pos.x + char_size.x, cursor_pos.y + line_height)));
        cursor_pos.x += char_size.x;
        current_char_ptr = next_char_ptr;
    }
    int first_idx = -1, last_idx = -1;
    ImRect sel_rect(selection_rect_min, selection_rect_max);
    for (int k = 0; k < out_char_rects.size(); ++k) {
        if (out_char_rects[k].Overlaps(sel_rect)) {
             if (first_idx == -1) first_idx = k;
            last_idx = k;
        }
    }
    if (first_idx != -1) { out_start_index = first_idx; out_end_index = last_idx + 1; return true; }
    return false;
}

// --- Main Drawing Function ---
void drawAllViews(GraphManager& gm, GuiInterface& gui) {
    ImVec2 scroll_offsets = gui.getAndClearScrollOffsets();
    static std::vector<GuiInterface::ModelEntry> available_models_list;
    static std::string current_gui_selected_model_id;
    static int current_gui_selected_model_idx = -1;

    const ImVec2 display_size = ImGui::GetIO().DisplaySize;
    const float input_height = 35.0f;
    float settings_height = 0.0f;

    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f)); ImGui::SetNextWindowSize(display_size);
    ImGui::Begin("Main", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus);

    if (ImGui::CollapsingHeader("Settings")) {
        settings_height = ImGui::GetItemRectSize().y;
        ImGui::Indent();
        ImGui::Text("Theme:"); ImGui::SameLine();
        if (ImGui::RadioButton("Dark", currentTheme == ThemeType::DARK)) {
            currentTheme = ThemeType::DARK; ThemeUtils::setTheme(gui, currentTheme);
            try { gui.getDbManager().saveSetting("theme", "DARK"); } catch (const std::exception& e) { std::cerr << "Warning: Failed to save theme: " << e.what() << std::endl; }
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("White", currentTheme == ThemeType::WHITE)) {
            currentTheme = ThemeType::WHITE; ThemeUtils::setTheme(gui, currentTheme);
            try { gui.getDbManager().saveSetting("theme", "WHITE"); } catch (const std::exception& e) { std::cerr << "Warning: Failed to save theme: " << e.what() << std::endl; }
        }
        ImGui::Unindent();
        settings_height += ImGui::GetItemRectSize().y;

        ImGui::Indent();
        if (is_loading_models) {
            ImGui::Text("Loading models...");
            settings_height += ImGui::GetTextLineHeightWithSpacing();
        } else {
            available_models_list = gui.getAvailableModelsForUI();
            current_gui_selected_model_id = gui.getSelectedModelIdFromUI();
            current_gui_selected_model_idx = -1;
            for (int i = 0; i < available_models_list.size(); ++i) if (available_models_list[i].id == current_gui_selected_model_id) { current_gui_selected_model_idx = i; break; }
            if (current_gui_selected_model_idx == -1 && !available_models_list.empty()) { current_gui_selected_model_idx = 0; current_gui_selected_model_id = available_models_list[0].id; }

            if (available_models_list.empty()) {
                ImGui::Text("No models available.");
                settings_height += ImGui::GetTextLineHeightWithSpacing();
            } else {
                ImGui::BeginDisabled(is_loading_models);
                const char* combo_preview = (current_gui_selected_model_idx != -1) ? available_models_list[current_gui_selected_model_idx].name.c_str() : "Select a Model";
                if (ImGui::BeginCombo("Active Model", combo_preview)) {
                    ImFont *main_font = gui.GetMainFont(), *small_font = gui.GetSmallFont();
                    for (int i = 0; i < available_models_list.size(); ++i) {
                        const auto& entry = available_models_list[i];
                        std::optional<ModelData> model_data = gui.getDbManager().getModelById(entry.id);
                        float line1_h = main_font ? main_font->FontSize : ImGui::GetTextLineHeight();
                        float line3_h = small_font ? small_font->FontSize : ImGui::GetTextLineHeightWithSpacing() * 0.8f;
                        float total_h = line1_h + line3_h + ImGui::GetStyle().ItemSpacing.y;

                        if (ImGui::Selectable(("##" + entry.id).c_str(), current_gui_selected_model_idx == i, 0, ImVec2(0, total_h))) {
                            current_gui_selected_model_idx = i;
                            current_gui_selected_model_id = entry.id;
                            gui.setSelectedModelInUI(entry.id);
                        }
                        if (current_gui_selected_model_idx == i) ImGui::SetItemDefaultFocus();
                        ImVec2 item_pos = ImGui::GetItemRectMin();
                        ImDrawList* draw_list = ImGui::GetWindowDrawList();
                        if (model_data) {
                            std::string icons; if (is_model_new_or_updated(*model_data)) icons += " \u2728"; if (model_data->top_provider_is_moderated) icons += " \U0001F512"; std::string mi = get_modality_icon_str(*model_data); if (!mi.empty()) icons += " " + mi;
                            std::string line1 = model_data->name + icons;
                            std::string line3 = "Context: " + format_context_size(model_data->context_length) + " | P: " + format_price_per_million(model_data->pricing_prompt) + "/1M | C: " + format_price_per_million(model_data->pricing_completion) + "/1M tokens";
                            if (main_font) ImGui::PushFont(main_font); draw_list->AddText(item_pos, ImGui::GetColorU32(ImGuiCol_Text), line1.c_str()); if (main_font) ImGui::PopFont();
                            item_pos.y += line1_h + ImGui::GetStyle().ItemSpacing.y;
                            if (small_font) ImGui::PushFont(small_font); draw_list->AddText(item_pos, ImGui::GetColorU32(ImGuiCol_Text), line3.c_str()); if (small_font) ImGui::PopFont();
                        } else { draw_list->AddText(item_pos, ImGui::GetColorU32(ImGuiCol_Text), entry.name.c_str()); }
                    }
                    ImGui::EndCombo();
                }
                ImGui::EndDisabled();
                settings_height += ImGui::GetItemRectSize().y;
            }
        }
        ImGui::Unindent();
        ImGui::Indent();
        ImGui::Checkbox("Show Graph View", &s_is_graph_view_visible);
        ImGui::Unindent();
        settings_height += ImGui::GetItemRectSize().y;
        settings_height += ImGui::GetStyle().ItemSpacing.y;
    } else { settings_height = ImGui::GetItemRectSize().y; }

    float spacing = ImGui::GetStyle().ItemSpacing.y > 0 ? ImGui::GetStyle().ItemSpacing.y : 8.0f;
    const float bottom_h = input_height + settings_height + spacing;

    if (ImGui::BeginTabBar("ViewModeTabBar", ImGuiTabBarFlags_None)) {
        if (ImGui::BeginTabItem("Linear View")) {
            ImGui::BeginChild("Output", ImVec2(0, -bottom_h), true);
            static bool is_selecting = false; static int sel_msg_idx = -1;
            static ImVec2 sel_start_pos; static int sel_start_char = -1, sel_end_char = -1;
            for (int i = 0; i < output_history.size(); ++i) {
                const auto& msg = output_history[i];
                std::string text = FormatMessageForGraph(msg, gm);
                ImVec4 color = ImGui::GetStyleColorVec4(ImGuiCol_Text); bool use_color = false;
                if (msg.type == MessageType::USER_INPUT) { color = (currentTheme == ThemeType::DARK) ? darkUserColor : lightUserColor; use_color = true; }
                else if (msg.type == MessageType::STATUS) { color = (currentTheme == ThemeType::DARK) ? darkStatusColor : lightStatusColor; use_color = true; }
                float wrap_w = ImGui::GetContentRegionAvail().x;
                ImVec2 text_size = ImGui::CalcTextSize(text.c_str(), NULL, false, wrap_w);
                float calc_h = (text_size.y < ImGui::GetTextLineHeight()) ? ImGui::GetTextLineHeight() : text_size.y;
                ImVec2 text_pos = ImGui::GetCursorScreenPos();
                ImGui::Selectable(("##msg_" + std::to_string(i)).c_str(), is_selecting && sel_msg_idx == i, ImGuiSelectableFlags_AllowItemOverlap, ImVec2(wrap_w, calc_h));
                if (ImGui::IsItemHovered() && ImGui::IsMouseDragging(0) && !is_selecting) { is_selecting = true; sel_msg_idx = i; sel_start_pos = ImGui::GetMousePos(); sel_start_char = -1; sel_end_char = -1; }
                if (!ImGui::IsMouseDown(0) && is_selecting) { if (sel_msg_idx == i && sel_start_char != -1 && sel_end_char != -1 && sel_start_char < sel_end_char) { ImGui::SetClipboardText(text.substr(sel_start_char, sel_end_char - sel_start_char).c_str()); } is_selecting = false; sel_msg_idx = -1; }
                if (is_selecting && sel_msg_idx == i) {
                    ImVec2 m = ImGui::GetMousePos(), s_min = ImGui::GetItemRectMin(), s_max = ImGui::GetItemRectMax();
                    ImVec2 r_min(std::min(sel_start_pos.x,m.x), std::min(sel_start_pos.y,m.y)), r_max(std::max(sel_start_pos.x,m.x), std::max(sel_start_pos.y,m.y));
                    r_min.x=std::max(r_min.x,s_min.x); r_min.y=std::max(r_min.y,s_min.y); r_max.x=std::min(r_max.x,s_max.x); r_max.y=std::min(r_max.y,s_max.y);
                    if (r_min.x < r_max.x && r_min.y < r_max.y) {
                        std::vector<ImRect> char_rects;
                        if (MapScreenCoordsToTextIndices(text.c_str(), wrap_w, s_min, r_min, r_max, sel_start_char, sel_end_char, char_rects)) {
                           ImDrawList* dl = ImGui::GetForegroundDrawList();
                           for (int k = sel_start_char; k < sel_end_char; ++k) if (k < char_rects.size()) dl->AddRectFilled(char_rects[k].Min, char_rects[k].Max, ImGui::GetColorU32(ImGuiCol_TextSelectedBg));
                        }
                    } else { sel_start_char = -1; sel_end_char = -1; }
                }
                ImGui::SetCursorScreenPos(text_pos);
                if (use_color) ImGui::PushStyleColor(ImGuiCol_Text, color);
                ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + wrap_w);
                ImGui::TextWrapped("%s", text.c_str());
                ImGui::PopTextWrapPos();
                if (use_color) ImGui::PopStyleColor();
            }
            if (new_output_added && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 10.0f) { ImGui::SetScrollHereY(1.0f); new_output_added = false; }
            if (scroll_offsets.x != 0.0f || scroll_offsets.y != 0.0f) { ImGui::SetScrollY(ImGui::GetScrollY() + scroll_offsets.y); ImGui::SetScrollX(ImGui::GetScrollX() + scroll_offsets.x); }
            ImGui::EndChild();
            ImGui::EndTabItem();
        }
        if (s_is_graph_view_visible && ImGui::BeginTabItem("Graph View")) {
            static bool tab_was_active = false; bool tab_is_active = ImGui::IsItemVisible();
            if (tab_is_active && (!tab_was_active || gm.GetAllNodes().empty())) { if (!output_history.empty()) gm.PopulateGraphFromHistory(output_history, gui.getDbManager()); }
            tab_was_active = tab_is_active;
            if (ImGui::Button("Refresh Graph")) gm.PopulateGraphFromHistory(output_history, gui.getDbManager());
            ImGui::SameLine(); ImGui::Text("Nodes: %zu", gm.GetAllNodes().size()); ImGui::Separator();
            ImGui::Text("Animation Controls:");
            if (gm.IsLayoutRunning()) { if (gm.isAnimationPaused()) { if (ImGui::Button("Resume")) gm.setAnimationPaused(false); } else { if (ImGui::Button("Pause")) gm.setAnimationPaused(true); } }
            else { if (ImGui::Button("Start Animation")) { gm.RestartLayoutAnimation(); gm.setAnimationPaused(false); } }
            ImGui::SameLine(); if (ImGui::Button("Reset Layout")) { gm.RestartLayoutAnimation(); gm.setAnimationPaused(false); }
            ImGui::Text("Speed:"); ImGui::SameLine(); ImGui::SetNextItemWidth(150.0f);
            ImGui::SliderFloat("##AnimSpeed", &s_animation_speed, 0.1f, 3.0f, "%.1fx");
            ImGui::SameLine();
            if (gm.IsLayoutRunning()) { if (gm.isAnimationPaused()) ImGui::TextColored({1,1,0,1}, "PAUSED"); else ImGui::TextColored({0,1,0,1}, "RUNNING"); }
            else { ImGui::TextColored({0.5,0.5,0.5,1}, "STOPPED"); }
            ImGui::BeginChild("GraphCanvas", ImVec2(0, -bottom_h - ImGui::GetFrameHeightWithSpacing()), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
            RenderGraphView(gm, currentTheme, false);
            gm.SetAnimationSpeed(s_animation_speed);
            ImGui::EndChild();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    const float button_width = 60.0f;
    float input_width = ImGui::GetContentRegionAvail().x - button_width - ImGui::GetStyle().ItemSpacing.x;
    if (request_input_focus) { ImGui::SetKeyboardFocusHere(); request_input_focus = false; }
    ImGui::BeginDisabled(is_loading_models);
    ImGui::PushItemWidth(input_width);
    if (ImGui::InputText("##Input", gui.getInputBuffer(), gui.getInputBufferSize(), ImGuiInputTextFlags_EnterReturnsTrue) || ImGui::Button("Send", ImVec2(button_width, 0))) {
        char* input_buf = gui.getInputBuffer();
        if (input_buf[0] != '\0') {
            gui.sendInputToWorker(input_buf);
            HistoryMessage user_msg;
            user_msg.message_id = g_next_message_id.fetch_add(1);
            user_msg.type = MessageType::USER_INPUT;
            user_msg.content = std::string(input_buf);
            output_history.push_back(user_msg);
            gm.HandleNewHistoryMessage(user_msg, gm.GetSelectedNodeId(), gui.getDbManager());
            gm.RestartLayoutAnimation();
            new_output_added = true;
            input_buf[0] = '\0';
            request_input_focus = true;
        }
    }
    ImGui::PopItemWidth();
    ImGui::SameLine();
    ImGui::EndDisabled();

    ImGui::End();
}