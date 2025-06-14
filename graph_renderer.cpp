#include "graph_renderer.h"
#include "extern/imgui/imgui_internal.h" // For ImGui::CalcTextSize, ImGui::PushClipRect, ImTextCharToUtf8
#include "id_types.h"                    // NodeIdType definition
#include <string>     // For std::string, std::to_string
#include <algorithm>  // For std::min, std::max
#include <limits>     // Required for std::numeric_limits
#include <cstdio>     // For sprintf
#include <cinttypes>  // For PRIu64
#include <cstring>    // For strlen
#include <cmath>      // For sqrtf, FLT_MAX, powf, fabsf
#include <chrono>     // For std::chrono::system_clock, std::chrono::steady_clock
#include <functional> // For std::function
// Initialize the static member for the new message buffer
char GraphEditor::newMessageBuffer_[1024 * 16] = {0};
#include "graph_manager.h" // For GraphManager
#include "graph_layout.h" // For CalculateNodePositionsRecursive

// Helper to add text with truncation using ImFont::CalcWordWrapPositionA
static void AddTextTruncated(ImDrawList* draw_list, ImFont* font, float font_size, const ImVec2& pos, ImU32 col, const char* text_begin, const char* text_end, float wrap_width, const ImVec4* cpu_fine_clip_rect) {
    if (text_begin == text_end || font_size <= 0.0f || font == nullptr)
        return;

    if (wrap_width <= 0.0f) {
        draw_list->AddText(font, font_size, pos, col, text_begin, text_end, 0.0f, cpu_fine_clip_rect);
        return;
    }

    float scale = font->FontSize > 0 ? font_size / font->FontSize : 1.0f;
    const char* end_of_fit_text_ptr = font->CalcWordWrapPositionA(scale, text_begin, text_end, wrap_width);

    if (end_of_fit_text_ptr == text_begin) {
        const char* ellipsis = "...";
        ImVec2 ellipsis_size = font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, ellipsis);
        if (ellipsis_size.x <= wrap_width) {
            draw_list->AddText(font, font_size, pos, col, ellipsis, ellipsis + strlen(ellipsis), 0.0f, cpu_fine_clip_rect);
        }
        return;
    }

    if (end_of_fit_text_ptr == text_end) {
        draw_list->AddText(font, font_size, pos, col, text_begin, text_end, 0.0f, cpu_fine_clip_rect);
        return;
    }

    const char* ellipsis = "...";
    ImVec2 ellipsis_size = font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, ellipsis);
    float width_for_text_before_ellipsis = wrap_width - ellipsis_size.x;
    std::string final_text_str;

    if (width_for_text_before_ellipsis > 0) {
        const char* end_of_text_part_ptr = font->CalcWordWrapPositionA(scale, text_begin, text_end, width_for_text_before_ellipsis);
        if (end_of_text_part_ptr > text_begin) {
            final_text_str.assign(text_begin, end_of_text_part_ptr);
            final_text_str += ellipsis;
        } else {
            if (ellipsis_size.x <= wrap_width) {
                final_text_str = ellipsis;
            }
        }
    } else {
        if (ellipsis_size.x <= wrap_width) {
            final_text_str = ellipsis;
        }
    }

    if (!final_text_str.empty()) {
        draw_list->AddText(font, font_size, pos, col, final_text_str.c_str(), final_text_str.c_str() + final_text_str.length(), 0.0f, cpu_fine_clip_rect);
    }
}

// Easing functions for smooth camera animation
namespace CameraEasing {
    // Ease-out cubic function for natural deceleration
    float EaseOutCubic(float t) {
        return 1.0f - powf(1.0f - t, 3.0f);
    }
    
    // Ease-in-out cubic function for smooth start and end
    float EaseInOutCubic(float t) {
        return t < 0.5f ? 4.0f * t * t * t : 1.0f - powf(-2.0f * t + 2.0f, 3.0f) / 2.0f;
    }
    
    // Linear interpolation between two values
    float Lerp(float a, float b, float t) {
        return a + t * (b - a);
    }
    
    // Linear interpolation between two ImVec2 values
    ImVec2 LerpVec2(const ImVec2& a, const ImVec2& b, float t) {
        return ImVec2(Lerp(a.x, b.x, t), Lerp(a.y, b.y, t));
    }
}

