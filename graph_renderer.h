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

    ImVec2 WorldToScreen(const ImVec2& world_pos) const;
    ImVec2 ScreenToWorld(const ImVec2& screen_pos, const ImVec2& canvas_screen_pos) const;

    GraphViewState& GetViewState() { return view_state_; } // Getter for view_state

private:
    GraphViewState view_state_;
    std::map<int, GraphNode*> nodes_;

    void RenderNode(ImDrawList* draw_list, GraphNode& node);
    void RenderEdge(ImDrawList* draw_list, const GraphNode& parent_node, const GraphNode& child_node);
    // Recursive rendering helper for nodes and their children if expanded
    void RenderNodeRecursive(ImDrawList* draw_list, GraphNode& node, const ImVec2& canvas_pos);
};

#endif // GRAPH_RENDERER_H