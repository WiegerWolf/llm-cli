#pragma once
 
#include <vector>
#include <string>
#include <memory>
#include "../extern/imgui/imgui.h"          // For ImVec2
#include "id_types.h"                       // Defines NodeIdType (std::int64_t) and kInvalidNodeId
#include "gui_interface/gui_interface.h"    // For HistoryMessage

struct GraphNode; // Forward declaration
// Helper to safely obtain a raw pointer from a weak_ptr (nullptr if expired)
inline GraphNode* Raw(const std::weak_ptr<GraphNode>& wp);

// Forward declare GraphNode if parent/children pointers cause issues with direct include
// struct GraphNode; // Likely not needed if definition is self-contained here

struct GraphNode : public std::enable_shared_from_this<GraphNode> {
    // Core Data
    NodeIdType graph_node_id;       // Unique ID for this graph node (64-bit to avoid overflow)
    NodeIdType message_id;          // Original ID from HistoryMessage (64-bit, matches HistoryMessage::message_id)
    HistoryMessage message_data;    // A copy of the message content and metadata
    std::string label;              // Node label, e.g., a summary or type of message

    // Visual Properties
    ImVec2 position;                // On-screen position for rendering
    ImVec2 size;                    // Calculated size of the node for layout and rendering

    // State Flags
    bool is_expanded;               // If the node's children/alternatives are visible
    bool is_selected;               // If the node is currently selected by the user
    bool content_needs_refresh;     // Flag to force content re-rendering (fixes auto-refresh issue)

    // Relational Pointers for Graph Structure
    // Relational references (non-owning)
    std::weak_ptr<GraphNode> parent;                       // Weak reference to parent node
    std::vector<std::weak_ptr<GraphNode>> children;        // Weak references to direct children
    std::vector<std::weak_ptr<GraphNode>> alternative_paths; // Weak references for alternative paths
    ImU32 color;                    // Node color

    // Layout Helper
    int depth;                       // Depth in the graph, useful for layout algorithms
    
    // Constructor (optional, but good practice for initialization)
    GraphNode(NodeIdType g_node_id, const HistoryMessage& msg_data)
        : graph_node_id(g_node_id), message_id(msg_data.message_id), message_data(msg_data),
          position(ImVec2(0,0)), size(ImVec2(0,0)), // Default visual properties
          is_expanded(true), is_selected(false), content_needs_refresh(true), // Default states - new nodes need refresh
          parent(), depth(0), color(IM_COL32(200, 200, 200, 255)), label("") {} // Weak parent default-constructed
 
    // Helper accessors for safe, convenient access to related nodes
    GraphNode* parent_raw() const {
        if (auto sp = parent.lock()) {
            return sp.get();
        }
        return nullptr;
    }
 
    void add_child(const std::shared_ptr<GraphNode>& child_node) {
        children.emplace_back(child_node);
    }
 
    template<typename Func>
    void for_each_child(Func&& func) const {
        for (const auto& weak_child : children) {
            if (auto shared_child = weak_child.lock()) {
                func(shared_child.get());
            }
        }
    }
};

inline GraphNode* Raw(const std::weak_ptr<GraphNode>& wp) {
    if (auto sp = wp.lock()) {
        return sp.get();
    }
    return nullptr;
}

struct GraphViewState {
    ImVec2 pan_offset;
    float zoom_scale;
    NodeIdType selected_node_id; // Stores the unique graph_node_id of the selected node, -1 if none

    // Camera auto-pan animation state
    bool auto_pan_active;
    ImVec2 auto_pan_start_offset;
    ImVec2 auto_pan_target_offset;
    float auto_pan_start_zoom;
    float auto_pan_target_zoom;
    float auto_pan_progress;
    float auto_pan_duration;
    float auto_pan_timer;
    bool user_interrupted_auto_pan;
    double last_time = 0.0;

    GraphViewState() : pan_offset(0.0f, 0.0f), zoom_scale(1.0f), selected_node_id(kInvalidNodeId),
                       auto_pan_active(false), auto_pan_start_offset(0.0f, 0.0f),
                       auto_pan_target_offset(0.0f, 0.0f), auto_pan_start_zoom(1.0f),
                       auto_pan_target_zoom(1.0f), auto_pan_progress(0.0f),
                       auto_pan_duration(1.5f), auto_pan_timer(0.0f),
                       user_interrupted_auto_pan(false), last_time(0.0) {}
};