// Helper function to render wrapped text within a bounded area
static void RenderWrappedText(ImDrawList* draw_list, ImFont* font, float font_size, const ImVec2& pos, ImU32 col, const char* text, float wrap_width, float max_height, const ImVec4* cpu_fine_clip_rect) {
    if (!text || font_size <= 0.0f || font == nullptr || wrap_width <= 10.0f) // Ensure minimum wrap width
        return;

    // Force immediate text rendering by bypassing any potential caching issues
    // This ensures new message content is displayed immediately without manual refresh
    
    // Use ImGui's built-in text wrapping which is more reliable
    ImVec2 text_size = ImGui::CalcTextSize(text, nullptr, false, wrap_width);
    
    // If the text fits in one line and within bounds, render it directly
    if (text_size.y <= max_height) {
        // Force immediate rendering with explicit parameters to avoid caching issues
        // Use AddText with explicit text_end parameter to ensure fresh rendering
        const char* text_end = text + strlen(text);
        draw_list->AddText(font, font_size, pos, col, text, text_end, wrap_width, cpu_fine_clip_rect);
        return;
    }
    
    // For multi-line text that exceeds max_height, we need to truncate
    float scale = font->FontSize > 0 ? font_size / font->FontSize : 1.0f;
    float line_height = font_size * 1.2f;
    const int max_lines = std::max(1, (int)(max_height / line_height));
    
    const char* text_ptr = text;
    const char* text_end = text + strlen(text);
    ImVec2 current_pos = pos;
    int line_count = 0;
    
    while (text_ptr < text_end && line_count < max_lines) {
        // Handle explicit newlines first
        const char* newline_pos = text_ptr;
        while (newline_pos < text_end && *newline_pos != '\n') {
            newline_pos++;
        }
        
        // Find the end of the current line that fits within wrap_width
        const char* line_end = font->CalcWordWrapPositionA(scale, text_ptr, newline_pos, wrap_width);
        
        if (line_end == text_ptr && text_ptr < text_end) {
            // If we can't fit even one character, force at least one character
            line_end = text_ptr + 1;
            if (line_end > text_end) line_end = text_end;
        }
        
        // Check if this is the last line we can fit and there's more text
        bool is_last_line = (line_count == max_lines - 1) && (line_end < text_end || (text_ptr < text_end && *text_ptr == '\n'));
        
        if (is_last_line) {
            // Reserve space for ellipsis
            const char* ellipsis = "...";
            ImVec2 ellipsis_size = font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, ellipsis);
            float available_width = wrap_width - ellipsis_size.x;
            
            if (available_width > 0) {
                line_end = font->CalcWordWrapPositionA(scale, text_ptr, newline_pos, available_width);
                if (line_end == text_ptr && text_ptr < text_end) {
                    line_end = text_ptr + 1;
                    if (line_end > text_end) line_end = text_end;
                }
            }
            
            // Render the truncated line with explicit parameters to force fresh rendering
            if (line_end > text_ptr) {
                draw_list->AddText(font, font_size, current_pos, col, text_ptr, line_end, 0.0f, cpu_fine_clip_rect);
            }
            
            // Add ellipsis with explicit text_end to force fresh rendering
            ImVec2 ellipsis_pos = ImVec2(current_pos.x + font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, text_ptr, line_end).x, current_pos.y);
            const char* ellipsis_end = ellipsis + strlen(ellipsis);
            draw_list->AddText(font, font_size, ellipsis_pos, col, ellipsis, ellipsis_end, 0.0f, cpu_fine_clip_rect);
            break;
        } else {
            // Render this line normally with explicit text_end to force fresh rendering
            if (line_end > text_ptr) {
                draw_list->AddText(font, font_size, current_pos, col, text_ptr, line_end, 0.0f, cpu_fine_clip_rect);
            }
        }
        
        // Move to next line
        current_pos.y += line_height;
        line_count++;
        text_ptr = line_end;
        
        // Skip whitespace at the beginning of the next line (but not newlines)
        while (text_ptr < text_end && (*text_ptr == ' ' || *text_ptr == '\t')) {
            text_ptr++;
        }
        
        // Handle explicit newlines
        if (text_ptr < text_end && *text_ptr == '\n') {
            text_ptr++;
            // Skip additional whitespace after newline
            while (text_ptr < text_end && (*text_ptr == ' ' || *text_ptr == '\t')) {
                text_ptr++;
            }
        }
    }
}

// Theme-aware color functions
ImU32 GraphEditor::GetThemeNodeColor(ThemeType theme) const {
    switch (theme) {
        case ThemeType::DARK:
            return IM_COL32(45, 45, 50, 255);  // Dark grey for dark theme
        case ThemeType::WHITE:
            return IM_COL32(240, 240, 245, 255); // Light grey for white theme
        default:
            return IM_COL32(200, 200, 200, 255); // Fallback
    }
}

ImU32 GraphEditor::GetThemeNodeBorderColor(ThemeType theme) const {
    switch (theme) {
        case ThemeType::DARK:
            return IM_COL32(80, 80, 85, 255);   // Lighter grey border for dark theme
        case ThemeType::WHITE:
            return IM_COL32(180, 180, 185, 255); // Darker grey border for white theme
        default:
            return IM_COL32(100, 100, 100, 255); // Fallback
    }
}

