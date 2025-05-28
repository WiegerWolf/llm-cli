#pragma once

#include <vector>
#include <memory>
#include <string>
#include <unordered_map>
#include "gui_interface/graph_types.h"
#include "gui_interface/gui_interface.h" // For HistoryMessage

// Define MessageId if it's not globally available
// Assuming MessageId is int based on GraphNode::message_id
using MessageId = int;

class GraphManager {
public:
    std::vector<std::unique_ptr<GraphNode>> all_nodes;
    GraphNode* root_node = nullptr;
    std::unordered_map<MessageId, GraphNode*> node_map;

    GraphManager() = default;

    void PopulateGraphFromHistory(const std::vector<HistoryMessage>& history_messages);
    // Placeholder for graph rendering logic, to be implemented in a later step
    // void RenderGraphView(); // This will likely be a free function or part of a different class
};

// Placeholder for the graph rendering function, to be defined elsewhere (e.g., graph_renderer.cpp)
// It needs access to GraphManager, so it might take it as a parameter.
void RenderGraphView(GraphManager& graph_manager);