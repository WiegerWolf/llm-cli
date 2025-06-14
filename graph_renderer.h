#ifndef GRAPH_RENDERER_H
#define GRAPH_RENDERER_H

#include "extern/imgui/imgui.h"
#include "gui_interface/graph_types.h" // For GraphNode and GraphViewState
#include "gui_interface/gui_interface.h" // For ThemeType
#include <vector> // For storing GraphNode pointers or objects
#include <map>    // For managing nodes by ID
#include <memory> // For std::shared_ptr

// Forward declaration if GraphNode is complex and defined elsewhere,
// or include the necessary header if it's simple.
// Assuming GraphNode is defined in "gui_interface/graph_types.h"

// Forward declaration for GraphManager
class GraphManager;

class GraphEditor {
public:
    GraphEditor();

    void Render(ImDrawList* draw_list, const ImVec2& canvas_pos, const ImVec2& canvas_size);
    void AddNode(std::shared_ptr<GraphNode> node);
    std::shared_ptr<GraphNode> GetNode(int node_id);
    void ClearNodes(); // If nodes are managed internally

    // Interaction Handlers
    void HandlePanning(const ImVec2& canvas_pos, const ImVec2& canvas_size); // Corrected typo
    void HandleZooming(const ImVec2& canvas_pos, const ImVec2& canvas_size);
    void HandleNodeSelection(ImDrawList* draw_list, const ImVec2& canvas_pos, const ImVec2& canvas_size);
    void DisplaySelectedNodeDetails(); // To be called by main GUI loop
    void HandleExpandCollapse(GraphNode& node, const ImVec2& canvas_pos); // Handles button logic for expand/collapse
    void RenderPopups(ImDrawList* draw_list, const ImVec2& canvas_pos); // Renders context menus and modals

    // Camera auto-pan functionality
    void StartAutoPanToNode(const std::shared_ptr<GraphNode>& target_node, const ImVec2& canvas_size);
    void StartAutoPanToPosition(const ImVec2& target_world_pos, float target_zoom, const ImVec2& canvas_size);
    void UpdateAutoPan(float delta_time);
    void CancelAutoPan();
    bool IsAutoPanActive() const { return view_state_.auto_pan_active; }

    ImVec2 WorldToScreen(const ImVec2& world_pos) const;
    ImVec2 ScreenToWorld(const ImVec2& screen_pos, const ImVec2& canvas_screen_pos) const;

    GraphViewState& GetViewState() { return view_state_; } // Getter for view_state

    // Theme management
    void SetCurrentTheme(ThemeType theme) { current_theme_ = theme; }
    ThemeType GetCurrentTheme() const { return current_theme_; }

    // Culling helper
    bool IsNodeVisible(const GraphNode& node, const ImVec2& canvas_screen_pos, const ImVec2& canvas_size) const;

    // Theme-aware color getters
    ImU32 GetThemeNodeColor(ThemeType theme) const;
    ImU32 GetThemeNodeBorderColor(ThemeType theme) const;
    ImU32 GetThemeNodeSelectedBorderColor(ThemeType theme) const;
    ImU32 GetThemeEdgeColor(ThemeType theme) const;
    ImU32 GetThemeBackgroundColor(ThemeType theme) const;
    ImU32 GetThemeTextColor(ThemeType theme) const;
    ImU32 GetThemeExpandCollapseIconColor(ThemeType theme) const;

private:
    GraphViewState view_state_;
    std::map<int, std::shared_ptr<GraphNode>> nodes_;
    std::shared_ptr<GraphNode> context_node_ = nullptr; // Node for which context menu is triggered
    std::shared_ptr<GraphNode> reply_parent_node_ = nullptr; // Parent node for the new message
    ThemeType current_theme_ = ThemeType::DARK; // Current theme for color selection

    // Buffer for the new message input modal
    static char newMessageBuffer_[1024 * 16];

    // Methods for context menu and modal
    void RenderNodeContextMenu();
    void RenderNewMessageModal(ImDrawList* draw_list, const ImVec2& canvas_pos); // Pass draw_list and canvas_pos if needed for node creation logic

    void RenderNode(ImDrawList* draw_list, GraphNode& node);
    void RenderEdge(ImDrawList* draw_list, const GraphNode& parent_node, const GraphNode& child_node);
    void RenderBezierEdge(ImDrawList* draw_list, const GraphNode& parent_node, const GraphNode& child_node, bool is_alternative_path = false);
    // Recursive rendering helper for nodes and their children if expanded
    void RenderNodeRecursive(ImDrawList* draw_list, GraphNode& node, const ImVec2& canvas_screen_pos, const ImVec2& canvas_size);
};

// Graph view rendering function that works with GraphManager
void RenderGraphView(GraphManager& graph_manager, GraphViewState& view_state, ThemeType current_theme);

#endif // GRAPH_RENDERER_H