ImU32 GraphEditor::GetThemeNodeSelectedBorderColor(ThemeType theme) const {
    switch (theme) {
        case ThemeType::DARK:
            return IM_COL32(255, 255, 0, 255);   // Bright yellow for dark theme
        case ThemeType::WHITE:
            return IM_COL32(255, 165, 0, 255);   // Orange for white theme (better contrast)
        default:
            return IM_COL32(255, 255, 0, 255);   // Fallback
    }
}

ImU32 GraphEditor::GetThemeEdgeColor(ThemeType theme) const {
    switch (theme) {
        case ThemeType::DARK:
            return IM_COL32(120, 120, 125, 255); // Medium grey for dark theme
        case ThemeType::WHITE:
            return IM_COL32(100, 100, 105, 255); // Darker grey for white theme
        default:
            return IM_COL32(150, 150, 150, 255); // Fallback
    }
}

ImU32 GraphEditor::GetThemeBackgroundColor(ThemeType theme) const {
    switch (theme) {
        case ThemeType::DARK:
            return IM_COL32(25, 25, 30, 255);    // Very dark background
        case ThemeType::WHITE:
            return IM_COL32(250, 250, 255, 255); // Very light background
        default:
            return IM_COL32(30, 30, 40, 255);    // Fallback
    }
}

ImU32 GraphEditor::GetThemeTextColor(ThemeType theme) const {
    switch (theme) {
        case ThemeType::DARK:
            return IM_COL32(220, 220, 225, 255); // Light text for dark theme
        case ThemeType::WHITE:
            return IM_COL32(30, 30, 35, 255);    // Dark text for white theme
        default:
            return IM_COL32(255, 255, 255, 255); // Fallback
    }
}

ImU32 GraphEditor::GetThemeExpandCollapseIconColor(ThemeType theme) const {
    switch (theme) {
        case ThemeType::DARK:
            return IM_COL32(180, 180, 185, 255); // Light grey icon for dark theme
        case ThemeType::WHITE:
            return IM_COL32(80, 80, 85, 255);    // Dark grey icon for white theme
        default:
            return IM_COL32(200, 200, 200, 255); // Fallback
    }
}


GraphEditor::GraphEditor(GraphManager* graph_manager) : m_graph_manager(graph_manager) {
    // context_node_ and reply_parent_node_ are initialized to nullptr by default
}

NodeIdType GetNextUniqueID(const std::map<NodeIdType, std::shared_ptr<GraphNode>>& nodes) {
    if (nodes.empty()) {
        return 1;
    }
    NodeIdType max_id = 0;
    for(const auto& pair : nodes) {
        if (pair.first > max_id) {
            max_id = pair.first;
        }
    }
    return max_id + 1;
}

void GraphEditor::AddNode(std::shared_ptr<GraphNode> node) {
    if (node) {
        nodes_[node->graph_node_id] = node; // Use graph_node_id as the unique key
    }
}

std::shared_ptr<GraphNode> GraphEditor::GetNode(NodeIdType node_id) {
    if (node_id == kInvalidNodeId) return nullptr;
    auto it = nodes_.find(node_id);
    if (it != nodes_.end()) {
        return it->second;
    }
    return nullptr;
}

void GraphEditor::ClearNodes() {
    nodes_.clear();
}

ImVec2 GraphEditor::WorldToScreen(const ImVec2& world_pos, const GraphViewState& view_state) const {
    return ImVec2((world_pos.x * view_state.zoom_scale) + view_state.pan_offset.x,
                  (world_pos.y * view_state.zoom_scale) + view_state.pan_offset.y);
}

ImVec2 GraphEditor::ScreenToWorld(const ImVec2& screen_pos_absolute, const ImVec2& canvas_screen_pos_absolute, const GraphViewState& view_state) const {
    ImVec2 mouse_relative_to_canvas_origin = ImVec2(screen_pos_absolute.x - canvas_screen_pos_absolute.x,
                                                   screen_pos_absolute.y - canvas_screen_pos_absolute.y);

    if (view_state.zoom_scale == 0.0f) return ImVec2(0,0); // Avoid division by zero
    return ImVec2((mouse_relative_to_canvas_origin.x - view_state.pan_offset.x) / view_state.zoom_scale,
                  (mouse_relative_to_canvas_origin.y - view_state.pan_offset.y) / view_state.zoom_scale);
}

