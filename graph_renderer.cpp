#include "graph_renderer.h"
#include "extern/imgui/imgui_internal.h" // For ImGui::CalcTextSize, ImGui::PushClipRect, ImTextCharToUtf8
#include <string>     // For std::string, std::to_string
#include <algorithm>  // For std::min, std::max
#include <limits>     // Required for std::numeric_limits
#include <cstdio>     // For sprintf
#include <cmath>      // For sqrtf, FLT_MAX

// Helper to add text with truncation using ImFont::CalcWordWrapPositionA
static void AddTextTruncated(ImDrawList* draw_list, ImFont* font, float font_size, const ImVec2& pos, ImU32 col, const char* text_begin, const char* text_end, float wrap_width, const ImVec4* cpu_fine_clip_rect) {
    if (text_begin == text_end || font_size <= 0.0f || font == nullptr)
        return;

    // If wrap_width is essentially zero or negative, draw nothing or full text if no wrap_width constraint.
    // For this function, a valid positive wrap_width is expected for truncation.
    if (wrap_width <= 0.0f) {
        draw_list->AddText(font, font_size, pos, col, text_begin, text_end, 0.0f, cpu_fine_clip_rect);
        return;
    }

    float scale = font->FontSize > 0 ? font_size / font->FontSize : 1.0f;
    const char* end_of_fit_text_ptr = font->CalcWordWrapPositionA(scale, text_begin, text_end, wrap_width);

    // Check if any text fits at all
    if (end_of_fit_text_ptr == text_begin) {
        // No character fits. Try to draw ellipsis if it fits.
        const char* ellipsis = "...";
        ImVec2 ellipsis_size = font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, ellipsis);
        if (ellipsis_size.x <= wrap_width) {
            draw_list->AddText(font, font_size, pos, col, ellipsis, ellipsis + strlen(ellipsis), 0.0f, cpu_fine_clip_rect);
        }
        // Otherwise, draw nothing as not even ellipsis fits.
        return;
    }

    // If the entire text fits, draw it as is.
    if (end_of_fit_text_ptr == text_end) {
        draw_list->AddText(font, font_size, pos, col, text_begin, text_end, 0.0f, cpu_fine_clip_rect);
        return;
    }

    // Text needs truncation. Prepare to add ellipsis.
    const char* ellipsis = "...";
    ImVec2 ellipsis_size = font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, ellipsis);

    // Calculate available width for text part before ellipsis
    float width_for_text_before_ellipsis = wrap_width - ellipsis_size.x;

    std::string final_text_str;

    if (width_for_text_before_ellipsis > 0) {
        // Find how much of the original text fits in this reduced width
        const char* end_of_text_part_ptr = font->CalcWordWrapPositionA(scale, text_begin, text_end, width_for_text_before_ellipsis);
        if (end_of_text_part_ptr > text_begin) {
            final_text_str.assign(text_begin, end_of_text_part_ptr);
            final_text_str += ellipsis;
        } else {
            // Not enough space for any char + ellipsis, so just use ellipsis if it fits the original wrap_width
            if (ellipsis_size.x <= wrap_width) {
                final_text_str = ellipsis;
            } else {
                // Ellipsis itself doesn't fit. Draw nothing or what CalcWordWrapPositionA gave for original width (which might be nothing).
                // This case should ideally be caught by the initial end_of_fit_text_ptr == text_begin check if ellipsis is longer than any single char.
                // For safety, if final_text_str is empty, don't draw.
            }
        }
    } else { // Not enough space for any text part, only ellipsis might fit
        if (ellipsis_size.x <= wrap_width) {
            final_text_str = ellipsis;
        }
        // Otherwise, final_text_str remains empty, nothing is drawn.
    }

    if (!final_text_str.empty()) {
        draw_list->AddText(font, font_size, pos, col, final_text_str.c_str(), final_text_str.c_str() + final_text_str.length(), 0.0f, cpu_fine_clip_rect);
    }
}


GraphEditor::GraphEditor() {
    // view_state_ is already initialized by its default constructor
}

void GraphEditor::AddNode(GraphNode* node) {
    if (node) {
        nodes_[node->message_id] = node;
    }
}

