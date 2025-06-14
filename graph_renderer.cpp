#include "graph_renderer.h"
#include "extern/imgui/imgui_internal.h" // For ImGui::CalcTextSize, ImGui::PushClipRect, ImTextCharToUtf8
#include "id_types.h"                    // NodeIdType definition
#include <string>     // For std::string, std::to_string
#include <algorithm>  // For std::min, std::max
#include <limits>     // Required for std::numeric_limits
#include <cstdio>     // For sprintf
#include <cstring>    // For strlen
#include <cmath>      // For sqrtf, FLT_MAX, powf, fabsf
#include <chrono>     // For std::chrono::system_clock, std::chrono::steady_clock
#include <functional> // For std::function
// Initialize the static member for the new message buffer
char GraphEditor::newMessageBuffer_[1024 * 16] = {};
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


GraphEditor::GraphEditor() {
    // view_state_ is already initialized by its default constructor
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
        nodes_[node->message_id] = node; // Assuming message_id is unique for now, or use graph_node_id
    }
}

std::shared_ptr<GraphNode> GraphEditor::GetNode(NodeIdType node_id) {
    auto it = nodes_.find(node_id);
    if (it != nodes_.end()) {
        return it->second;
    }
    return nullptr;
}

void GraphEditor::ClearNodes() {
    nodes_.clear();
}

ImVec2 GraphEditor::WorldToScreen(const ImVec2& world_pos) const {
    return ImVec2((world_pos.x * view_state_.zoom_scale) + view_state_.pan_offset.x,
                  (world_pos.y * view_state_.zoom_scale) + view_state_.pan_offset.y);
}

ImVec2 GraphEditor::ScreenToWorld(const ImVec2& screen_pos_absolute, const ImVec2& canvas_screen_pos_absolute) const {
    ImVec2 mouse_relative_to_canvas_origin = ImVec2(screen_pos_absolute.x - canvas_screen_pos_absolute.x, 
                                                   screen_pos_absolute.y - canvas_screen_pos_absolute.y);
    
    if (view_state_.zoom_scale == 0.0f) return ImVec2(0,0); // Avoid division by zero
    return ImVec2((mouse_relative_to_canvas_origin.x - view_state_.pan_offset.x) / view_state_.zoom_scale,
                  (mouse_relative_to_canvas_origin.y - view_state_.pan_offset.y) / view_state_.zoom_scale);
}

void GraphEditor::HandlePanning(const ImVec2& canvas_screen_pos, const ImVec2& canvas_size) {
    ImGuiIO& io = ImGui::GetIO();
    if (io.MousePos.x >= canvas_screen_pos.x && io.MousePos.x <= canvas_screen_pos.x + canvas_size.x &&
        io.MousePos.y >= canvas_screen_pos.y && io.MousePos.y <= canvas_screen_pos.y + canvas_size.y) {
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
            // Cancel auto-pan if user starts manual panning
            if (view_state_.auto_pan_active) {
                CancelAutoPan();
            }
            
            view_state_.pan_offset.x += ImGui::GetIO().MouseDelta.x;
            view_state_.pan_offset.y += ImGui::GetIO().MouseDelta.y;
        }
    }
}

void GraphEditor::HandleZooming(const ImVec2& canvas_screen_pos, const ImVec2& canvas_size) {
    ImGuiIO& io = ImGui::GetIO();
     if (io.MousePos.x >= canvas_screen_pos.x && io.MousePos.x <= canvas_screen_pos.x + canvas_size.x &&
        io.MousePos.y >= canvas_screen_pos.y && io.MousePos.y <= canvas_screen_pos.y + canvas_size.y) {
        float wheel = ImGui::GetIO().MouseWheel;
        if (wheel != 0.0f) {
            // Cancel auto-pan if user starts manual zooming
            if (view_state_.auto_pan_active) {
                CancelAutoPan();
            }
            
            const float zoom_sensitivity = 0.1f;
            float zoom_factor = 1.0f + wheel * zoom_sensitivity;

            ImVec2 mouse_pos_screen_absolute = ImGui::GetMousePos();
            ImVec2 mouse_pos_in_canvas_content = ImVec2(mouse_pos_screen_absolute.x - canvas_screen_pos.x,
                                                       mouse_pos_screen_absolute.y - canvas_screen_pos.y);

            view_state_.pan_offset.x = (view_state_.pan_offset.x - mouse_pos_in_canvas_content.x) * zoom_factor + mouse_pos_in_canvas_content.x;
            view_state_.pan_offset.y = (view_state_.pan_offset.y - mouse_pos_in_canvas_content.y) * zoom_factor + mouse_pos_in_canvas_content.y;
            
            view_state_.zoom_scale *= zoom_factor;
            view_state_.zoom_scale = std::max(0.1f, std::min(view_state_.zoom_scale, 10.0f));
        }
    }
}

