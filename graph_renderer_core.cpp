#include "graph_renderer.h"
#include "camera_utils.h"
#include "graph_drawing_utils.h"
#include "theme_utils.h"
#include "graph_manager.h"
#include "graph_layout.h"
#include "id_types.h"
#include "extern/imgui/imgui_internal.h"
#include <string>
#include <algorithm>
#include <cstdio>
#include <cinttypes>
#include <chrono>

// Initialize the static member for the new message buffer
char GraphEditor::newMessageBuffer_[1024 * 16] = {0};

GraphEditor::GraphEditor(GraphManager* graph_manager) : m_graph_manager(graph_manager) {
    // context_node_ and reply_parent_node_ are initialized to nullptr by default
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
    return CameraUtils::WorldToScreen(world_pos, view_state);
}

ImVec2 GraphEditor::ScreenToWorld(const ImVec2& screen_pos_absolute, const ImVec2& canvas_screen_pos_absolute, const GraphViewState& view_state) const {
    return CameraUtils::ScreenToWorld(screen_pos_absolute, canvas_screen_pos_absolute, view_state);
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

void GraphEditor::Render(ImDrawList* draw_list, const ImVec2& canvas_pos, const ImVec2& canvas_size, GraphViewState& view_state) {
    draw_list->PushClipRect(canvas_pos, ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y), true);

    if (m_graph_manager) {
        auto root_nodes = m_graph_manager->GetRootNodes();
        auto all_nodes = m_graph_manager->GetAllNodes();

        ClearNodes();
        for (const auto& pair : all_nodes) {
            AddNode(pair.second);
        }

        for (auto& root_node : root_nodes) {
            if (root_node) {
                 RenderNodeRecursive(draw_list, *root_node, canvas_pos, canvas_size, view_state);
            }
        }
    }

    HandlePanning(canvas_pos, canvas_size, view_state);
    HandleZooming(canvas_pos, canvas_size, view_state);
    HandleNodeSelection(draw_list, canvas_pos, canvas_size, view_state);

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
    RenderNewMessageModal(draw_list, canvas_pos);
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
}