GraphNode* GraphEditor::GetNode(int node_id) {
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
    // Check hover state using the canvas's screen position and size to define the hover rectangle.
    // ImGui::IsWindowHovered() is more for when code is inside a Begin/End block.
    // Here, we assume canvas_screen_pos and canvas_size define the interactive area.
    ImGuiIO& io = ImGui::GetIO();
    if (io.MousePos.x >= canvas_screen_pos.x && io.MousePos.x <= canvas_screen_pos.x + canvas_size.x &&
        io.MousePos.y >= canvas_screen_pos.y && io.MousePos.y <= canvas_screen_pos.y + canvas_size.y) {
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
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
                                 ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows) && // Ensure the main window containing canvas is hovered
                                 ImGui::IsMouseClicked(ImGuiMouseButton_Left) && 
                                 !ImGui::IsAnyItemHovered() && !ImGui::IsAnyItemActive();
    
    // Check if click was on canvas specifically
    ImVec2 mouse_pos = ImGui::GetMousePos();
    bool click_is_on_canvas = (mouse_pos.x >= canvas_screen_pos.x && mouse_pos.x < canvas_screen_pos.x + canvas_size.x &&
                               mouse_pos.y >= canvas_screen_pos.y && mouse_pos.y < canvas_screen_pos.y + canvas_size.y);
    if (!click_is_on_canvas) clicked_on_background = false;


    bool item_interacted_this_frame = false; // Tracks if any node button (select or expand) was clicked

    for (auto& pair : nodes_) {
        GraphNode* node_ptr = pair.second;
        if (!node_ptr) continue;
        GraphNode& node = *node_ptr;

        ImVec2 node_pos_screen_relative_to_canvas = WorldToScreen(node.position);
        ImVec2 node_size_screen = ImVec2(node.size.x * view_state_.zoom_scale, node.size.y * view_state_.zoom_scale);

        if (node_size_screen.x < 1.0f || node_size_screen.y < 1.0f) continue; 

        ImVec2 absolute_node_button_pos = ImVec2(canvas_screen_pos.x + node_pos_screen_relative_to_canvas.x, 
                                                 canvas_screen_pos.y + node_pos_screen_relative_to_canvas.y);

        ImGui::PushID(node.message_id); 

        // Expand/Collapse Button
        if (!node.children.empty() || !node.alternative_paths.empty()) {
            float icon_world_size = 10.0f; 
            ImVec2 icon_size_screen = ImVec2(icon_world_size * view_state_.zoom_scale, icon_world_size * view_state_.zoom_scale);
            if (icon_size_screen.x >= 1.0f && icon_size_screen.y >= 1.0f) { // Only if icon is reasonably sized
                ImVec2 icon_local_pos(node.size.x - icon_world_size - 2.0f, node.size.y * 0.5f - icon_world_size * 0.5f); 
                ImVec2 icon_world_pos = ImVec2(node.position.x + icon_local_pos.x, node.position.y + icon_local_pos.y);
                ImVec2 icon_screen_pos_relative_to_canvas = WorldToScreen(icon_world_pos);
                ImVec2 absolute_icon_button_pos = ImVec2(canvas_screen_pos.x + icon_screen_pos_relative_to_canvas.x, 
                                                         canvas_screen_pos.y + icon_screen_pos_relative_to_canvas.y);
                
                ImGui::SetCursorScreenPos(absolute_icon_button_pos);
                char exp_btn_id[32];
                sprintf(exp_btn_id, "expcol##%d_btn", node.message_id); 
                if (ImGui::Button(exp_btn_id, icon_size_screen)) { 
                    node.is_expanded = !node.is_expanded;
                    item_interacted_this_frame = true; 
                }
                 if (ImGui::IsItemHovered() || ImGui::IsItemActive()) item_interacted_this_frame = true; // Also count hover/active on this button
            }
        }

        // Node Selection InvisibleButton
        ImGui::SetCursorScreenPos(absolute_node_button_pos);
        char select_btn_id[32];
        sprintf(select_btn_id, "select##%d_btn", node.message_id);
        if (ImGui::InvisibleButton(select_btn_id, node_size_screen)) {
            if (view_state_.selected_node_id != node.message_id) {
                if (view_state_.selected_node_id != -1) {
                    GraphNode* prev_selected_node = GetNode(view_state_.selected_node_id);
                    if (prev_selected_node) {
                        prev_selected_node->is_selected = false;
                    }
                }
                node.is_selected = true;
                view_state_.selected_node_id = node.message_id;
            }
            item_interacted_this_frame = true; 
        }
        if (ImGui::IsItemHovered() || ImGui::IsItemActive()) item_interacted_this_frame = true; // Count hover/active on node body

        ImGui::PopID(); 
    }

    if (clicked_on_background && !item_interacted_this_frame) {
        if (view_state_.selected_node_id != -1) {
            GraphNode* current_selected_node = GetNode(view_state_.selected_node_id);
            if (current_selected_node) {
                current_selected_node->is_selected = false;
            }
            view_state_.selected_node_id = -1;
        }
    }
}