void GraphEditor::HandlePanning(const ImVec2& canvas_screen_pos, const ImVec2& canvas_size, GraphViewState& view_state) {
    ImGuiIO& io = ImGui::GetIO();
    if (io.MousePos.x >= canvas_screen_pos.x && io.MousePos.x <= canvas_screen_pos.x + canvas_size.x &&
        io.MousePos.y >= canvas_screen_pos.y && io.MousePos.y <= canvas_screen_pos.y + canvas_size.y) {
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
            // Cancel auto-pan if user starts manual panning
            if (view_state.auto_pan_active) {
                CancelAutoPan(view_state);
            }
            view_state.user_interrupted_auto_pan = true;
            view_state.pan_offset.x += ImGui::GetIO().MouseDelta.x;
            view_state.pan_offset.y += ImGui::GetIO().MouseDelta.y;
        }
    }
}

void GraphEditor::HandleZooming(const ImVec2& canvas_screen_pos, const ImVec2& canvas_size, GraphViewState& view_state) {
    ImGuiIO& io = ImGui::GetIO();
     if (io.MousePos.x >= canvas_screen_pos.x && io.MousePos.x <= canvas_screen_pos.x + canvas_size.x &&
        io.MousePos.y >= canvas_screen_pos.y && io.MousePos.y <= canvas_screen_pos.y + canvas_size.y) {
        float wheel = ImGui::GetIO().MouseWheel;
        if (wheel != 0.0f) {
            // Cancel auto-pan if user starts manual zooming
            if (view_state.auto_pan_active) {
                CancelAutoPan(view_state);
            }
            view_state.user_interrupted_auto_pan = true;
            const float zoom_sensitivity = 0.1f;
            float zoom_factor = 1.0f + wheel * zoom_sensitivity;

            ImVec2 mouse_pos_screen_absolute = ImGui::GetMousePos();
            ImVec2 mouse_pos_in_canvas_content = ImVec2(mouse_pos_screen_absolute.x - canvas_screen_pos.x,
                                                       mouse_pos_screen_absolute.y - canvas_screen_pos.y);

            view_state.pan_offset.x = (view_state.pan_offset.x - mouse_pos_in_canvas_content.x) * zoom_factor + mouse_pos_in_canvas_content.x;
            view_state.pan_offset.y = (view_state.pan_offset.y - mouse_pos_in_canvas_content.y) * zoom_factor + mouse_pos_in_canvas_content.y;

            view_state.zoom_scale *= zoom_factor;
            view_state.zoom_scale = std::max(0.1f, std::min(view_state.zoom_scale, 10.0f));
        }
    }
}

void GraphEditor::HandleNodeSelection(ImDrawList* draw_list, const ImVec2& canvas_screen_pos, const ImVec2& canvas_size, GraphViewState& view_state) {
    bool clicked_on_background = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
                                 ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows) &&
                                 ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
                                 !ImGui::IsAnyItemHovered() && !ImGui::IsAnyItemActive();

    ImVec2 mouse_pos = ImGui::GetMousePos();
    bool click_is_on_canvas = (mouse_pos.x >= canvas_screen_pos.x && mouse_pos.x < canvas_screen_pos.x + canvas_size.x &&
                               mouse_pos.y >= canvas_screen_pos.y && mouse_pos.y < canvas_screen_pos.y + canvas_size.y);
    if (!click_is_on_canvas) clicked_on_background = false;

    bool item_interacted_this_frame = false;
    context_node_ = nullptr;

    for (auto& pair : nodes_) {
        auto& node_ptr = pair.second;
        if (!node_ptr) continue;
        GraphNode& node = *node_ptr;

        ImVec2 node_pos_screen_relative_to_canvas = WorldToScreen(node.position, view_state);
        ImVec2 node_size_screen = ImVec2(node.size.x * view_state.zoom_scale, node.size.y * view_state.zoom_scale);

        if (node_size_screen.x < 1.0f || node_size_screen.y < 1.0f) continue;

        ImVec2 absolute_node_button_pos = ImVec2(canvas_screen_pos.x + node_pos_screen_relative_to_canvas.x,
                                                 canvas_screen_pos.y + node_pos_screen_relative_to_canvas.y);

        ImGui::PushID(node.graph_node_id); // Use unique graph_node_id

        if (node.children.size() > 0 || node.alternative_paths.size() > 0) {
            float icon_world_size = 10.0f;
            ImVec2 icon_size_screen = ImVec2(icon_world_size * view_state.zoom_scale, icon_world_size * view_state.zoom_scale);
            if (icon_size_screen.x >= 1.0f && icon_size_screen.y >= 1.0f) {
                ImVec2 icon_local_pos(node.size.x - icon_world_size - 2.0f, node.size.y * 0.5f - icon_world_size * 0.5f);
                ImVec2 icon_world_pos = ImVec2(node.position.x + icon_local_pos.x, node.position.y + icon_local_pos.y);
                ImVec2 icon_screen_pos_relative_to_canvas = WorldToScreen(icon_world_pos, view_state);
                ImVec2 absolute_icon_button_pos = ImVec2(canvas_screen_pos.x + icon_screen_pos_relative_to_canvas.x,
                                                         canvas_screen_pos.y + icon_screen_pos_relative_to_canvas.y);

                ImGui::SetCursorScreenPos(absolute_icon_button_pos);
                char exp_btn_id[32];
                sprintf(exp_btn_id, "expcol##%ld_btn", node.graph_node_id);
                if (ImGui::Button(exp_btn_id, icon_size_screen)) {
                    node.is_expanded = !node.is_expanded;
                    item_interacted_this_frame = true;
                    // graph_layout_dirty = true; // Layout needs update - This should be handled by GraphManager
                }
                 if (ImGui::IsItemHovered() || ImGui::IsItemActive()) item_interacted_this_frame = true;
            }
        }

        ImGui::SetCursorScreenPos(absolute_node_button_pos);
        char select_btn_id[32];
        sprintf(select_btn_id, "select##%ld_btn", node.graph_node_id);
        if (ImGui::InvisibleButton(select_btn_id, node_size_screen)) {
            if (view_state.selected_node_id != node.graph_node_id) {
                if (view_state.selected_node_id != kInvalidNodeId) {
                    auto prev_selected_node = GetNode(view_state.selected_node_id);
                    if (prev_selected_node) {
                        prev_selected_node->is_selected = false;
                    }
                }
                node.is_selected = true;
                view_state.selected_node_id = node.graph_node_id;
            }
            item_interacted_this_frame = true;
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup)) {
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
                context_node_ = node_ptr;
            }
        }
        if (ImGui::IsItemHovered() || ImGui::IsItemActive()) item_interacted_this_frame = true;

        ImGui::PopID();
    }

    if (clicked_on_background && !item_interacted_this_frame) {
        if (view_state.selected_node_id != kInvalidNodeId) {
            auto current_selected_node = GetNode(view_state.selected_node_id);
            if (current_selected_node) {
                current_selected_node->is_selected = false;
            }
            view_state.selected_node_id = kInvalidNodeId;
        }
    }
}

