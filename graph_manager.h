#pragma once

#include <vector>
#include <memory>
#include <string>
#include <unordered_map>
#include "gui_interface/graph_types.h"
#include "gui_interface/gui_interface.h" // For HistoryMessage

// Define MessageId if it's not globally available
// NodeIdType will be the type for unique graph node IDs.
using NodeIdType = int;

class GraphManager {
public:
    // Graph Data
    std::unordered_map<NodeIdType, std::unique_ptr<GraphNode>> all_nodes; // Main storage for all nodes, keyed by GraphNode::graph_node_id
    std::vector<GraphNode*> root_nodes; // Pointers to root nodes
    GraphNode* last_node_added_to_graph = nullptr; // Pointer to the most recently added node

    // Graph View State
    GraphViewState graph_view_state; // Contains selected_node_id (which is a graph_node_id), pan, zoom

    // State Flags
    bool graph_layout_dirty = false; // Flag to indicate if graph layout needs recalculation
    
    // ID Generation
    NodeIdType next_graph_node_id_counter;


    GraphManager(); // Constructor to initialize members

    void PopulateGraphFromHistory(const std::vector<HistoryMessage>& history_messages);
    // current_selected_node_id is GraphNode::graph_node_id
    void HandleNewHistoryMessage(const HistoryMessage& new_msg, NodeIdType current_selected_graph_node_id);
    
    // Helper to get a node pointer by its unique graph_node_id
    GraphNode* GetNodeById(NodeIdType graph_node_id);

    // Placeholder for graph rendering logic, to be implemented in a later step
    // void RenderGraphView(); // This will likely be a free function or part of a different class
};

// Placeholder for the graph rendering function, to be defined elsewhere (e.g., graph_renderer.cpp)
// It needs access to GraphManager, so it might take it as a parameter.
// void RenderGraphView(GraphManager& graph_manager, GraphViewState& view_state); // Commented out to resolve conflict