void GraphEditor::DisplaySelectedNodeDetails() {
    ImGui::Begin("Node Details"); 
    if (view_state_.selected_node_id != -1) {
        GraphNode* selected_node_ptr = GetNode(view_state_.selected_node_id);
        if (selected_node_ptr) {
            ImGui::Text("ID: %d", selected_node_ptr->message_id);
            ImGui::TextWrapped("Content: %s", selected_node_ptr->message_data.content.c_str());
            ImGui::Text("Type: %d", static_cast<int>(selected_node_ptr->message_data.type));
            if (selected_node_ptr->message_data.model_id.has_value()) {
                ImGui::Text("Model ID: %d", selected_node_ptr->message_data.model_id.value());
            }
        } else {
            ImGui::TextWrapped("Error: Selected node data not found (ID: %d).", view_state_.selected_node_id);
        }
    } else {
        ImGui::TextWrapped("Select a node to view its details.");
    }
    ImGui::End(); 
}

void GraphEditor::HandleExpandCollapse(GraphNode& node, const ImVec2& canvas_screen_pos) {
    // Button logic is now integrated into HandleNodeSelection
}


void GraphEditor::RenderNode(ImDrawList* draw_list, GraphNode& node) { 
    ImVec2 node_pos_screen_relative_to_canvas = WorldToScreen(node.position);
    ImVec2 node_size_screen = ImVec2(node.size.x * view_state_.zoom_scale, node.size.y * view_state_.zoom_scale);
    
    if (node_size_screen.x < 1.0f || node_size_screen.y < 1.0f) return; 

    ImVec2 final_draw_pos = node_pos_screen_relative_to_canvas; 
    ImVec2 node_end_pos = ImVec2(final_draw_pos.x + node_size_screen.x, final_draw_pos.y + node_size_screen.y);

    ImU32 bg_color = IM_COL32(50, 50, 50, 255);
    ImU32 border_color = IM_COL32(100, 100, 100, 255);
    float border_thickness = std::max(1.0f, 1.0f * view_state_.zoom_scale); 

    if (node.is_selected) {
        border_color = IM_COL32(255, 255, 0, 255);
        border_thickness = std::max(1.0f, 2.0f * view_state_.zoom_scale);
    }
    float rounding = std::max(0.0f, 4.0f * view_state_.zoom_scale);
    draw_list->AddRectFilled(final_draw_pos, node_end_pos, bg_color, rounding); 
    draw_list->AddRect(final_draw_pos, node_end_pos, border_color, rounding, 0, border_thickness);

    float padding = std::max(1.0f, 5.0f * view_state_.zoom_scale); 
    ImVec2 text_pos = ImVec2(final_draw_pos.x + padding, final_draw_pos.y + padding);
    ImVec2 content_area_size = ImVec2(node_size_screen.x - 2 * padding, node_size_screen.y - 2 * padding);
    
    bool has_children_or_alternatives = !node.children.empty() || !node.alternative_paths.empty();
    float icon_area_width = 0.0f;

    if (has_children_or_alternatives) {
        float icon_world_size = 10.0f; 
        icon_area_width = (icon_world_size + 2.0f) * view_state_.zoom_scale; 
        content_area_size.x -= icon_area_width;
    }
    content_area_size.x = std::max(0.0f, content_area_size.x);
    content_area_size.y = std::max(0.0f, content_area_size.y);
    
    if (content_area_size.x > 1.0f && content_area_size.y > 1.0f) { // Only draw text if area is valid
        ImVec4 clip_rect_vec4(final_draw_pos.x + padding, final_draw_pos.y + padding, 
                              final_draw_pos.x + padding + content_area_size.x, 
                              final_draw_pos.y + padding + content_area_size.y);
        draw_list->PushClipRect(ImVec2(clip_rect_vec4.x, clip_rect_vec4.y), ImVec2(clip_rect_vec4.z, clip_rect_vec4.w), true);

        const char* text_to_display = node.message_data.content.c_str();
        if (node.message_data.content.empty()) {
            text_to_display = "[Empty Node]";
        }
        
        ImFont* current_font = ImGui::GetFont();
        float base_font_size = current_font->FontSize; 
        float scaled_font_size = base_font_size * view_state_.zoom_scale;
        scaled_font_size = std::max(6.0f, std::min(scaled_font_size, 72.0f)); 
        
        AddTextTruncated(draw_list, current_font, scaled_font_size, text_pos, IM_COL32(255, 255, 255, 255), 
                         text_to_display, text_to_display + strlen(text_to_display), 
                         content_area_size.x, 
                         &clip_rect_vec4);
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

            ImU32 indicator_color = IM_COL32(200, 200, 200, 255);
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
    ImVec2 start_world = ImVec2(parent_node.position.x + parent_node.size.x / 2.0f, parent_node.position.y + parent_node.size.y);
    ImVec2 end_world = ImVec2(child_node.position.x + child_node.size.x / 2.0f, child_node.position.y);

    ImVec2 start_screen = WorldToScreen(start_world);
    ImVec2 end_screen = WorldToScreen(end_world);

    float line_thickness = std::max(1.0f, 1.5f * view_state_.zoom_scale);
    draw_list->AddLine(start_screen, end_screen, IM_COL32(150, 150, 150, 255), line_thickness);
    
    float arrow_base_size = 8.0f;
    float arrow_size = std::max(2.0f, arrow_base_size * view_state_.zoom_scale);
    ImVec2 dir = ImVec2(end_screen.x - start_screen.x, end_screen.y - start_screen.y);
    float len = sqrtf(dir.x*dir.x + dir.y*dir.y);

    if (len > arrow_size && arrow_size > 1.0f) { 
        dir.x /= len;
        dir.y /= len;
        ImVec2 p1 = ImVec2(end_screen.x - dir.x * arrow_size - dir.y * arrow_size / 2.0f, 
                           end_screen.y - dir.y * arrow_size + dir.x * arrow_size / 2.0f);
        ImVec2 p2 = ImVec2(end_screen.x - dir.x * arrow_size + dir.y * arrow_size / 2.0f, 
                           end_screen.y - dir.y * arrow_size - dir.x * arrow_size / 2.0f);
        draw_list->AddTriangleFilled(end_screen, p1, p2, IM_COL32(150, 150, 150, 255));
    }
}

void GraphEditor::RenderNodeRecursive(ImDrawList* draw_list, GraphNode& node, const ImVec2& canvas_screen_pos) {
    RenderNode(draw_list, node); 

    if (node.is_expanded) {
        for (GraphNode* child_ptr : node.children) {
            if (child_ptr) {
                RenderEdge(draw_list, node, *child_ptr); 
                RenderNodeRecursive(draw_list, *child_ptr, canvas_screen_pos); 
            }
        }
        for (GraphNode* alt_ptr : node.alternative_paths) {
            if (alt_ptr) {
                RenderEdge(draw_list, node, *alt_ptr); 
                RenderNodeRecursive(draw_list, *alt_ptr, canvas_screen_pos); 
            }
        }
    }
}


void GraphEditor::Render(ImDrawList* draw_list, const ImVec2& canvas_screen_pos, const ImVec2& canvas_size) {
    // Handle interactions. These use canvas_screen_pos (absolute top-left of canvas) and canvas_size.
    HandlePanning(canvas_screen_pos, canvas_size); 
    HandleZooming(canvas_screen_pos, canvas_size);
    // HandleNodeSelection also handles expand/collapse button logic internally now.
    HandleNodeSelection(draw_list, canvas_screen_pos, canvas_size); 
    
    std::vector<GraphNode*> root_nodes;
    if (!nodes_.empty()) {
        for (auto& pair : nodes_) {
            if (pair.second && pair.second->parent == nullptr) { 
                root_nodes.push_back(pair.second);
            }
        }
        // If no roots found but nodes exist, it might be a partially formed graph or all nodes are children.
        // For this rendering pass, we only start from explicit roots.
        // If all nodes should be rendered regardless of being a root, then iterate all nodes_ here.
        // However, RenderNodeRecursive will handle children, so only roots are needed.
        if (root_nodes.empty()) {
             // Fallback: if no roots, render all nodes individually (no edges or hierarchy)
             // This might be useful during initial population before parent links are set.
             // Or, this indicates an issue if a tree structure is expected.
             // For now, let's stick to rendering from roots. If no roots, nothing from this loop.
        }
    }

    // The draw_list is assumed to be for the canvas, with its origin at canvas_screen_pos.
    // All drawing coordinates from WorldToScreen are relative to this canvas origin.
    // So, when calling AddRectFilled, AddLine etc., these relative coordinates are correct.
    for (GraphNode* root_node_ptr : root_nodes) {
        if (root_node_ptr) {
            RenderNodeRecursive(draw_list, *root_node_ptr, canvas_screen_pos);
        }
    }
}