void GraphEditor::DisplaySelectedNodeDetails(GraphViewState& view_state) {
    ImGui::Begin("Node Details");
    if (view_state.selected_node_id != kInvalidNodeId) {
        auto selected_node_ptr = GetNode(view_state.selected_node_id);
        if (selected_node_ptr) {
            ImGui::Text("Graph Node ID: %" PRIu64, selected_node_ptr->graph_node_id);
            ImGui::Text("Message ID: %" PRIu64, selected_node_ptr->message_id);
            ImGui::TextWrapped("Content: %s", selected_node_ptr->message_data.content.c_str());
            ImGui::Text("Type: %d", static_cast<int>(selected_node_ptr->message_data.type));
            if (selected_node_ptr->message_data.model_id.has_value()) {
                ImGui::Text("Model ID: %s", selected_node_ptr->message_data.model_id.value().c_str());
            }
        } else {
            ImGui::TextWrapped("Error: Selected node data not found (ID: %" PRIu64 ").", view_state.selected_node_id);
        }
    } else {
        ImGui::TextWrapped("Select a node to view its details.");
    }
    ImGui::End();
}

void GraphEditor::HandleExpandCollapse(GraphNode& node, const ImVec2& canvas_screen_pos, GraphViewState& view_state) {
    // Integrated into HandleNodeSelection
}

