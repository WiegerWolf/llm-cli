#include "graph_manager.h"
#include "imgui.h" // For ImVec2, ImU32, IM_COL32
#include <memory> // For std::make_unique, std::move
#include <algorithm> // For std::find_if
#include <cmath> // For std::max, std::min

// Forward declaration of helper function from main_gui.cpp
extern std::string FormatMessageForGraph(const HistoryMessage& msg, PersistenceManager& db_manager);

// Helper function to calculate dynamic node size based on content
ImVec2 CalculateNodeSize(const std::string& content) {
    if (content.empty()) {
        return ImVec2(300.0f, 100.0f); // Larger minimum size for empty content
    }
    
    // Calculate text size with wrapping
    float max_width = 500.0f; // Increased maximum node width
    float min_width = 300.0f; // Increased minimum width for better readability
    float min_height = 100.0f; // Increased minimum node height
    float padding = 20.0f; // Increased internal padding
    
    // Calculate the optimal width first
    ImVec2 single_line_size = ImGui::CalcTextSize(content.c_str(), nullptr, false, FLT_MAX);
    float node_width = std::max(min_width, std::min(max_width, single_line_size.x + 2 * padding));
    
    // Now calculate height with the determined width
    float effective_width = node_width - 2 * padding;
    ImVec2 wrapped_text_size = ImGui::CalcTextSize(content.c_str(), nullptr, false, effective_width);
    float node_height = std::max(min_height, wrapped_text_size.y + 2 * padding);
    
    // Add extra height for expand/collapse icons and better spacing
    node_height += 30.0f;
    
    // Ensure reasonable aspect ratio - don't make nodes too tall and narrow
    if (node_height > node_width * 1.5f) {
        node_width = std::min(max_width, node_height / 1.5f);
        // Recalculate height with new width
        effective_width = node_width - 2 * padding;
        wrapped_text_size = ImGui::CalcTextSize(content.c_str(), nullptr, false, effective_width);
        node_height = std::max(min_height, wrapped_text_size.y + 2 * padding + 30.0f);
    }
    
    return ImVec2(node_width, node_height);
}

// Constructor
GraphManager::GraphManager()
    : last_node_added_to_graph(nullptr),
      graph_layout_dirty(false),
      next_graph_node_id_counter(0) { // Initialize ID counter
    // graph_view_state is default constructed (selected_node_id = -1)
}

// Helper to get a node by its unique graph_node_id
GraphNode* GraphManager::GetNodeById(NodeIdType graph_node_id) {
    if (graph_node_id == -1) return nullptr;
    auto it = all_nodes.find(graph_node_id);
    if (it != all_nodes.end()) {
        return it->second.get();
    }
    return nullptr;
}

// Implementation of PopulateGraphFromHistory
void GraphManager::PopulateGraphFromHistory(const std::vector<HistoryMessage>& history_messages, PersistenceManager& db_manager) {
    all_nodes.clear();
    root_nodes.clear();
    last_node_added_to_graph = nullptr;
    graph_view_state.selected_node_id = -1;
    next_graph_node_id_counter = 0; // Reset ID counter

    GraphNode* previous_node_ptr = nullptr;

    for (const auto& msg : history_messages) {
        NodeIdType current_g_node_id = next_graph_node_id_counter++;
        // Constructor now takes (graph_node_id, HistoryMessage)
        auto new_node_unique_ptr = std::make_unique<GraphNode>(current_g_node_id, msg);
        GraphNode* current_node_ptr = new_node_unique_ptr.get();

        // Use the helper function to format message content consistently with linear view
        current_node_ptr->label = FormatMessageForGraph(msg, db_manager);

        current_node_ptr->parent = previous_node_ptr;
        if (previous_node_ptr != nullptr) {
            previous_node_ptr->children.push_back(current_node_ptr);
            current_node_ptr->depth = previous_node_ptr->depth + 1;
        } else {
            root_nodes.push_back(current_node_ptr);
            current_node_ptr->depth = 0;
        }

        current_node_ptr->size = CalculateNodeSize(current_node_ptr->label);
        
        // Calculate position based on depth and previous nodes
        float x_pos = 50.0f + current_node_ptr->depth * 550.0f; // Increased spacing for wider nodes
        float y_pos = 50.0f;
        
        // For root nodes, stack them vertically with proper spacing
        if (current_node_ptr->depth == 0) {
            float total_height = 0.0f;
            for (GraphNode* root : root_nodes) {
                if (root != current_node_ptr) {
                    total_height += root->size.y + 30.0f; // Increased spacing between nodes
                }
            }
            y_pos = 50.0f + total_height;
        } else {
            // For child nodes, position relative to parent
            if (previous_node_ptr) {
                y_pos = previous_node_ptr->position.y + previous_node_ptr->size.y + 50.0f; // Increased vertical spacing
            }
        }
        
        current_node_ptr->position = ImVec2(x_pos, y_pos);

        // Key for all_nodes is now graph_node_id
        all_nodes[current_node_ptr->graph_node_id] = std::move(new_node_unique_ptr);
        
        previous_node_ptr = current_node_ptr;
        last_node_added_to_graph = current_node_ptr;
    }
    graph_layout_dirty = true;
}

