#pragma once

#include <vector>
#include <string> // Potentially, if HistoryMessage::message_id becomes std::string
#include "imgui.h" // For ImVec2
#include "gui_interface/gui_interface.h" // For HistoryMessage

// Forward declare GraphNode if parent/children pointers cause issues with direct include
// struct GraphNode; // Likely not needed if definition is self-contained here

struct GraphNode {
    // Core Data
    int message_id;                 // Links to HistoryMessage::message_id
    HistoryMessage message_data;    // A copy of the message content and metadata

    // Visual Properties
    ImVec2 position;                // On-screen position for rendering
    ImVec2 size;                    // Calculated size of the node for layout and rendering

    // State Flags
    bool is_expanded;               // If the node's children/alternatives are visible
    bool is_selected;               // If the node is currently selected by the user

    // Relational Pointers for Graph Structure
    GraphNode* parent;                       // Pointer to the parent node in the primary branch
    std::vector<GraphNode*> children;          // Pointers to direct children in the primary branch
    std::vector<GraphNode*> alternative_paths; // Pointers to nodes representing alternative paths/branches from this message

    // Layout Helper
    int depth;                       // Depth in the graph, useful for layout algorithms
    
    // Constructor (optional, but good practice for initialization)
    GraphNode(int id, const HistoryMessage& msg_data)
        : message_id(id), message_data(msg_data),
          position(ImVec2(0,0)), size(ImVec2(0,0)), // Default visual properties
          is_expanded(true), is_selected(false),   // Default states
          parent(nullptr), depth(0) {}             // Default relational/layout properties
};

struct GraphViewState {
    ImVec2 pan_offset;
    float zoom_scale;
    int selected_node_id;

    GraphViewState() : pan_offset(0.0f, 0.0f), zoom_scale(1.0f), selected_node_id(-1) {}
};