void GraphEditor::RenderNode(ImDrawList* draw_list, GraphNode& node, const GraphViewState& view_state) {
    ImVec2 node_pos_screen_relative_to_canvas = WorldToScreen(node.position, view_state);
    ImVec2 node_size_screen = ImVec2(node.size.x * view_state.zoom_scale, node.size.y * view_state.zoom_scale);

    if (node_size_screen.x < 1.0f || node_size_screen.y < 1.0f) return;

    ImVec2 final_draw_pos = node_pos_screen_relative_to_canvas;
    ImVec2 node_end_pos = ImVec2(final_draw_pos.x + node_size_screen.x, final_draw_pos.y + node_size_screen.y);

    // Remove bubble/box drawing - comment out AddRectFilled and AddRect calls
    // ImU32 bg_color = GetThemeNodeColor(current_theme_);
    // ImU32 border_color = GetThemeNodeBorderColor(current_theme_);
    // float border_thickness = std::max(1.0f, 1.0f * view_state.zoom_scale);

    // if (node.is_selected) {
    //     border_color = GetThemeNodeSelectedBorderColor(current_theme_);
    //     border_thickness = std::max(1.0f, 2.0f * view_state.zoom_scale);
    // }
    // float rounding = std::max(0.0f, 4.0f * view_state.zoom_scale);
    // draw_list->AddRectFilled(final_draw_pos, node_end_pos, bg_color, rounding);
    // draw_list->AddRect(final_draw_pos, node_end_pos, border_color, rounding, 0, border_thickness);
    // float text_wrap_width = node_size_screen.x - (2.0f * 10.0f * view_state.zoom_scale);
    // float max_text_height = node_size_screen.y - (2.0f * 10.0f * view_state.zoom_scale);

    // ImFont* font = ImGui::GetFont();
    // float font_size = ImGui::GetFontSize() * view_state.zoom_scale;
    // ImU32 text_color = GetThemeTextColor(current_theme_);

    // ImVec2 text_pos_screen = ImVec2(final_draw_pos.x + (10.0f * view_state.zoom_scale),
    //                                 final_draw_pos.y + (10.0f * view_state.zoom_scale));

     // Draw text content with wrapping and truncation
    // ImVec2 selectable_pos = ImVec2(final_draw_pos.x + (5.0f * view_state.zoom_scale), final_draw_pos.y + (5.0f * view_state.zoom_scale));
    // ImVec2 text_size = ImVec2(node_size_screen.x - (10.0f * view_state.zoom_scale), node_size_screen.y - (10.0f * view_state.zoom_scale));
    //ImGui::PushTextWrapPos(selectable_pos.x + text_size.x);
    //ImGui::SetCursorScreenPos(selectable_pos);
    //ImGui::TextWrapped("%s", node.label.c_str());
    //ImGui::PopTextWrapPos();

    // Render text using the helper function
    // RenderWrappedText(draw_list, font, font_size, text_pos_screen, text_color, node.label.c_str(), text_wrap_width, max_text_height, nullptr);

     // Draw expand/collapse icon if applicable at a much smaller size
    if (node.children.size() > 0 || node.alternative_paths.size() > 0) {
        float icon_world_size = 10.0f; // smaller icon size in world units
        ImVec2 icon_size_screen = ImVec2(icon_world_size * view_state.zoom_scale, icon_world_size * view_state.zoom_scale);

        if (icon_size_screen.x >= 1.0f && icon_size_screen.y >= 1.0f) {
            // Position top-right
            ImVec2 icon_local_pos(node.size.x - icon_world_size - 2.0f, node.size.y * 0.5f - icon_world_size * 0.5f);
            ImVec2 icon_world_pos = ImVec2(node.position.x + icon_local_pos.x, node.position.y + icon_local_pos.y);
            ImVec2 icon_screen_pos = WorldToScreen(icon_world_pos, view_state);

            ImU32 icon_color = GetThemeExpandCollapseIconColor(current_theme_);
            float h = icon_size_screen.y * 0.866f; // height of equilateral triangle
            if (node.is_expanded) {
                // Triangle pointing down
                draw_list->AddTriangleFilled(icon_screen_pos,
                                             ImVec2(icon_screen_pos.x + icon_size_screen.x, icon_screen_pos.y),
                                             ImVec2(icon_screen_pos.x + icon_size_screen.x * 0.5f, icon_screen_pos.y + h),
                                             icon_color);
            } else {
                // Triangle pointing right
                draw_list->AddTriangleFilled(icon_screen_pos,
                                             ImVec2(icon_screen_pos.x, icon_screen_pos.y + icon_size_screen.y),
                                             ImVec2(icon_screen_pos.x + h, icon_screen_pos.y + icon_size_screen.y * 0.5f),
                                             icon_color);
            }
        }
    }
}

void GraphEditor::RenderEdge(ImDrawList* draw_list, const GraphNode& parent_node, const GraphNode& child_node, const GraphViewState& view_state) {
    ImVec2 parent_center = ImVec2(parent_node.position.x + parent_node.size.x * 0.5f, parent_node.position.y + parent_node.size.y * 0.5f);
    ImVec2 child_center = ImVec2(child_node.position.x + child_node.size.x * 0.5f, child_node.position.y + child_node.size.y * 0.5f);
    ImVec2 p1 = WorldToScreen(parent_center, view_state);
    ImVec2 p2 = WorldToScreen(child_center, view_state);

    draw_list->AddLine(p1, p2, GetThemeEdgeColor(current_theme_), 1.0f);
}

