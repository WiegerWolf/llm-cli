#ifndef GRAPH_RENDERER_H
#define GRAPH_RENDERER_H

#include "extern/imgui/imgui.h"
#include "gui_interface/graph_types.h" // For GraphNode and GraphViewState
#include "gui_interface/gui_interface.h" // For ThemeType
#include "id_types.h"
#include <map>    // For managing nodes by ID
#include <memory> // For std::shared_ptr
#include <vector> // For storing GraphNode pointers or objects

// Forward declaration if GraphNode is complex and defined elsewhere,
// or include the necessary header if it's simple.
// Assuming GraphNode is defined in "gui_interface/graph_types.h"

// Forward declaration for GraphManager
class GraphManager;

class GraphEditor {
public:
    GraphEditor(GraphManager* graph_manager);

    void Render(ImDrawList* draw_list, const ImVec2& canvas_pos, const ImVec2& canvas_size, GraphViewState& view_state);
    void AddNode(std::shared_ptr<GraphNode> node);
    std::shared_ptr<GraphNode> GetNode(NodeIdType node_id);
    void ClearNodes(); // If nodes are managed internally

    // Interaction Handlers
    void HandlePanning(const ImVec2& canvas_pos, const ImVec2& canvas_size, GraphViewState& view_state); // Corrected typo
    void HandleZooming(const ImVec2& canvas_pos, const ImVec2& canvas_size, GraphViewState& view_state);
    void HandleNodeSelection(ImDrawList* draw_list, const ImVec2& canvas_pos, const ImVec2& canvas_size, GraphViewState& view_state);
    void DisplaySelectedNodeDetails(GraphViewState& view_state); // To be called by main GUI loop
    void HandleExpandCollapse(GraphNode& node, const ImVec2& canvas_pos, GraphViewState& view_state); // Handles button logic for expand/collapse
    void RenderPopups(ImDrawList* draw_list, const ImVec2& canvas_pos, GraphViewState& view_state); // Renders context menus and modals

    // Camera auto-pan functionality
    void StartAutoPanToNode(const std::shared_ptr<GraphNode>& target_node, const ImVec2& canvas_size);
    void StartAutoPanToPosition(const ImVec2& target_world_pos, float target_zoom, const ImVec2& canvas_size, GraphViewState& view_state);
    void UpdateAutoPan(float delta_time, GraphViewState& view_state);
    void CancelAutoPan(GraphViewState& view_state);
    bool IsAutoPanActive(const GraphViewState& view_state) const { return view_state.auto_pan_active; }

    ImVec2 WorldToScreen(const ImVec2& world_pos, const GraphViewState& view_state) const;
    ImVec2 ScreenToWorld(const ImVec2& screen_pos, const ImVec2& canvas_screen_pos, const GraphViewState& view_state) const;

    // Theme management
    void SetCurrentTheme(ThemeType theme) { current_theme_ = theme; }
    ThemeType GetCurrentTheme() const { return current_theme_; }

    // Culling helper
    bool IsNodeVisible(const GraphNode& node, const ImVec2& canvas_screen_pos, const ImVec2& canvas_size, const GraphViewState& view_state) const;

    // Theme-aware color getters
    ImU32 GetThemeNodeColor(ThemeType theme) const;
    ImU32 GetThemeNodeBorderColor(ThemeType theme) const;
    ImU32 GetThemeNodeSelectedBorderColor(ThemeType theme) const;
    ImU32 GetThemeEdgeColor(ThemeType theme) const;
    ImU32 GetThemeBackgroundColor(ThemeType theme) const;
    ImU32 GetThemeTextColor(ThemeType theme) const;
    ImU32 GetThemeExpandCollapseIconColor(ThemeType theme) const;

private:
    GraphManager* m_graph_manager; // Pointer to the graph manager
    // Map from unique graph_node_id to the corresponding graph node object.
    std::map<NodeIdType, std::shared_ptr<GraphNode>> nodes_;
    std::shared_ptr<GraphNode> context_node_ = nullptr; // Node for which context menu is triggered
    std::shared_ptr<GraphNode> reply_parent_node_ = nullptr; // Parent node for the new message
    ThemeType current_theme_ = ThemeType::DARK; // Current theme for color selection

    // Buffer for the new message input modal
    static char newMessageBuffer_[1024 * 16];

    // Methods for context menu and modal
    void RenderNodeContextMenu();
    void RenderNewMessageModal(ImDrawList* draw_list, const ImVec2& canvas_pos); // Pass draw_list and canvas_pos if needed for node creation logic

    void RenderNode(ImDrawList* draw_list, GraphNode& node, const GraphViewState& view_state);
    void RenderEdge(ImDrawList* draw_list, const GraphNode& parent_node, const GraphNode& child_node, const GraphViewState& view_state);
    void RenderBezierEdge(ImDrawList* draw_list, const GraphNode& parent_node, const GraphNode& child_node, const GraphViewState& view_state, bool is_alternative_path = false);
    // Recursive rendering helper for nodes and their children if expanded
    void RenderNodeRecursive(ImDrawList* draw_list, GraphNode& node, const ImVec2& canvas_screen_pos, const ImVec2& canvas_size, GraphViewState& view_state);
};

// Graph view rendering function that works with GraphManager
void RenderGraphView(GraphManager& graph_manager, ThemeType current_theme);

#endif // GRAPH_RENDERER_H