void GraphEditor::RenderNewMessageModal(ImDrawList* draw_list, const ImVec2& canvas_pos) {
    if (ImGui::BeginPopupModal("New Message Modal", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Enter new message content:");
        ImGui::InputTextMultiline("##new_message_input", newMessageBuffer_, sizeof(newMessageBuffer_), ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 8));

        if (ImGui::Button("OK", ImVec2(120, 0))) {
            if (reply_parent_node_ && m_graph_manager) {
                m_graph_manager->CreateNode(reply_parent_node_->graph_node_id, MessageType::USER_REPLY, newMessageBuffer_);
            }
            memset(newMessageBuffer_, 0, sizeof(newMessageBuffer_));
            reply_parent_node_ = nullptr;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SetItemDefaultFocus();
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            memset(newMessageBuffer_, 0, sizeof(newMessageBuffer_));
            reply_parent_node_ = nullptr;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}
void GraphEditor::CancelAutoPan(GraphViewState& view_state) {
    view_state.auto_pan_active = false;
}

void GraphEditor::StartAutoPanToNode(const std::shared_ptr<GraphNode>& target_node, const ImVec2& canvas_size) {
    if (!target_node) return;
    
    GraphViewState view_state = m_graph_manager->getGraphViewStateSnapshot();

    ImVec2 target_world_pos = ImVec2(target_node->position.x + target_node->size.x / 2,
                                     target_node->position.y + target_node->size.y / 2);

    StartAutoPanToPosition(target_world_pos, view_state.zoom_scale, canvas_size, view_state);
    
    m_graph_manager->setGraphViewState(view_state);
}

void GraphEditor::StartAutoPanToPosition(const ImVec2& target_world_pos, float target_zoom, const ImVec2& canvas_size, GraphViewState& view_state) {
    view_state.auto_pan_active = true;
    view_state.auto_pan_timer = 0.0f;
    
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
    float eased_t = CameraUtils::Easing::EaseInOutCubic(t);

    view_state.pan_offset = CameraUtils::Easing::LerpVec2(view_state.auto_pan_start_offset, view_state.auto_pan_target_offset, eased_t);
    view_state.zoom_scale = CameraUtils::Easing::Lerp(view_state.auto_pan_start_zoom, view_state.auto_pan_target_zoom, eased_t);

    if (t >= 1.0f) {
        view_state.auto_pan_active = false;
    }
}

bool GraphEditor::IsNodeVisible(const GraphNode& node, const ImVec2& canvas_screen_pos, const ImVec2& canvas_size, const GraphViewState& view_state) const {
    ImVec2 node_pos_screen = WorldToScreen(node.position, view_state);
    ImVec2 node_size_screen = ImVec2(node.size.x * view_state.zoom_scale, node.size.y * view_state.zoom_scale);

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

void GraphEditor::RenderNode(ImDrawList* draw_list, GraphNode& node, const GraphViewState& view_state) {
    ImVec2 node_pos_screen_relative_to_canvas = WorldToScreen(node.position, view_state);
    ImVec2 node_size_screen = ImVec2(node.size.x * view_state.zoom_scale, node.size.y * view_state.zoom_scale);

    if (node_size_screen.x < 1.0f || node_size_screen.y < 1.0f) return;

    if (node.children.size() > 0 || node.alternative_paths.size() > 0) {
        float icon_world_size = 10.0f;
        ImVec2 icon_size_screen = ImVec2(icon_world_size * view_state.zoom_scale, icon_world_size * view_state.zoom_scale);

        if (icon_size_screen.x >= 1.0f && icon_size_screen.y >= 1.0f) {
            ImVec2 icon_local_pos(node.size.x - icon_world_size - 2.0f, node.size.y * 0.5f - icon_world_size * 0.5f);
            ImVec2 icon_world_pos = ImVec2(node.position.x + icon_local_pos.x, node.position.y + icon_local_pos.y);
            ImVec2 icon_screen_pos = WorldToScreen(icon_world_pos, view_state);
            ImU32 icon_color = ThemeUtils::GetThemeExpandCollapseIconColor(current_theme_);
            float h = icon_size_screen.y * 0.866f; 
            if (node.is_expanded) {
                draw_list->AddTriangleFilled(icon_screen_pos,
                                             ImVec2(icon_screen_pos.x + icon_size_screen.x, icon_screen_pos.y),
                                             ImVec2(icon_screen_pos.x + icon_size_screen.x * 0.5f, icon_screen_pos.y + h),
                                             icon_color);
            } else {
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

    draw_list->AddLine(p1, p2, ThemeUtils::GetThemeEdgeColor(current_theme_), 1.0f);
}

void GraphEditor::RenderBezierEdge(ImDrawList* draw_list, const GraphNode& parent_node, const GraphNode& child_node, const GraphViewState& view_state, bool is_alternative_path) {
    ImVec2 parent_center_world = ImVec2(parent_node.position.x + parent_node.size.x * 0.5f, parent_node.position.y + parent_node.size.y);
    ImVec2 child_center_world = ImVec2(child_node.position.x + child_node.size.x * 0.5f, child_node.position.y);

    ImVec2 p1 = WorldToScreen(parent_center_world, view_state);
    ImVec2 p2 = WorldToScreen(child_center_world, view_state);

    float vertical_offset = (p2.y - p1.y) * 0.4f;
    ImVec2 cp1 = ImVec2(p1.x, p1.y + vertical_offset);
    ImVec2 cp2 = ImVec2(p2.x, p2.y - vertical_offset);

    ImU32 edge_color = ThemeUtils::GetThemeEdgeColor(current_theme_);
    float edge_thickness = 1.2f;
    if (is_alternative_path) {
        edge_color = IM_COL32(100, 100, 150, 200);
        edge_thickness = 0.8f;
    }

    draw_list->AddBezierCubic(p1, cp1, cp2, p2, edge_color, edge_thickness);
}


void RenderGraphView(GraphManager& graph_manager, ThemeType current_theme, bool create_window) {
    static GraphEditor graph_editor(&graph_manager);
    graph_editor.SetCurrentTheme(current_theme);

    if (create_window) {
        if (!ImGui::Begin("Graph View")) {
            ImGui::End();
            return;
        }
    }

    ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
    ImVec2 canvas_size = ImGui::GetContentRegionAvail();
    if (canvas_size.x < 50.0f) canvas_size.x = 50.0f;
    if (canvas_size.y < 50.0f) canvas_size.y = 50.0f;

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->AddRectFilled(canvas_pos, ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y), ThemeUtils::GetThemeBackgroundColor(current_theme));
    draw_list->AddRect(canvas_pos, ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y), IM_COL32(100, 100, 100, 255));
    
    GraphViewState view_state = graph_manager.getGraphViewStateSnapshot();

    graph_editor.Render(draw_list, canvas_pos, canvas_size, view_state);
    
    graph_editor.UpdateAutoPan(ImGui::GetIO().DeltaTime, view_state);
    
    graph_manager.setGraphViewState(view_state);

    if (create_window) {
        ImGui::End();
    }

    graph_editor.DisplaySelectedNodeDetails(view_state);
}