# Plan for Graph-Based Dialog Interface

## 1. Current System Analysis (Summary)

*   Messages are stored in a `std::vector<HistoryMessage>` in `main_gui.cpp`.
*   `HistoryMessage` (defined in `gui_interface.h`) holds message type, content, and an optional model ID.
*   Rendering is linear in an ImGui child window using `ImGui::TextWrapped()` and `ImGui::Selectable()`.
*   New messages are queued via `GuiInterface` and appended to the history vector for display.

## 2. Conceptual Plan for Graph Interface

### Data Structure
*   Introduce a `GraphNode` struct containing:
    *   `message_id` (linking to original `Message::id`)
    *   A copy of the `Message` data (`message_data`)
    *   Visual properties: `ImVec2 position`, `ImVec2 size`
    *   State flags: `bool is_expanded`, `bool is_selected`
    *   Relational pointers: `GraphNode* parent`, `std::vector<GraphNode*> children` (for primary branches), and `std::vector<GraphNode*> alternative_paths` (for non-primary explorations)
    *   Layout helper: `int depth`
*   The existing `Message` struct will not require immediate modification.

### Rendering
*   Use ImGui's custom drawing (`ImDrawList`) for nodes (rectangles) and edges (lines/Bezier curves).
*   Start with a simple tree-based layout algorithm (e.g., top-down or left-to-right).
*   Consider third-party ImGui node editor libraries for advanced features later, but prefer custom drawing initially.

### User Interaction
*   **Navigation:** Support panning (e.g., drag middle mouse button) and zooming (e.g., mouse wheel).
*   **Node Interaction:**
    *   Nodes selectable (e.g., by clicking).
    *   Display full message content for a selected node in a separate panel or detailed tooltip.
    *   Nodes with children will have an expand/collapse icon.
*   **Adding New Messages:** Allow adding new messages (creating new nodes/branches) from an existing node (e.g., via a context menu "Reply from here").

### Integration with Existing System
*   The new graph view will initially coexist with the current linear view (e.g., separate tab or switchable mode).
*   **Initial Population:** Populate the graph by traversing the current `std::vector<Message>`. Inferring the initial tree structure from linear history is a key consideration.
*   **New Messages:** Add new messages from user input or worker threads to the graph, typically as children of the active/selected node.

### Key Challenges and Considerations
*   **Layout Complexity:** ✅ **IMPLEMENTED** - Hybrid chronological-force layout algorithm provides robust and aesthetically pleasing layouts. See [Chronological Layout Documentation](chronological-layout-index.md).
*   **Rendering Performance:** Optimizing drawing for many nodes/edges (e.g., culling).
*   **State Management:** Managing visual state (position, expansion) in ImGui.
*   **Inferring Structure:** Defining how to represent initial linear history as a graph.
*   **User Experience:** ✅ **IMPLEMENTED** - Chronological layout ensures intuitive time-aware visualization with natural clustering.

### Implemented Features

The graph-based dialog interface now includes:

*   **Hybrid Chronological-Force Layout Algorithm**: Combines physics-based force simulation with chronological ordering constraints
*   **Configurable Parameters**: Tunable force strengths, temporal constraints, and animation settings
*   **Real-time Animation**: Smooth layout transitions and interactive positioning
*   **Multi-conversation Support**: Handles linear, branched, and complex conversation structures
*   **Performance Optimization**: Efficient convergence detection and parameter optimization for different scales

For detailed information, see:
- [Algorithm Overview](chronological-layout-algorithm.md)
- [User Guide](chronological-layout-user-guide.md)
- [API Reference](chronological-layout-api-reference.md)
- [Documentation Index](chronological-layout-index.md)