void GraphEditor::RenderBezierEdge(ImDrawList* draw_list, const GraphNode& parent_node, const GraphNode& child_node, const GraphViewState& view_state, bool is_alternative_path) {
    ImVec2 parent_center_world = ImVec2(parent_node.position.x + parent_node.size.x * 0.5f, parent_node.position.y + parent_node.size.y);
    ImVec2 child_center_world = ImVec2(child_node.position.x + child_node.size.x * 0.5f, child_node.position.y);

    ImVec2 p1 = WorldToScreen(parent_center_world, view_state);
    ImVec2 p2 = WorldToScreen(child_center_world, view_state);

    float vertical_offset = (p2.y - p1.y) * 0.4f;
    ImVec2 cp1 = ImVec2(p1.x, p1.y + vertical_offset);
    ImVec2 cp2 = ImVec2(p2.x, p2.y - vertical_offset);

    ImU32 edge_color = GetThemeEdgeColor(current_theme_);
    float edge_thickness = 1.2f;
    if (is_alternative_path) {
        edge_color = IM_COL32(100, 100, 150, 200);
        edge_thickness = 0.8f;
    }

    draw_list->AddBezierCubic(p1, cp1, cp2, p2, edge_color, edge_thickness);
}

bool GraphEditor::IsNodeVisible(const GraphNode& node, const ImVec2& canvas_screen_pos, const ImVec2& canvas_size, const GraphViewState& view_state) const {
    ImVec2 node_pos_screen = WorldToScreen(node.position, view_state);
    ImVec2 node_size_screen = ImVec2(node.size.x * view_state.zoom_scale, node.size.y * view_state.zoom_scale);

    // Check for intersection between node's bounding box and canvas's bounding box
    return (node_pos_screen.x + node_size_screen.x > canvas_screen_pos.x &&
            node_pos_screen.x < canvas_screen_pos.x + canvas_size.x &&
            node_pos_screen.y + node_size_screen.y > canvas_screen_pos.y &&
            node_pos_screen.y < canvas_screen_pos.y + canvas_size.y);
}

void GraphEditor::RenderNodeRecursive(ImDrawList* draw_list, GraphNode& node, const ImVec2& canvas_screen_pos, const ImVec2& canvas_size, GraphViewState& view_state) {
    if (&node == nullptr || !IsNodeVisible(node, canvas_screen_pos, canvas_size, view_state)) {
        return;
    }

    RenderNode(draw_list, node, view_state);

    if (node.is_expanded) {
        if (node.parent.lock()) { // Draw primary parent-child relationship
            // RenderEdge(draw_list, *node.parent.lock(), node); // Re-enable if needed
        }
        for (const auto& child_ptr : node.children) {
            if (auto shared_child = child_ptr.lock()) {
                RenderBezierEdge(draw_list, node, *shared_child, view_state);
                RenderNodeRecursive(draw_list, *shared_child, canvas_screen_pos, canvas_size, view_state);
            }
        }
        for (const auto& alt_path_ptr : node.alternative_paths) {
            if (auto shared_alt = alt_path_ptr.lock()) {
                RenderBezierEdge(draw_list, node, *shared_alt, view_state, true);
                RenderNodeRecursive(draw_list, *shared_alt, canvas_screen_pos, canvas_size, view_state);
            }
        }
    }
}

void GraphEditor::Render(ImDrawList* draw_list, const ImVec2& canvas_pos, const ImVec2& canvas_size, GraphViewState& view_state) {
    draw_list->PushClipRect(canvas_pos, ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y), true);

    if (m_graph_manager) {
        auto root_nodes = m_graph_manager->GetRootNodes();
        auto all_nodes = m_graph_manager->GetAllNodes();

        // Clear existing nodes and repopulate from the manager to stay in sync
        ClearNodes();
        for (const auto& pair : all_nodes) {
            AddNode(pair.second);
        }

        // Culling logic can be done here. Pass canvas_pos and canvas_size.
        for (auto& root_node : root_nodes) {
            if (root_node) {
                 RenderNodeRecursive(draw_list, *root_node, canvas_pos, canvas_size, view_state);
            }
        }
    }

    // Interaction handlers
    HandlePanning(canvas_pos, canvas_size, view_state);
    HandleZooming(canvas_pos, canvas_size, view_state);
    HandleNodeSelection(draw_list, canvas_pos, canvas_size, view_state);

    // Render popups
    RenderPopups(draw_list, canvas_pos, view_state);

    draw_list->PopClipRect();
}

void GraphEditor::RenderPopups(ImDrawList* draw_list, const ImVec2& canvas_pos, GraphViewState& view_state) {
    if (context_node_ && ImGui::IsMouseClicked(ImGuiMouseButton_Right) && ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows)) {
        ImGui::OpenPopup("NodeContextMenu");
    }

    if (ImGui::BeginPopup("NodeContextMenu")) {
        RenderNodeContextMenu();
        ImGui::EndPopup();
    }
    RenderNewMessageModal(draw_list, canvas_pos); // Call the new modal rendering function
}

void GraphEditor::RenderNodeContextMenu() {
    if (!context_node_) return;

    if (ImGui::MenuItem("Reply to this message")) {
        reply_parent_node_ = context_node_;
        ImGui::OpenPopup("New Message Modal");
    }

    if (ImGui::MenuItem("Delete Node")) {
        // Implement node deletion logic
    }
    // Add more context menu items here
}

