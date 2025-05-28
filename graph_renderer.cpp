#include "graph_renderer.h"
#include "extern/imgui/imgui_internal.h" // For ImGui::CalcTextSize, ImGui::PushClipRect, ImTextCharToUtf8
#include <string>     // For std::string, std::to_string
#include <algorithm>  // For std::min, std::max
#include <limits>     // Required for std::numeric_limits
#include <cstdio>     // For sprintf
#include <cmath>      // For sqrtf, FLT_MAX
#include <chrono>     // For std::chrono::system_clock
#include <functional> // For std::function
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

// Initialize static member
char GraphEditor::newMessageBuffer_[1024 * 16] = "";

GraphEditor::GraphEditor() {
    // view_state_ is already initialized by its default constructor
    // context_node_ and reply_parent_node_ are initialized to nullptr by default
}

int GetNextUniqueID(const std::map<int, GraphNode*>& nodes) {
    if (nodes.empty()) {
        return 1;
    }
    int max_id = 0;
    for(const auto& pair : nodes) {
        if (pair.first > max_id) {
            max_id = pair.first;
        }
    }
    return max_id + 1;
}

void GraphEditor::AddNode(GraphNode* node) {
    if (node) {
        nodes_[node->message_id] = node; // Assuming message_id is unique for now, or use graph_node_id
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
    for (auto& pair : nodes_) {
        delete pair.second; // Assuming GraphEditor owns the nodes
    }
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
        GraphNode* node_ptr = pair.second;
        if (!node_ptr) continue;
        GraphNode& node = *node_ptr;

        ImVec2 node_pos_screen_relative_to_canvas = WorldToScreen(node.position);
        ImVec2 node_size_screen = ImVec2(node.size.x * view_state_.zoom_scale, node.size.y * view_state_.zoom_scale);

        if (node_size_screen.x < 1.0f || node_size_screen.y < 1.0f) continue; 

        ImVec2 absolute_node_button_pos = ImVec2(canvas_screen_pos.x + node_pos_screen_relative_to_canvas.x, 
                                                 canvas_screen_pos.y + node_pos_screen_relative_to_canvas.y);

        ImGui::PushID(node.graph_node_id); // Use unique graph_node_id

        if (!node.children.empty() || !node.alternative_paths.empty()) {
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
                sprintf(exp_btn_id, "expcol##%d_btn", node.graph_node_id);
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
        sprintf(select_btn_id, "select##%d_btn", node.graph_node_id);
        if (ImGui::InvisibleButton(select_btn_id, node_size_screen)) {
            if (view_state_.selected_node_id != node.graph_node_id) {
                if (view_state_.selected_node_id != -1) {
                    GraphNode* prev_selected_node = GetNode(view_state_.selected_node_id);
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
            ImGui::Text("Graph Node ID: %d", selected_node_ptr->graph_node_id);
            ImGui::Text("Message ID: %d", selected_node_ptr->message_id);
            ImGui::TextWrapped("Content: %s", selected_node_ptr->message_data.content.c_str());
            ImGui::Text("Type: %d", static_cast<int>(selected_node_ptr->message_data.type));
            if (selected_node_ptr->message_data.model_id.has_value()) {
                ImGui::Text("Model ID: %s", selected_node_ptr->message_data.model_id.value().c_str());
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
    // Integrated into HandleNodeSelection
}

void GraphEditor::RenderNode(ImDrawList* draw_list, GraphNode& node) { 
    ImVec2 node_pos_screen_relative_to_canvas = WorldToScreen(node.position);
    ImVec2 node_size_screen = ImVec2(node.size.x * view_state_.zoom_scale, node.size.y * view_state_.zoom_scale);
    
    if (node_size_screen.x < 1.0f || node_size_screen.y < 1.0f) return; 

    ImVec2 final_draw_pos = node_pos_screen_relative_to_canvas; 
    ImVec2 node_end_pos = ImVec2(final_draw_pos.x + node_size_screen.x, final_draw_pos.y + node_size_screen.y);

    ImU32 bg_color = node.color; // Use node's color
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
    
    if (content_area_size.x > 1.0f && content_area_size.y > 1.0f) {
        ImVec4 clip_rect_vec4(final_draw_pos.x + padding, final_draw_pos.y + padding, 
                              final_draw_pos.x + padding + content_area_size.x, 
                              final_draw_pos.y + padding + content_area_size.y);
        draw_list->PushClipRect(ImVec2(clip_rect_vec4.x, clip_rect_vec4.y), ImVec2(clip_rect_vec4.z, clip_rect_vec4.w), true);

        const char* text_to_display = node.label.empty() ? node.message_data.content.c_str() : node.label.c_str();
        if (strlen(text_to_display) == 0) { // Check if effective text is empty
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
        for (GraphNode* child_ptr : node.children) {
            if (child_ptr) {
                bool child_is_currently_visible = IsNodeVisible(*child_ptr, canvas_screen_pos, canvas_size);
                if (node_is_currently_visible && child_is_currently_visible) {
                    RenderEdge(draw_list, node, *child_ptr); 
                }
                RenderNodeRecursive(draw_list, *child_ptr, canvas_screen_pos, canvas_size); 
            }
        }
        for (GraphNode* alt_ptr : node.alternative_paths) {
            if (alt_ptr) {
                bool alt_is_currently_visible = IsNodeVisible(*alt_ptr, canvas_screen_pos, canvas_size);
                if (node_is_currently_visible && alt_is_currently_visible) {
                    RenderEdge(draw_list, node, *alt_ptr); 
                }
                RenderNodeRecursive(draw_list, *alt_ptr, canvas_screen_pos, canvas_size); 
            }
        }
    }
}

void GraphEditor::Render(ImDrawList* draw_list, const ImVec2& canvas_screen_pos, const ImVec2& canvas_size) {
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


    std::vector<GraphNode*> root_nodes_to_render;
    if (!nodes_.empty()) {
        for (auto& pair : nodes_) {
            if (pair.second && pair.second->parent == nullptr) { 
                root_nodes_to_render.push_back(pair.second);
            }
        }
    }

    for (GraphNode* root_node_ptr : root_nodes_to_render) {
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

        ImGui::Text("Node ID: %d", context_node_->graph_node_id);
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

        ImGui::Text("Replying to Node ID: %d", reply_parent_node_->graph_node_id);
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
                new_hist_msg.message_id = -1; // This should be set by the system adding the message
                new_hist_msg.type = MessageType::USER_REPLY;
                new_hist_msg.content = new_message_content;
                new_hist_msg.timestamp = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
                new_hist_msg.parent_id = reply_parent_node_->message_id; // Original message_id of parent

                // The GraphManager should handle creating the GraphNode and assigning a unique graph_node_id
                // For now, let's simulate some parts if GraphManager isn't fully integrated here.
                // This part would typically be: graph_manager_instance.HandleNewHistoryMessage(new_hist_msg, reply_parent_node_->graph_node_id);
                
                // Example: Manually create and add if not using GraphManager directly here
                int new_graph_node_id = GetNextUniqueID(nodes_); // Generate a new graph node ID
                GraphNode* new_graph_node = new GraphNode(new_graph_node_id, new_hist_msg);
                new_graph_node->parent = reply_parent_node_;
                new_graph_node->depth = reply_parent_node_->depth + 1;
                // Position and size would be set by layout algorithm
                new_graph_node->size = ImVec2(150, 80); // Default size
                
                reply_parent_node_->children.push_back(new_graph_node);
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

// This is a free function, not part of GraphEditor class.
// It's a placeholder for how the graph view might be rendered using GraphManager.
void RenderGraphView(GraphManager& graph_manager, GraphViewState& view_state) {
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 canvas_pos = ImGui::GetCursorScreenPos(); // Top-left of the current window's drawable area
    ImVec2 canvas_size = ImGui::GetContentRegionAvail(); // Size of the drawable area

    // Basic background for the canvas
    draw_list->AddRectFilled(canvas_pos, ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y), IM_COL32(30, 30, 40, 255));
    draw_list->AddRect(canvas_pos, ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y), IM_COL32(200, 200, 200, 255));

    // Create a GraphEditor instance to manage view state and rendering logic for this canvas
    // This might be a member of a higher-level GUI class, or created per view.
    // For simplicity, let's assume it's managed elsewhere and passed or accessed.
    // Here, we'll just use the graph_manager's internal nodes and view_state.
    // This is a conceptual merge; a real GraphEditor would encapsulate its own nodes_ map.

    // Create a temporary GraphEditor to handle interactions
    GraphEditor temp_editor_for_interactions;
    temp_editor_for_interactions.GetViewState() = view_state; // Sync view state
    
    // Handle pan/zoom interactions
    temp_editor_for_interactions.HandlePanning(canvas_pos, canvas_size);
    temp_editor_for_interactions.HandleZooming(canvas_pos, canvas_size);
    
    // Update the view_state with any changes from interactions
    view_state = temp_editor_for_interactions.GetViewState();

    if (graph_manager.graph_layout_dirty && !graph_manager.all_nodes.empty()) {
        std::map<int, float> level_x_offset;
        ImVec2 layout_start_pos(20.0f, 20.0f); // Example starting position for layout
        // Call layout for each root node
        for (GraphNode* root_node : graph_manager.root_nodes) {
            if (root_node) {
                // CalculateNodePositionsRecursive(root_node, layout_start_pos, 50.0f, 100.0f, 0, level_x_offset, layout_start_pos);
                // ^ This function is not part of GraphManager or GraphEditor, it's a global/static helper.
                // It would need to be callable here. For now, assume layout is done.
            }
        }
        graph_manager.graph_layout_dirty = false;
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
                 draw_list->AddRectFilled(node_abs_screen_pos, 
                                         ImVec2(node_abs_screen_pos.x + node_screen_size.x, node_abs_screen_pos.y + node_screen_size.y), 
                                         node->color, 4.0f * temp_editor_for_render.GetViewState().zoom_scale);
                draw_list->AddRect(node_abs_screen_pos, 
                                   ImVec2(node_abs_screen_pos.x + node_screen_size.x, node_abs_screen_pos.y + node_screen_size.y), 
                                   IM_COL32(200,200,200,255), 4.0f * temp_editor_for_render.GetViewState().zoom_scale);
                
                // Simplified text
                ImVec2 text_render_pos = ImVec2(node_abs_screen_pos.x + 5, node_abs_screen_pos.y + 5);
                 if (node_screen_size.x > 10 && node_screen_size.y > 10) { // Only if node is somewhat visible
                    AddTextTruncated(draw_list, ImGui::GetFont(), ImGui::GetFontSize() * temp_editor_for_render.GetViewState().zoom_scale, 
                                     text_render_pos, IM_COL32(255,255,255,255), 
                                     node->label.c_str(), node->label.c_str() + node->label.length(), 
                                     node_screen_size.x - 10, nullptr);
                 }
            }
        }

        if (node->is_expanded) {
            for (GraphNode* child : node->children) {
                if (child) {
                    bool child_is_visible = temp_editor_for_render.IsNodeVisible(*child, canvas_pos, canvas_size);
                    if (node_is_visible && child_is_visible) {
                        // Simplified edge rendering
                        ImVec2 start_world = ImVec2(node->position.x + node->size.x / 2.0f, node->position.y + node->size.y);
                        ImVec2 end_world = ImVec2(child->position.x + child->size.x / 2.0f, child->position.y);
                        ImVec2 start_screen_rel = temp_editor_for_render.WorldToScreen(start_world);
                        ImVec2 end_screen_rel = temp_editor_for_render.WorldToScreen(end_world);
                        draw_list->AddLine(ImVec2(canvas_pos.x + start_screen_rel.x, canvas_pos.y + start_screen_rel.y), 
                                           ImVec2(canvas_pos.x + end_screen_rel.x, canvas_pos.y + end_screen_rel.y), 
                                           IM_COL32(150,150,150,255), 1.5f * temp_editor_for_render.GetViewState().zoom_scale);
                    }
                    render_recursive_lambda(child);
                }
            }
            // Similarly for alternative_paths if they are to be rendered
        }
    };

    if (graph_manager.all_nodes.empty()) {
        ImGui::TextWrapped("Graph is empty. Populate it from history or click 'Refresh Graph'.");
    } else {
        // ImGui::Text("Graph View: Rendering %zu nodes from GraphManager.", graph_manager.all_nodes.size());
        for (GraphNode* root_node : graph_manager.root_nodes) {
            render_recursive_lambda(root_node);
        }
    }
}