#include "graph_manager.h"
#include "imgui.h" // For ImVec2, ImU32, IM_COL32
#include <memory> // For std::make_unique, std::move
#include <algorithm> // For std::find_if

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
void GraphManager::PopulateGraphFromHistory(const std::vector<HistoryMessage>& history_messages) {
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

        switch (msg.type) {
            case MessageType::USER_INPUT: current_node_ptr->label = "User"; break;
            case MessageType::LLM_RESPONSE: current_node_ptr->label = "LLM"; break;
            case MessageType::STATUS: current_node_ptr->label = "Status"; break;
            case MessageType::ERROR: current_node_ptr->label = "Error"; break;
            case MessageType::USER_REPLY: current_node_ptr->label = "Reply"; break;
            default: current_node_ptr->label = "Message"; break;
        }

        current_node_ptr->parent = previous_node_ptr;
        if (previous_node_ptr != nullptr) {
            previous_node_ptr->children.push_back(current_node_ptr);
            current_node_ptr->depth = previous_node_ptr->depth + 1;
        } else {
            root_nodes.push_back(current_node_ptr);
            current_node_ptr->depth = 0;
        }

        current_node_ptr->position = ImVec2(50.0f + current_node_ptr->depth * 30.0f,
                                           static_cast<float>(std::count_if(root_nodes.begin(), root_nodes.end(), [&](GraphNode* rn){ return rn->depth == 0 && rn != current_node_ptr; }) ) * 150.0f + current_node_ptr->depth * 120.0f);
        current_node_ptr->size = ImVec2(200.0f, 80.0f);

        // Key for all_nodes is now graph_node_id
        all_nodes[current_node_ptr->graph_node_id] = std::move(new_node_unique_ptr);
        
        previous_node_ptr = current_node_ptr;
        last_node_added_to_graph = current_node_ptr;
    }
    graph_layout_dirty = true;
}

// Implementation of HandleNewHistoryMessage
// current_selected_graph_node_id is GraphNode::graph_node_id or -1
void GraphManager::HandleNewHistoryMessage(const HistoryMessage& new_msg, NodeIdType current_selected_graph_node_id) {
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

    switch (new_msg.type) {
        case MessageType::USER_INPUT: new_graph_node->label = "User"; break;
        case MessageType::LLM_RESPONSE: new_graph_node->label = "LLM"; break;
        case MessageType::STATUS: new_graph_node->label = "Status"; break;
        case MessageType::ERROR: new_graph_node->label = "Error"; break; // Typo fixed here
        case MessageType::USER_REPLY: new_graph_node->label = "Reply"; break;
        default: new_graph_node->label = "Message"; break;
    }

    new_graph_node->parent = parent_node;
    if (parent_node) {
        parent_node->children.push_back(new_graph_node);
        new_graph_node->depth = parent_node->depth + 1;
    } else {
        new_graph_node->depth = 0;
        root_nodes.push_back(new_graph_node);
    }

    if (parent_node) {
        new_graph_node->position = ImVec2(parent_node->position.x + 30.0f, parent_node->position.y + parent_node->size.y + 40.0f);
    } else {
        float y_offset = 0.0f;
        if (root_nodes.size() > 1) {
             GraphNode* last_prev_root = nullptr;
             for(auto it = root_nodes.rbegin(); it != root_nodes.rend(); ++it) {
                 if (*it != new_graph_node) {
                    last_prev_root = *it;
                    break;
                 }
             }
             if(last_prev_root) y_offset = last_prev_root->position.y + last_prev_root->size.y + 20.0f;
        }
        new_graph_node->position = ImVec2(50.0f, y_offset);
    }
    new_graph_node->size = ImVec2(200.0f, 60.0f);

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