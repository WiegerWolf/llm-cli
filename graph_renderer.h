#ifndef GRAPH_RENDERER_H
#define GRAPH_RENDERER_H

#include "extern/imgui/imgui.h"
#include "gui_interface/graph_types.h" // For GraphNode and GraphViewState
#include <vector> // For storing GraphNode pointers or objects
#include <map>    // For managing nodes by ID

// Forward declaration if GraphNode is complex and defined elsewhere,
// or include the necessary header if it's simple.
// Assuming GraphNode is defined in "gui_interface/graph_types.h"

class GraphEditor {
public:
    GraphEditor();

    void Render(ImDrawList* draw_list, const ImVec2& canvas_pos, const ImVec2& canvas_size);
    void AddNode(GraphNode* node); // Or pass by value/reference depending on ownership model
    GraphNode* GetNode(int node_id);
    void ClearNodes(); // If nodes are managed internally

    // Interaction Handlers
    void HandlePanning(const ImVec2& canvas_pos, const ImVec2& canvas_size); // Corrected typo
    void HandleZooming(const ImVec2& canvas_pos, const ImVec2& canvas_size);
    void HandleNodeSelection(ImDrawList* draw_list, const ImVec2& canvas_pos, const ImVec2& canvas_size);
    void DisplaySelectedNodeDetails(); // To be called by main GUI loop
    void HandleExpandCollapse(GraphNode& node, const ImVec2& canvas_pos); // Handles button logic for expand/collapse
    void RenderPopups(ImDrawList* draw_list, const ImVec2& canvas_pos); // Renders context menus and modals

    ImVec2 WorldToScreen(const ImVec2& world_pos) const;
    ImVec2 ScreenToWorld(const ImVec2& screen_pos, const ImVec2& canvas_screen_pos) const;

    GraphViewState& GetViewState() { return view_state_; } // Getter for view_state

    // Culling helper
    bool IsNodeVisible(const GraphNode& node, const ImVec2& canvas_screen_pos, const ImVec2& canvas_size) const;

private:
    GraphViewState view_state_;
    std::map<int, GraphNode*> nodes_;
    GraphNode* context_node_ = nullptr; // Node for which context menu is triggered
    GraphNode* reply_parent_node_ = nullptr; // Parent node for the new message

    // Buffer for the new message input modal
    static char newMessageBuffer_[1024 * 16];

    // Methods for context menu and modal
    void RenderNodeContextMenu();
    void RenderNewMessageModal(ImDrawList* draw_list, const ImVec2& canvas_pos); // Pass draw_list and canvas_pos if needed for node creation logic

    void RenderNode(ImDrawList* draw_list, GraphNode& node);
    void RenderEdge(ImDrawList* draw_list, const GraphNode& parent_node, const GraphNode& child_node);
    // Recursive rendering helper for nodes and their children if expanded
    void RenderNodeRecursive(ImDrawList* draw_list, GraphNode& node, const ImVec2& canvas_screen_pos, const ImVec2& canvas_size);
};

#endif // GRAPH_RENDERER_H