void GraphEditor::RenderNewMessageModal(ImDrawList* draw_list, const ImVec2& canvas_pos) {
    if (ImGui::BeginPopupModal("New Message Modal", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Enter new message content:");
        ImGui::InputTextMultiline("##new_message_input", newMessageBuffer_, sizeof(newMessageBuffer_), ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 8));

        if (ImGui::Button("OK", ImVec2(120, 0))) {
            if (reply_parent_node_ && m_graph_manager) {
                m_graph_manager->CreateNode(reply_parent_node_->graph_node_id, MessageType::USER_REPLY, newMessageBuffer_);
            }
            // Reset for next use
            memset(newMessageBuffer_, 0, sizeof(newMessageBuffer_));
            reply_parent_node_ = nullptr;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SetItemDefaultFocus();
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            // Reset for next use
            memset(newMessageBuffer_, 0, sizeof(newMessageBuffer_));
            reply_parent_node_ = nullptr;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}
void GraphEditor::CancelAutoPan(GraphViewState& view_state) {
    view_state.auto_pan_active = false;
    // No need to reset pan_velocity_, it will be recalculated on next StartAutoPan
}

void GraphEditor::StartAutoPanToNode(const std::shared_ptr<GraphNode>& target_node, const ImVec2& canvas_size) {
    if (!target_node) return;
    
    // We get a snapshot, but we need to modify it and set it back.
    GraphViewState view_state = m_graph_manager->getGraphViewStateSnapshot();

    ImVec2 target_world_pos = ImVec2(target_node->position.x + target_node->size.x / 2,
                                     target_node->position.y + target_node->size.y / 2);

    StartAutoPanToPosition(target_world_pos, view_state.zoom_scale, canvas_size, view_state);
    
    m_graph_manager->setGraphViewState(view_state);
}

void GraphEditor::StartAutoPanToPosition(const ImVec2& target_world_pos, float target_zoom, const ImVec2& canvas_size, GraphViewState& view_state) {
    view_state.auto_pan_active = true;
    view_state.auto_pan_timer = 0.0f;
    
    // Calculate the target pan offset to center the target position on the canvas
    view_state.auto_pan_target_offset.x = canvas_size.x / 2 - (target_world_pos.x * target_zoom);
    view_state.auto_pan_target_offset.y = canvas_size.y / 2 - (target_world_pos.y * target_zoom);
    
    view_state.auto_pan_start_offset = view_state.pan_offset;
    view_state.auto_pan_target_zoom = target_zoom;
    view_state.auto_pan_start_zoom = view_state.zoom_scale;
}


void GraphEditor::UpdateAutoPan(float delta_time, GraphViewState& view_state) {
    if (!view_state.auto_pan_active) return;

    view_state.auto_pan_timer += delta_time;
    float t = std::min(view_state.auto_pan_timer / view_state.auto_pan_duration, 1.0f);
    float eased_t = CameraEasing::EaseInOutCubic(t);

    // Interpolate pan offset and zoom
    view_state.pan_offset = CameraEasing::LerpVec2(view_state.auto_pan_start_offset, view_state.auto_pan_target_offset, eased_t);
    view_state.zoom_scale = CameraEasing::Lerp(view_state.auto_pan_start_zoom, view_state.auto_pan_target_zoom, eased_t);

    if (t >= 1.0f) {
        view_state.auto_pan_active = false;
    }
}


// --- Main Rendering Function ---
void RenderGraphView(GraphManager& graph_manager, ThemeType current_theme) {
    static GraphEditor graph_editor(&graph_manager);
    graph_editor.SetCurrentTheme(current_theme);

    ImGui::Begin("Graph View");

    ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
    ImVec2 canvas_size = ImGui::GetContentRegionAvail();
    if (canvas_size.x < 50.0f) canvas_size.x = 50.0f;
    if (canvas_size.y < 50.0f) canvas_size.y = 50.0f;

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->AddRectFilled(canvas_pos, ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y), graph_editor.GetThemeBackgroundColor(current_theme));
    draw_list->AddRect(canvas_pos, ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y), IM_COL32(100, 100, 100, 255));
    
    GraphViewState view_state = graph_manager.getGraphViewStateSnapshot();

    graph_editor.Render(draw_list, canvas_pos, canvas_size, view_state);
    
    // Update auto-pan if active
    graph_editor.UpdateAutoPan(ImGui::GetIO().DeltaTime, view_state);
    
    // After all modifications, update the state in the manager
    graph_manager.setGraphViewState(view_state);

    ImGui::End();

    // Display node details in a separate window
    graph_editor.DisplaySelectedNodeDetails(view_state);
}