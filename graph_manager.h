#pragma once

#include <vector>
#include <memory>
#include <string>
#include <unordered_map>
#include <shared_mutex>
#include "id_types.h"                   // Centralized definition of NodeIdType (int64_t) and sentinel
#include "gui_interface/graph_types.h"
#include "gui_interface/gui_interface.h" // For HistoryMessage
#include "database.h" // For PersistenceManager
#include "graph_layout.h" // For ForceDirectedLayout

// NodeIdType is now defined in id_types.h as std::int64_t to prevent overflow.

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
    
    // Layout System
    ForceDirectedLayout force_layout; // Force-directed layout algorithm
    bool use_force_layout = true; // Flag to enable/disable force-directed layout
    
    // ID Generation
    NodeIdType next_graph_node_id_counter;

    // Thread-safety mutex guards graph data and flags
    mutable std::shared_mutex m_mutex;


    GraphManager(); // Constructor to initialize members

    void PopulateGraphFromHistory(const std::vector<HistoryMessage>& history_messages, PersistenceManager& db_manager);
    // current_selected_node_id is GraphNode::graph_node_id
    void HandleNewHistoryMessage(const HistoryMessage& new_msg, NodeIdType current_selected_graph_node_id, PersistenceManager& db_manager);
    
    // Helper to get a node pointer by its unique graph_node_id
    GraphNode* GetNodeById(NodeIdType graph_node_id);
    
    // Layout management functions
    void UpdateLayout(); // Apply force-directed layout if needed
    void SetLayoutParams(const ForceDirectedLayout::LayoutParams& params);
    void ToggleForceLayout(bool enable);
    bool IsLayoutRunning() const; // Check if layout animation is currently running
    void RestartLayoutAnimation(); // Restart the layout animation from the beginning
    void SetAnimationSpeed(float speed_multiplier); // Set animation speed multiplier
    
    // Get all nodes as raw pointers for layout algorithms
    std::vector<GraphNode*> GetAllNodes();
    
    // Auto-pan functionality
    void TriggerAutoPanToNewestNode(class GraphEditor* graph_editor, const ImVec2& canvas_size);

    // Placeholder for graph rendering logic, to be implemented in a later step
    // void RenderGraphView(); // This will likely be a free function or part of a different class
};

// Helper function to calculate dynamic node size based on content
ImVec2 CalculateNodeSize(const std::string& content);

// Placeholder for the graph rendering function, to be defined elsewhere (e.g., graph_renderer.cpp)
// It needs access to GraphManager, so it might take it as a parameter.
// void RenderGraphView(GraphManager& graph_manager, GraphViewState& view_state); // Commented out to resolve conflict