// Implementation of HandleNewHistoryMessage
// current_selected_graph_node_id is GraphNode::graph_node_id or -1
void GraphManager::HandleNewHistoryMessage(const HistoryMessage& new_msg, NodeIdType current_selected_graph_node_id, PersistenceManager& db_manager) {
    GraphNode* parent_node = nullptr;

    // 1. Determine Parent Node
    if (current_selected_graph_node_id != -1) {
        parent_node = GetNodeById(current_selected_graph_node_id);
    }

    if (!parent_node && last_node_added_to_graph) {
        if (all_nodes.count(last_node_added_to_graph->graph_node_id)) { // Check using graph_node_id
            parent_node = last_node_added_to_graph;
        } else {
            last_node_added_to_graph = nullptr;
        }
    }
    
    // 2. Create and Link New GraphNode
    NodeIdType new_g_node_id = next_graph_node_id_counter++;
    // Constructor now takes (graph_node_id, HistoryMessage)
    auto new_graph_node_unique_ptr = std::make_unique<GraphNode>(new_g_node_id, new_msg);
    GraphNode* new_graph_node = new_graph_node_unique_ptr.get();

    // Use the helper function to format message content consistently with linear view
    new_graph_node->label = FormatMessageForGraph(new_msg, db_manager);

    new_graph_node->parent = parent_node;
    if (parent_node) {
        parent_node->children.push_back(new_graph_node);
        new_graph_node->depth = parent_node->depth + 1;
    } else {
        new_graph_node->depth = 0;
        root_nodes.push_back(new_graph_node);
    }

    new_graph_node->size = CalculateNodeSize(new_graph_node->label);
    
    if (parent_node) {
        // Position child nodes to the right of parent with proper spacing
        float x_pos = parent_node->position.x + 550.0f; // Increased spacing for wider nodes
        float y_pos = parent_node->position.y;
        
        // If parent already has children, stack this one below them
        if (!parent_node->children.empty()) {
            GraphNode* last_child = parent_node->children.back();
            y_pos = last_child->position.y + last_child->size.y + 30.0f; // Increased spacing
        }
        
        new_graph_node->position = ImVec2(x_pos, y_pos);
    } else {
        // For root nodes, stack vertically with proper spacing
        float y_offset = 50.0f;
        if (root_nodes.size() > 1) {
             GraphNode* last_prev_root = nullptr;
             for(auto it = root_nodes.rbegin(); it != root_nodes.rend(); ++it) {
                 if (*it != new_graph_node) {
                    last_prev_root = *it;
                    break;
                 }
             }
             if(last_prev_root) y_offset = last_prev_root->position.y + last_prev_root->size.y + 30.0f; // Increased spacing
        }
        new_graph_node->position = ImVec2(50.0f, y_offset);
    }

    // Key for all_nodes is now graph_node_id
    all_nodes[new_graph_node->graph_node_id] = std::move(new_graph_node_unique_ptr);

    last_node_added_to_graph = new_graph_node;
    graph_layout_dirty = true;
}

// Placeholder for RenderGraphView - to be implemented in graph_renderer.cpp or similar
// The signature in graph_manager.h is: void RenderGraphView(GraphManager& graph_manager, GraphViewState& view_state);
// This definition should be in graph_renderer.cpp
// void RenderGraphView(GraphManager& graph_manager, GraphViewState& view_state) {
//     // Actual rendering logic will go into graph_renderer.cpp
// }