void GraphEditor::HandleNodeSelection(ImDrawList* draw_list, const ImVec2& canvas_screen_pos, const ImVec2& canvas_size) {
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

        ImVec2 node_pos_screen_relative_to_canvas = WorldToScreen(node.position);
        ImVec2 node_size_screen = ImVec2(node.size.x * view_state_.zoom_scale, node.size.y * view_state_.zoom_scale);

        if (node_size_screen.x < 1.0f || node_size_screen.y < 1.0f) continue; 

        ImVec2 absolute_node_button_pos = ImVec2(canvas_screen_pos.x + node_pos_screen_relative_to_canvas.x, 
                                                 canvas_screen_pos.y + node_pos_screen_relative_to_canvas.y);

        ImGui::PushID(node.graph_node_id); // Use unique graph_node_id

        if (node.children.size() > 0 || node.alternative_paths.size() > 0) {
            float icon_world_size = 10.0f; 
            ImVec2 icon_size_screen = ImVec2(icon_world_size * view_state_.zoom_scale, icon_world_size * view_state_.zoom_scale);
            if (icon_size_screen.x >= 1.0f && icon_size_screen.y >= 1.0f) {
                ImVec2 icon_local_pos(node.size.x - icon_world_size - 2.0f, node.size.y * 0.5f - icon_world_size * 0.5f); 
                ImVec2 icon_world_pos = ImVec2(node.position.x + icon_local_pos.x, node.position.y + icon_local_pos.y);
                ImVec2 icon_screen_pos_relative_to_canvas = WorldToScreen(icon_world_pos);
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
            if (view_state_.selected_node_id != node.graph_node_id) {
                if (view_state_.selected_node_id != kInvalidNodeId) {
                    auto prev_selected_node = GetNode(view_state_.selected_node_id);
                    if (prev_selected_node) {
                        prev_selected_node->is_selected = false;
                    }
                }
                node.is_selected = true;
                view_state_.selected_node_id = node.graph_node_id;
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
        if (view_state_.selected_node_id != kInvalidNodeId) {
            auto current_selected_node = GetNode(view_state_.selected_node_id);
            if (current_selected_node) {
                current_selected_node->is_selected = false;
            }
            view_state_.selected_node_id = kInvalidNodeId;
        }
    }
}

void GraphEditor::DisplaySelectedNodeDetails() {
    ImGui::Begin("Node Details");
    if (view_state_.selected_node_id != kInvalidNodeId) {
        auto selected_node_ptr = GetNode(view_state_.selected_node_id);
        if (selected_node_ptr) {
            ImGui::Text("Graph Node ID: %ld", selected_node_ptr->graph_node_id);
            ImGui::Text("Message ID: %ld", selected_node_ptr->message_id);
            ImGui::TextWrapped("Content: %s", selected_node_ptr->message_data.content.c_str());
            ImGui::Text("Type: %d", static_cast<int>(selected_node_ptr->message_data.type));
            if (selected_node_ptr->message_data.model_id.has_value()) {
                ImGui::Text("Model ID: %s", selected_node_ptr->message_data.model_id.value().c_str());
            }
        } else {
            ImGui::TextWrapped("Error: Selected node data not found (ID: %ld).", view_state_.selected_node_id);
        }
    } else {
        ImGui::TextWrapped("Select a node to view its details.");
    }
    ImGui::End(); 
}

void GraphEditor::HandleExpandCollapse(GraphNode& node, const ImVec2& canvas_screen_pos) {
    // Integrated into HandleNodeSelection
}

void GraphEditor::RenderNode(ImDrawList* draw_list, GraphNode& node) {
    ImVec2 node_pos_screen_relative_to_canvas = WorldToScreen(node.position);
    ImVec2 node_size_screen = ImVec2(node.size.x * view_state_.zoom_scale, node.size.y * view_state_.zoom_scale);
    
    if (node_size_screen.x < 1.0f || node_size_screen.y < 1.0f) return;

    ImVec2 final_draw_pos = node_pos_screen_relative_to_canvas;
    ImVec2 node_end_pos = ImVec2(final_draw_pos.x + node_size_screen.x, final_draw_pos.y + node_size_screen.y);

    // Remove bubble/box drawing - comment out AddRectFilled and AddRect calls
    // ImU32 bg_color = GetThemeNodeColor(current_theme_);
    // ImU32 border_color = GetThemeNodeBorderColor(current_theme_);
    // float border_thickness = std::max(1.0f, 1.0f * view_state_.zoom_scale);

    // if (node.is_selected) {
    //     border_color = GetThemeNodeSelectedBorderColor(current_theme_);
    //     border_thickness = std::max(1.0f, 2.0f * view_state_.zoom_scale);
    // }
    // float rounding = std::max(0.0f, 4.0f * view_state_.zoom_scale);
    // draw_list->AddRectFilled(final_draw_pos, node_end_pos, bg_color, rounding);
    // draw_list->AddRect(final_draw_pos, node_end_pos, border_color, rounding, 0, border_thickness);

    // Use minimal padding for text-only nodes (5-10px instead of 20px)
    float padding = std::max(5.0f, 8.0f * view_state_.zoom_scale);
    ImVec2 text_pos = ImVec2(final_draw_pos.x, final_draw_pos.y); // Position text at node origin
    ImVec2 content_area_size = ImVec2(node_size_screen.x, node_size_screen.y); // Use full node size for text
    
    bool has_children_or_alternatives = !node.children.empty() || !node.alternative_paths.empty();
    float icon_area_width = 0.0f;

    if (has_children_or_alternatives) {
        float icon_world_size = 10.0f; 
        icon_area_width = (icon_world_size + 2.0f) * view_state_.zoom_scale; 
        content_area_size.x -= icon_area_width;
    }
    content_area_size.x = std::max(0.0f, content_area_size.x);
    content_area_size.y = std::max(0.0f, content_area_size.y);
    
    if (content_area_size.x > 1.0f && content_area_size.y > 1.0f) {
        // Update clipping rectangle to match new smaller text bounds
        ImVec4 clip_rect_vec4(final_draw_pos.x, final_draw_pos.y,
                              final_draw_pos.x + content_area_size.x,
                              final_draw_pos.y + content_area_size.y);
        draw_list->PushClipRect(ImVec2(clip_rect_vec4.x, clip_rect_vec4.y), ImVec2(clip_rect_vec4.z, clip_rect_vec4.w), true);

        // Use the full content from label (which now contains the formatted message)
        const char* text_to_display = node.label.empty() ? "[Empty Node]" : node.label.c_str();
        
        ImFont* current_font = ImGui::GetFont();
        float base_font_size = current_font->FontSize;
        float scaled_font_size = base_font_size * view_state_.zoom_scale;
        scaled_font_size = std::max(6.0f, std::min(scaled_font_size, 72.0f));
        
        // Add selection visual feedback for text-only nodes
        ImU32 text_color = GetThemeTextColor(current_theme_);
        if (node.is_selected) {
            // Use selection border color for text when node is selected
            text_color = GetThemeNodeSelectedBorderColor(current_theme_);
        }
        
        // Use proper text wrapping instead of truncation for full content display
        // Ensure minimum content area size for proper text rendering
        if (content_area_size.x < 50.0f) content_area_size.x = 50.0f;
        if (content_area_size.y < 20.0f) content_area_size.y = 20.0f;
        
        // Force immediate text rendering to fix auto-refresh issue for new nodes
        // Check if this node needs content refresh (new nodes or updated content)
        if (node.content_needs_refresh) {
            // Force immediate rendering by ensuring text is drawn with fresh parameters
            node.content_needs_refresh = false; // Clear the flag after rendering
        }
        
        // This ensures new message content is displayed immediately without manual refresh
        RenderWrappedText(draw_list, current_font, scaled_font_size, text_pos, text_color,
                         text_to_display, content_area_size.x, content_area_size.y, &clip_rect_vec4);
        draw_list->PopClipRect();
    }

    if (has_children_or_alternatives) {
        float icon_world_size = 10.0f;
        ImVec2 icon_size_screen = ImVec2(icon_world_size * view_state_.zoom_scale, icon_world_size * view_state_.zoom_scale);
        if (icon_size_screen.x >= 1.0f && icon_size_screen.y >= 1.0f) { 
            ImVec2 icon_local_pos(node.size.x - icon_world_size - 2.0f, node.size.y * 0.5f - icon_world_size * 0.5f);
            ImVec2 icon_world_pos = ImVec2(node.position.x + icon_local_pos.x, node.position.y + icon_local_pos.y);
            ImVec2 icon_screen_pos_relative_to_canvas = WorldToScreen(icon_world_pos);
            
            ImVec2 icon_visual_top_left = icon_screen_pos_relative_to_canvas;
            ImVec2 icon_visual_center = ImVec2(icon_visual_top_left.x + icon_size_screen.x * 0.5f,
                                               icon_visual_top_left.y + icon_size_screen.y * 0.5f);

            ImU32 indicator_color = GetThemeExpandCollapseIconColor(current_theme_);
            float s = icon_size_screen.x * 0.4f;

            if (node.is_expanded) {
                 draw_list->AddTriangleFilled(
                    ImVec2(icon_visual_center.x - s, icon_visual_center.y - s * 0.5f),
                    ImVec2(icon_visual_center.x + s, icon_visual_center.y - s * 0.5f),
                    ImVec2(icon_visual_center.x, icon_visual_center.y + s * 0.5f),
                    indicator_color);
            } else {
                draw_list->AddTriangleFilled(
                    ImVec2(icon_visual_center.x - s * 0.5f, icon_visual_center.y - s),
                    ImVec2(icon_visual_center.x - s * 0.5f, icon_visual_center.y + s),
                    ImVec2(icon_visual_center.x + s * 0.5f, icon_visual_center.y),
                    indicator_color);
            }
        }
    }
}

void GraphEditor::RenderEdge(ImDrawList* draw_list, const GraphNode& parent_node, const GraphNode& child_node) {
    // Use the new Bezier edge rendering function for standard parent-child relationships
    RenderBezierEdge(draw_list, parent_node, child_node, false);
}

// Helper function to calculate optimal control points for Bezier curves
static ImVec2 CalculateOptimalControlPoint(const ImVec2& start, const ImVec2& end, float control_offset, bool is_start_point, bool is_alternative_path) {
    ImVec2 direction = ImVec2(end.x - start.x, end.y - start.y);
    
    if (is_alternative_path) {
        // Alternative paths use more pronounced horizontal curves
        float horizontal_offset = control_offset * 0.7f;
        if (is_start_point) {
            return direction.x > 0 ?
                ImVec2(start.x + horizontal_offset, start.y + control_offset * 0.5f) :
                ImVec2(start.x - horizontal_offset, start.y + control_offset * 0.5f);
        } else {
            return direction.x > 0 ?
                ImVec2(end.x + horizontal_offset, end.y - control_offset * 0.5f) :
                ImVec2(end.x - horizontal_offset, end.y - control_offset * 0.5f);
        }
    } else {
        // Standard parent-child relationships use smooth downward curves
        return is_start_point ?
            ImVec2(start.x, start.y + control_offset) :
            ImVec2(end.x, end.y - control_offset);
    }
}

void GraphEditor::RenderBezierEdge(ImDrawList* draw_list, const GraphNode& parent_node, const GraphNode& child_node, bool is_alternative_path) {
    // Update edge connection points to connect to text bounds rather than large rectangles
    // For text-only nodes, connect from bottom center of parent to top center of child
    ImVec2 start_world = ImVec2(parent_node.position.x + parent_node.size.x / 2.0f, parent_node.position.y + parent_node.size.y);
    ImVec2 end_world = ImVec2(child_node.position.x + child_node.size.x / 2.0f, child_node.position.y);

    ImVec2 start_screen = WorldToScreen(start_world);
    ImVec2 end_screen = WorldToScreen(end_world);

    // Use theme-aware edge color with different styles for alternative paths
    ImU32 edge_color = GetThemeEdgeColor(current_theme_);
    float line_thickness = std::max(1.0f, 1.5f * view_state_.zoom_scale);
    
    // Different visual styles for alternative paths
    if (is_alternative_path) {
        // Make alternative paths slightly more transparent and thinner
        ImU32 base_color = edge_color;
        // Correctly halve the alpha channel without corrupting other color channels.
        uint32_t a = (base_color >> 24) & 0xFF;
        a >>= 1; // Halve alpha value.
        edge_color = (base_color & 0x00FFFFFFu) | (a << 24);
        line_thickness *= 0.8f; // Slightly thinner
    }
    
    // Calculate Bezier curve control points for smooth, natural-looking curves
    ImVec2 direction = ImVec2(end_screen.x - start_screen.x, end_screen.y - start_screen.y);
    float distance = sqrtf(direction.x * direction.x + direction.y * direction.y);
    
    // Control point offset based on distance and zoom for optimal curve shape
    float control_offset = std::min(distance * 0.4f, 80.0f * view_state_.zoom_scale);
    control_offset = std::max(control_offset, 20.0f * view_state_.zoom_scale);
    
    // Handle edge cases for very close nodes
    if (distance < 10.0f * view_state_.zoom_scale) {
        // For very close nodes, use a minimal curve to avoid visual artifacts
        control_offset = std::min(control_offset, distance * 0.2f);
    }
    
    // Calculate optimal control points using helper function
    ImVec2 control1 = CalculateOptimalControlPoint(start_screen, end_screen, control_offset, true, is_alternative_path);
    ImVec2 control2 = CalculateOptimalControlPoint(start_screen, end_screen, control_offset, false, is_alternative_path);
    
    // Render smooth Bezier curve with adaptive tessellation for performance
    // Use fewer segments for distant/small curves, more for close/large curves
    int num_segments = 0; // 0 = auto-tessellation by ImGui
    if (distance > 200.0f * view_state_.zoom_scale) {
        // For long curves, limit tessellation to maintain performance
        num_segments = std::max(8, (int)(distance / (50.0f * view_state_.zoom_scale)));
        num_segments = std::min(num_segments, 32); // Cap at 32 segments
    }
    
    draw_list->AddBezierCubic(start_screen, control1, control2, end_screen, edge_color, line_thickness, num_segments);
    
    // Calculate arrow direction at the end of the curve for proper arrow orientation
    float arrow_base_size = 8.0f;
    float arrow_size = std::max(2.0f, arrow_base_size * view_state_.zoom_scale);
    
    // Scale arrow size for alternative paths
    if (is_alternative_path) {
        arrow_size *= 0.8f;
    }
    
    // Calculate tangent direction at the end point for proper arrow orientation
    ImVec2 tangent_dir = ImVec2(end_screen.x - control2.x, end_screen.y - control2.y);
    float tangent_len = sqrtf(tangent_dir.x * tangent_dir.x + tangent_dir.y * tangent_dir.y);
    
    if (tangent_len > 0.1f && arrow_size > 1.0f) {
        tangent_dir.x /= tangent_len;
        tangent_dir.y /= tangent_len;
        
        // Create arrow head pointing in the direction of the curve tangent
        ImVec2 p1 = ImVec2(end_screen.x - tangent_dir.x * arrow_size - tangent_dir.y * arrow_size / 2.0f,
                           end_screen.y - tangent_dir.y * arrow_size + tangent_dir.x * arrow_size / 2.0f);
        ImVec2 p2 = ImVec2(end_screen.x - tangent_dir.x * arrow_size + tangent_dir.y * arrow_size / 2.0f,
                           end_screen.y - tangent_dir.y * arrow_size - tangent_dir.x * arrow_size / 2.0f);
        draw_list->AddTriangleFilled(end_screen, p1, p2, edge_color);
    }
}

// Culling helper function
bool GraphEditor::IsNodeVisible(const GraphNode& node, const ImVec2& canvas_screen_pos, const ImVec2& canvas_size) const {
    ImVec2 canvas_rect_min = canvas_screen_pos;
    ImVec2 canvas_rect_max = ImVec2(canvas_screen_pos.x + canvas_size.x, canvas_screen_pos.y + canvas_size.y);

    ImVec2 node_pos_screen_relative_to_canvas = WorldToScreen(node.position);
    ImVec2 node_screen_min_abs = ImVec2(canvas_screen_pos.x + node_pos_screen_relative_to_canvas.x, 
                                       canvas_screen_pos.y + node_pos_screen_relative_to_canvas.y);
    ImVec2 node_size_screen = ImVec2(node.size.x * view_state_.zoom_scale, node.size.y * view_state_.zoom_scale);
    ImVec2 node_screen_max_abs = ImVec2(node_screen_min_abs.x + node_size_screen.x, node_screen_min_abs.y + node_size_screen.y);
    
    bool x_overlap = (node_screen_min_abs.x < canvas_rect_max.x && node_screen_max_abs.x > canvas_rect_min.x);
    bool y_overlap = (node_screen_min_abs.y < canvas_rect_max.y && node_screen_max_abs.y > canvas_rect_min.y);

    return x_overlap && y_overlap;
}

void GraphEditor::RenderNodeRecursive(ImDrawList* draw_list, GraphNode& node, const ImVec2& canvas_screen_pos, const ImVec2& canvas_size) {
    bool node_is_currently_visible = IsNodeVisible(node, canvas_screen_pos, canvas_size);

    if (node_is_currently_visible) {
        RenderNode(draw_list, node); 
    }

    if (node.is_expanded) {
        for (const auto& weak_child : node.children) {
            if (auto shared_child = weak_child.lock()) {
                GraphNode* child_ptr = shared_child.get();
                bool child_is_currently_visible = IsNodeVisible(*child_ptr, canvas_screen_pos, canvas_size);
                if (node_is_currently_visible && child_is_currently_visible) {
                    RenderEdge(draw_list, node, *child_ptr);
                }
                RenderNodeRecursive(draw_list, *child_ptr, canvas_screen_pos, canvas_size);
            }
        }
        for (const auto& weak_alt : node.alternative_paths) {
            if (auto shared_alt = weak_alt.lock()) {
                GraphNode* alt_ptr = shared_alt.get();
                bool alt_is_currently_visible = IsNodeVisible(*alt_ptr, canvas_screen_pos, canvas_size);
                if (node_is_currently_visible && alt_is_currently_visible) {
                    RenderBezierEdge(draw_list, node, *alt_ptr, true); // Use alternative path styling
                }
                RenderNodeRecursive(draw_list, *alt_ptr, canvas_screen_pos, canvas_size);
            }
        }
    }
}

void GraphEditor::Render(ImDrawList* draw_list, const ImVec2& canvas_screen_pos, const ImVec2& canvas_size) {
    // Update auto-pan animation
    double current_time = ImGui::GetTime();
    float delta_time = (view_state_.last_time > 0.0) ? (float)(current_time - view_state_.last_time) : (1.0f / 60.0f);
    view_state_.last_time = current_time;
    
    UpdateAutoPan(delta_time);
    
    HandlePanning(canvas_screen_pos, canvas_size);
    HandleZooming(canvas_screen_pos, canvas_size);
    HandleNodeSelection(draw_list, canvas_screen_pos, canvas_size); // Also handles expand/collapse button logic
    RenderPopups(draw_list, canvas_screen_pos);

    // If layout is dirty, recalculate (example call, actual parameters might differ)
    // if (graph_layout_dirty && !nodes_.empty()) {
    //     std::map<int, float> level_x_offset;
    //     ImVec2 layout_start_pos(20,20); // Example
    //     for (auto& pair : nodes_) {
    //         if (pair.second && pair.second->parent == nullptr) { // Assuming roots
    //             CalculateNodePositionsRecursive(pair.second, layout_start_pos, 50.0f, 100.0f, 0, level_x_offset, layout_start_pos);
    //         }
    //     }
    //     graph_layout_dirty = false;
    // }


    std::vector<std::shared_ptr<GraphNode>> root_nodes_to_render;
    if (!nodes_.empty()) {
        for (auto& pair : nodes_) {
            if (pair.second && !pair.second->parent.lock()) {
                root_nodes_to_render.push_back(pair.second);
            }
        }
    }

    for (const auto& root_node_ptr : root_nodes_to_render) {
        if (root_node_ptr) {
            RenderNodeRecursive(draw_list, *root_node_ptr, canvas_screen_pos, canvas_size);
        }
    }
}

void GraphEditor::RenderPopups(ImDrawList* draw_list, const ImVec2& canvas_pos) {
    if (context_node_) {
        ImGui::OpenPopup("NodeContextMenu");
    }
    RenderNodeContextMenu();
    RenderNewMessageModal(draw_list, canvas_pos);
}

void GraphEditor::RenderNodeContextMenu() {
    if (ImGui::BeginPopup("NodeContextMenu")) {
        if (!context_node_) {
            ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
            return;
        }

        ImGui::Text("Node ID: %ld", context_node_->graph_node_id);
        ImGui::Separator();

        if (ImGui::MenuItem("Reply from here")) {
            reply_parent_node_ = context_node_;
            memset(newMessageBuffer_, 0, sizeof(newMessageBuffer_));
            ImGui::OpenPopup("NewMessageModal");
            ImGui::CloseCurrentPopup();
        }
        // Add other context menu items here...
        ImGui::EndPopup();
    }
}

void GraphEditor::RenderNewMessageModal(ImDrawList* draw_list, const ImVec2& canvas_pos) {
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal("NewMessageModal", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        if (!reply_parent_node_) {
            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Error: No parent node specified for reply.");
            if (ImGui::Button("Close")) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
            return;
        }

        ImGui::Text("Replying to Node ID: %ld", reply_parent_node_->graph_node_id);
        if (reply_parent_node_->message_data.content.length() > 50) {
            ImGui::TextWrapped("Parent: %.50s...", reply_parent_node_->message_data.content.c_str());
        } else {
            ImGui::TextWrapped("Parent: %s", reply_parent_node_->message_data.content.c_str());
        }
        ImGui::Separator();

        ImGui::Text("New Message:");
        ImGui::InputTextMultiline("##NewMessageInput", newMessageBuffer_, sizeof(newMessageBuffer_),
                                  ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 8), ImGuiInputTextFlags_AllowTabInput);

        if (ImGui::Button("Submit", ImVec2(120, 0))) {
            std::string new_message_content = newMessageBuffer_;
            if (!new_message_content.empty()) {
                HistoryMessage new_hist_msg;
                new_hist_msg.message_id = static_cast<NodeIdType>(-1); // Placeholder until assigned
                new_hist_msg.type = MessageType::USER_REPLY;
                new_hist_msg.content = new_message_content;
                new_hist_msg.timestamp = std::chrono::time_point_cast<std::chrono::milliseconds>(
                                              std::chrono::system_clock::now());
                new_hist_msg.parent_id = reply_parent_node_->message_id; // Original parent message id

                // The GraphManager should handle creating the GraphNode and assigning a unique graph_node_id
                // For now, let's simulate some parts if GraphManager isn't fully integrated here.
                // This part would typically be: graph_manager_instance.HandleNewHistoryMessage(new_hist_msg, reply_parent_node_->graph_node_id);
                
                // Example: Manually create and add if not using GraphManager directly here
                int new_graph_node_id = GetNextUniqueID(nodes_); // Generate a new graph node ID
                auto new_graph_node = std::make_shared<GraphNode>(new_graph_node_id, new_hist_msg);
                new_graph_node->parent = reply_parent_node_->weak_from_this();
                new_graph_node->depth = reply_parent_node_->depth + 1;
                // Position and size would be set by layout algorithm
                new_graph_node->size = ImVec2(150, 80); // Default size
                
                reply_parent_node_->add_child(new_graph_node);
                nodes_[new_graph_node_id] = new_graph_node; // Add to editor's map
                // graph_layout_dirty = true; // Mark layout as dirty - This should be handled by GraphManager

                memset(newMessageBuffer_, 0, sizeof(newMessageBuffer_));
                ImGui::CloseCurrentPopup();
                reply_parent_node_ = nullptr;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            memset(newMessageBuffer_, 0, sizeof(newMessageBuffer_));
            ImGui::CloseCurrentPopup();
            reply_parent_node_ = nullptr;
        }
        ImGui::EndPopup();
    }
}

// Camera auto-pan functionality implementation
void GraphEditor::StartAutoPanToNode(const std::shared_ptr<GraphNode>& target_node, const ImVec2& canvas_size) {
    if (!target_node) return;
    
    // Calculate optimal camera position to center the target node
    ImVec2 target_world_center = ImVec2(
        target_node->position.x + target_node->size.x * 0.5f,
        target_node->position.y + target_node->size.y * 0.5f
    );
    
    // Calculate target pan offset to center the node in the canvas
    ImVec2 canvas_center = ImVec2(canvas_size.x * 0.5f, canvas_size.y * 0.5f);
    ImVec2 target_pan_offset = ImVec2(
        canvas_center.x - target_world_center.x * view_state_.zoom_scale,
        canvas_center.y - target_world_center.y * view_state_.zoom_scale
    );
    
    // Ensure the target zoom keeps the node visible and readable
    float target_zoom = view_state_.zoom_scale;
    float min_node_screen_size = 150.0f; // Minimum desired node size on screen
    float max_node_screen_size = 400.0f; // Maximum desired node size on screen
    
    float current_node_screen_width = target_node->size.x * view_state_.zoom_scale;
    if (current_node_screen_width < min_node_screen_size) {
        target_zoom = min_node_screen_size / target_node->size.x;
    } else if (current_node_screen_width > max_node_screen_size) {
        target_zoom = max_node_screen_size / target_node->size.x;
    }
    
    // Clamp zoom to reasonable bounds
    target_zoom = std::max(0.1f, std::min(target_zoom, 3.0f));
    
    // Recalculate target pan offset with the new zoom
    target_pan_offset = ImVec2(
        canvas_center.x - target_world_center.x * target_zoom,
        canvas_center.y - target_world_center.y * target_zoom
    );
    
    StartAutoPanToPosition(target_pan_offset, target_zoom, canvas_size);
}

void GraphEditor::StartAutoPanToPosition(const ImVec2& target_world_pos, float target_zoom, const ImVec2& canvas_size) {
    // Cancel any existing auto-pan
    CancelAutoPan();
    
    // Set up animation state
    view_state_.auto_pan_active = true;
    view_state_.auto_pan_start_offset = view_state_.pan_offset;
    view_state_.auto_pan_target_offset = target_world_pos;
    view_state_.auto_pan_start_zoom = view_state_.zoom_scale;
    view_state_.auto_pan_target_zoom = target_zoom;
    view_state_.auto_pan_progress = 0.0f;
    view_state_.auto_pan_timer = 0.0f;
    view_state_.user_interrupted_auto_pan = false;
    
    // Adjust duration based on distance to travel
    ImVec2 distance_vec = ImVec2(
        target_world_pos.x - view_state_.pan_offset.x,
        target_world_pos.y - view_state_.pan_offset.y
    );
    float distance = sqrtf(distance_vec.x * distance_vec.x + distance_vec.y * distance_vec.y);
    float zoom_distance = fabsf(target_zoom - view_state_.zoom_scale);
    
    // Base duration with scaling based on distance
    float base_duration = 1.2f;
    float distance_factor = std::min(2.0f, distance / 500.0f); // Scale based on pixel distance
    float zoom_factor = std::min(1.5f, zoom_distance * 2.0f); // Scale based on zoom change
    
    view_state_.auto_pan_duration = base_duration + distance_factor * 0.5f + zoom_factor * 0.3f;
    view_state_.auto_pan_duration = std::max(0.8f, std::min(view_state_.auto_pan_duration, 3.0f));
}

void GraphEditor::UpdateAutoPan(float delta_time) {
    if (!view_state_.auto_pan_active) return;
    
    // Update timer and progress
    view_state_.auto_pan_timer += delta_time;
    view_state_.auto_pan_progress = view_state_.auto_pan_timer / view_state_.auto_pan_duration;
    
    // Check if animation is complete
    if (view_state_.auto_pan_progress >= 1.0f) {
        view_state_.auto_pan_progress = 1.0f;
        view_state_.auto_pan_active = false;
    }
    
    // Apply easing function for smooth animation
    float eased_progress = CameraEasing::EaseInOutCubic(view_state_.auto_pan_progress);
    
    // Interpolate camera position and zoom
    view_state_.pan_offset = CameraEasing::LerpVec2(
        view_state_.auto_pan_start_offset,
        view_state_.auto_pan_target_offset,
        eased_progress
    );
    
    view_state_.zoom_scale = CameraEasing::Lerp(
        view_state_.auto_pan_start_zoom,
        view_state_.auto_pan_target_zoom,
        eased_progress
    );
    
    // Ensure zoom stays within bounds
    view_state_.zoom_scale = std::max(0.1f, std::min(view_state_.zoom_scale, 10.0f));
}

void GraphEditor::CancelAutoPan() {
    view_state_.auto_pan_active = false;
    view_state_.user_interrupted_auto_pan = true;
}

// Helper function for automatic layout of child nodes
static void LayoutChildrenRecursive(GraphNode* parent_node, int depth, float horizontal_spacing) {
    if (!parent_node || parent_node->children.empty()) {
        return;
    }
    
    float child_y_start = parent_node->position.y + parent_node->size.y + 40.0f;
    float child_x = parent_node->position.x + horizontal_spacing;
    
    for (size_t i = 0; i < parent_node->children.size(); ++i) {
        auto child = parent_node->children[i].lock();
        if (child) {
            child->position = ImVec2(child_x, child_y_start + (i * 100.0f));
            
            // Recursively layout grandchildren
            LayoutChildrenRecursive(child.get(), depth + 1, horizontal_spacing);
        }
    }
}

// This is a free function, not part of GraphEditor class.
// It's a placeholder for how the graph view might be rendered using GraphManager.
void RenderGraphView(GraphManager& graph_manager, GraphViewState& view_state, ThemeType current_theme) {
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 canvas_pos = ImGui::GetCursorScreenPos(); // Top-left of the current window's drawable area
    ImVec2 canvas_size = ImGui::GetContentRegionAvail(); // Size of the drawable area

    // Create a temporary GraphEditor to get theme-aware colors
    GraphEditor temp_editor_for_colors;
    temp_editor_for_colors.SetCurrentTheme(current_theme);
    
    // Use theme-aware background and border colors
    ImU32 bg_color = temp_editor_for_colors.GetThemeBackgroundColor(current_theme);
    ImU32 border_color = temp_editor_for_colors.GetThemeNodeBorderColor(current_theme);
    draw_list->AddRectFilled(canvas_pos, ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y), bg_color);
    draw_list->AddRect(canvas_pos, ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y), border_color);

    // Create a GraphEditor instance to manage view state and rendering logic for this canvas
    // This might be a member of a higher-level GUI class, or created per view.
    // For simplicity, let's assume it's managed elsewhere and passed or accessed.
    // Here, we'll just use the graph_manager's internal nodes and view_state.
    // This is a conceptual merge; a real GraphEditor would encapsulate its own nodes_ map.

    // Create a temporary GraphEditor to handle interactions
    GraphEditor temp_editor_for_interactions;
    temp_editor_for_interactions.GetViewState() = view_state; // Sync view state
    temp_editor_for_interactions.SetCurrentTheme(current_theme); // Set theme for color consistency
    
    // Update auto-pan animation
    double current_time = ImGui::GetTime();
    float delta_time = (view_state.last_time > 0.0) ? (float)(current_time - view_state.last_time) : (1.0f / 60.0f);
    view_state.last_time = current_time;
    
    temp_editor_for_interactions.UpdateAutoPan(delta_time);
    
    // Handle pan/zoom interactions
    temp_editor_for_interactions.HandlePanning(canvas_pos, canvas_size);
    temp_editor_for_interactions.HandleZooming(canvas_pos, canvas_size);
    
    // Update the view_state with any changes from interactions
    view_state = temp_editor_for_interactions.GetViewState();

    // Update layout if needed using force-directed algorithm
    graph_manager.UpdateLayout();
    
    // Trigger auto-pan to newest node if layout was updated and there's a new node
    if (graph_manager.last_node_added_to_graph &&
        (graph_manager.last_node_added_to_graph->position.x != 0.0f || graph_manager.last_node_added_to_graph->position.y != 0.0f)) {
        // Only auto-pan if not already active and user hasn't interrupted
        if (!temp_editor_for_interactions.IsAutoPanActive() && !view_state.user_interrupted_auto_pan) {
            graph_manager.TriggerAutoPanToNewestNode(&temp_editor_for_interactions, canvas_size);
        }
    }

    // Rendering nodes and edges
    // This simplified RenderGraphView will directly iterate nodes from GraphManager
    // and use a temporary GraphEditor-like logic for transformations and culling.
    // A more robust design would have GraphEditor take GraphManager's data.

    // Use the same temp_editor_for_interactions for rendering transformations
    GraphEditor& temp_editor_for_render = temp_editor_for_interactions;

    std::function<void(GraphNode*)> render_recursive_lambda;
    render_recursive_lambda = 
        [&](GraphNode* node) {
        if (!node) return;

        bool node_is_visible = temp_editor_for_render.IsNodeVisible(*node, canvas_pos, canvas_size);
        
        if (node_is_visible) {
            // Simplified node rendering for this placeholder:
            ImVec2 node_screen_pos = temp_editor_for_render.WorldToScreen(node->position);
            ImVec2 node_abs_screen_pos = ImVec2(canvas_pos.x + node_screen_pos.x, canvas_pos.y + node_screen_pos.y);
            ImVec2 node_screen_size = ImVec2(node->size.x * temp_editor_for_render.GetViewState().zoom_scale, 
                                           node->size.y * temp_editor_for_render.GetViewState().zoom_scale);

            if (node_screen_size.x > 1.0f && node_screen_size.y > 1.0f) {
                // Use theme-aware colors for simplified rendering
                ImU32 node_bg_color = temp_editor_for_render.GetThemeNodeColor(current_theme);
                ImU32 node_border_color = temp_editor_for_render.GetThemeNodeBorderColor(current_theme);
                ImU32 text_color = temp_editor_for_render.GetThemeTextColor(current_theme);
                
                draw_list->AddRectFilled(node_abs_screen_pos,
                                        ImVec2(node_abs_screen_pos.x + node_screen_size.x, node_abs_screen_pos.y + node_screen_size.y),
                                        node_bg_color, 4.0f * temp_editor_for_render.GetViewState().zoom_scale);
                draw_list->AddRect(node_abs_screen_pos,
                                   ImVec2(node_abs_screen_pos.x + node_screen_size.x, node_abs_screen_pos.y + node_screen_size.y),
                                   node_border_color, 4.0f * temp_editor_for_render.GetViewState().zoom_scale);
                
                // Render full text with proper wrapping instead of truncation
                float padding = 5.0f;
                ImVec2 text_render_pos = ImVec2(node_abs_screen_pos.x + padding, node_abs_screen_pos.y + padding);
                if (node_screen_size.x > 10 && node_screen_size.y > 10) { // Only if node is somewhat visible
                    float content_width = node_screen_size.x - (2 * padding);
                    float content_height = node_screen_size.y - (2 * padding);
                    
                    // Use the full content from label (which contains the formatted message)
                    const char* text_to_display = node->label.empty() ? "[Empty Node]" : node->label.c_str();
                    
                    // Create clipping rectangle for the text area
                    ImVec4 clip_rect_vec4(node_abs_screen_pos.x + padding, node_abs_screen_pos.y + padding,
                                          node_abs_screen_pos.x + padding + content_width,
                                          node_abs_screen_pos.y + padding + content_height);
                    
                    // Force immediate text rendering for new nodes to fix auto-refresh issue
                    // Check if this node needs content refresh (new nodes or updated content)
                    if (node->content_needs_refresh) {
                        // Force immediate rendering by ensuring text is drawn with fresh parameters
                        node->content_needs_refresh = false; // Clear the flag after rendering
                    }
                    
                    // Ensure content is always displayed immediately without requiring manual refresh
                    draw_list->PushClipRect(ImVec2(clip_rect_vec4.x, clip_rect_vec4.y),
                                          ImVec2(clip_rect_vec4.z, clip_rect_vec4.w), true);
                    
                    // Use RenderWrappedText for full message content display
                    RenderWrappedText(draw_list, ImGui::GetFont(),
                                     ImGui::GetFontSize() * temp_editor_for_render.GetViewState().zoom_scale,
                                     text_render_pos, text_color, text_to_display,
                                     content_width, content_height, &clip_rect_vec4);
                    
                    draw_list->PopClipRect();
                }
            }
        }

        if (node->is_expanded) {
            node->for_each_child([&](GraphNode* child) {
                bool child_is_visible = temp_editor_for_render.IsNodeVisible(*child, canvas_pos, canvas_size);
                if (node_is_visible && child_is_visible) {
                    // Bezier curve edge rendering with theme-aware color
                    ImU32 edge_color = temp_editor_for_render.GetThemeEdgeColor(current_theme);
                    ImVec2 start_world = ImVec2(node->position.x + node->size.x / 2.0f, node->position.y + node->size.y);
                    ImVec2 end_world = ImVec2(child->position.x + child->size.x / 2.0f, child->position.y);
                    ImVec2 start_screen_rel = temp_editor_for_render.WorldToScreen(start_world);
                    ImVec2 end_screen_rel = temp_editor_for_render.WorldToScreen(end_world);

                    // Convert to absolute screen coordinates
                    ImVec2 start_screen = ImVec2(canvas_pos.x + start_screen_rel.x, canvas_pos.y + start_screen_rel.y);
                    ImVec2 end_screen = ImVec2(canvas_pos.x + end_screen_rel.x, canvas_pos.y + end_screen_rel.y);

                    // Calculate Bezier curve control points
                    ImVec2 direction = ImVec2(end_screen.x - start_screen.x, end_screen.y - start_screen.y);
                    float distance = sqrtf(direction.x * direction.x + direction.y * direction.y);

                    float control_offset = std::min(distance * 0.4f, 80.0f * temp_editor_for_render.GetViewState().zoom_scale);
                    control_offset = std::max(control_offset, 20.0f * temp_editor_for_render.GetViewState().zoom_scale);

                    // Handle edge cases for very close nodes
                    if (distance < 10.0f * temp_editor_for_render.GetViewState().zoom_scale) {
                        control_offset = std::min(control_offset, distance * 0.2f);
                    }

                    // Calculate optimal control points for standard parent-child relationships
                    ImVec2 control1 = CalculateOptimalControlPoint(start_screen, end_screen, control_offset, true, false);
                    ImVec2 control2 = CalculateOptimalControlPoint(start_screen, end_screen, control_offset, false, false);

                    // Render smooth Bezier curve with adaptive tessellation
                    float line_thickness = std::max(1.0f, 1.5f * temp_editor_for_render.GetViewState().zoom_scale);

                    int num_segments = 0; // Auto-tessellation
                    if (distance > 200.0f * temp_editor_for_render.GetViewState().zoom_scale) {
                        num_segments = std::max(8, (int)(distance / (50.0f * temp_editor_for_render.GetViewState().zoom_scale)));
                        num_segments = std::min(num_segments, 32);
                    }

                    draw_list->AddBezierCubic(start_screen, control1, control2, end_screen, edge_color, line_thickness, num_segments);
                }
                render_recursive_lambda(child);
            });

            // Render alternative paths with different curve styling
            for (const auto& weak_alt : node->alternative_paths) {
                if (auto alt_child = weak_alt.lock()) {
                    bool alt_is_visible = temp_editor_for_render.IsNodeVisible(*alt_child, canvas_pos, canvas_size);
                    if (node_is_visible && alt_is_visible) {
                        // Bezier curve edge rendering for alternative paths with distinct styling
                        ImU32 edge_color = temp_editor_for_render.GetThemeEdgeColor(current_theme);
                        
                        // Make alternative paths more transparent and thinner
                        ImU32 base_color = edge_color;
                        ImU32 alpha_mask = 0x00FFFFFF;
                        ImU32 alpha_component = (base_color & 0xFF000000) >> 1; // Half transparency
                        edge_color = (base_color & alpha_mask) | alpha_component;
                        
                        ImVec2 start_world = ImVec2(node->position.x + node->size.x / 2.0f, node->position.y + node->size.y);
                        ImVec2 end_world = ImVec2(alt_child->position.x + alt_child->size.x / 2.0f, alt_child->position.y);
                        ImVec2 start_screen_rel = temp_editor_for_render.WorldToScreen(start_world);
                        ImVec2 end_screen_rel = temp_editor_for_render.WorldToScreen(end_world);
                        
                        // Convert to absolute screen coordinates
                        ImVec2 start_screen = ImVec2(canvas_pos.x + start_screen_rel.x, canvas_pos.y + start_screen_rel.y);
                        ImVec2 end_screen = ImVec2(canvas_pos.x + end_screen_rel.x, canvas_pos.y + end_screen_rel.y);
                        
                        // Calculate Bezier curve control points for alternative paths
                        ImVec2 direction = ImVec2(end_screen.x - start_screen.x, end_screen.y - start_screen.y);
                        float distance = sqrtf(direction.x * direction.x + direction.y * direction.y);
                        
                        float control_offset = std::min(distance * 0.4f, 80.0f * temp_editor_for_render.GetViewState().zoom_scale);
                        control_offset = std::max(control_offset, 20.0f * temp_editor_for_render.GetViewState().zoom_scale);
                        
                        // Handle edge cases for very close nodes
                        if (distance < 10.0f * temp_editor_for_render.GetViewState().zoom_scale) {
                            control_offset = std::min(control_offset, distance * 0.2f);
                        }
                        
                        // Calculate optimal control points for alternative paths
                        ImVec2 control1 = CalculateOptimalControlPoint(start_screen, end_screen, control_offset, true, true);
                        ImVec2 control2 = CalculateOptimalControlPoint(start_screen, end_screen, control_offset, false, true);
                        
                        // Render smooth Bezier curve for alternative path with adaptive tessellation
                        float line_thickness = std::max(1.0f, 1.2f * temp_editor_for_render.GetViewState().zoom_scale); // Slightly thinner
                        
                        int num_segments = 0; // Auto-tessellation
                        if (distance > 200.0f * temp_editor_for_render.GetViewState().zoom_scale) {
                            num_segments = std::max(8, (int)(distance / (50.0f * temp_editor_for_render.GetViewState().zoom_scale)));
                            num_segments = std::min(num_segments, 32);
                        }
                        
                        draw_list->AddBezierCubic(start_screen, control1, control2, end_screen, edge_color, line_thickness, num_segments);
                    }
                    render_recursive_lambda(alt_child.get());
                }
            }
        }
    };

    if (graph_manager.all_nodes.empty()) {
        ImGui::TextWrapped("Graph is empty. Populate it from history or click 'Refresh Graph'.");
    } else {
        // ImGui::Text("Graph View: Rendering %zu nodes from GraphManager.", graph_manager.all_nodes.size());
        for (const auto& root_node : graph_manager.root_nodes) {
            if (root_node) {
                render_recursive_lambda(root_node.get());
            }
        }
    }
}