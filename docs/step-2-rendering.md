# Step 2: Rendering Implementation Plan

## 1. Node Rendering Function (`RenderGraphNode`)
- [ ] Define a function `void RenderGraphNode(ImDrawList* draw_list, const GraphNode& node, const ImVec2& view_offset)` in a suitable C++ file (e.g., `graph_renderer.cpp`).
- [ ] **Node Body:**
    - [ ] Calculate screen position: `node.position + view_offset`.
    - [ ] Draw a filled rectangle for the node background using `draw_list->AddRectFilled()`. Use `node.size` for dimensions.
    - [ ] Draw a border for the node using `draw_list->AddRect()`.
    - [ ] Adjust border color/thickness if `node.is_selected` (e.g., brighter color or thicker line).
- [ ] **Node Content:**
    - [ ] Calculate text position within the node.
    - [ ] Display a truncated version of `node.message_data.content` (or a summary) within the node boundaries using `draw_list->AddText()`.
    - [ ] Use `ImGui::CalcTextSize()` to help with text layout and truncation if necessary.
    - [ ] Consider `ImGui::PushClipRect()` to ensure text does not overflow node boundaries.
- [ ] **Expansion Indicator (if applicable):**
    - [ ] If `node.children` is not empty (or `node.has_children` flag is set), draw a small visual indicator (e.g., a '+' or '-' symbol, or a small triangle) to show if the node is expanded (`node.is_expanded`) or collapsed. This could be based on `node.is_expanded`.

## 2. Edge Rendering Function (`RenderEdge`)
- [ ] Define a function `void RenderEdge(ImDrawList* draw_list, const GraphNode& parent_node, const GraphNode& child_node, const ImVec2& view_offset)` in the same file (e.g., `graph_renderer.cpp`).
- [ ] **Calculate Connection Points:**
    - [ ] Determine start point on `parent_node`. For a top-down layout, this might be the center of the bottom edge of `parent_node.position` and `parent_node.size`.
    - [ ] Determine end point on `child_node`. For a top-down layout, this might be the center of the top edge of `child_node.position` and `child_node.size`.
    - [ ] Adjust calculated points by `view_offset`: `start_point + view_offset`, `end_point + view_offset`.
- [ ] **Draw Line:**
    - [ ] Use `draw_list->AddLine()` to draw a straight line between the calculated and offset screen points.
    - [ ] (Optional Placeholder for Future) Note that `draw_list->AddBezierCubic()` or `draw_list->AddBezierQuadratic()` could be used later for curved edges if desired.

## 3. Graph View Rendering Logic
- [ ] In the main GUI loop (likely within `main_gui.cpp` or a dedicated function called from there, perhaps in a new `GraphView::Render()` method):
    - [ ] Begin an ImGui window or child window that will serve as the canvas for the graph.
    - [ ] Obtain the `ImDrawList` for this window using `ImGui::GetWindowDrawList()`.
    - [ ] **Iterate and Render:**
        - [ ] Iterate through all `GraphNode`s in the graph data structure (e.g., a `std::vector<GraphNode>` or a map).
        - [ ] For each node, call `RenderGraphNode(draw_list, node, view_offset)`.
        - [ ] After rendering all nodes (or before, depending on desired Z-order), iterate through nodes again. For each node that is a parent, iterate through its `children` (or use the `edges` list if that's how connections are stored).
        - [ ] For each parent-child connection (or edge), call `RenderEdge(draw_list, parent_node, child_node, view_offset)`.
    - [ ] **Rendering Order:**
        - [ ] Decide on rendering order: e.g., render all edges first, then all nodes on top to ensure nodes are not obscured by edge lines.
    - [ ] **Panning and Zooming (Initial Thoughts for Panning):**
        - [ ] Implement basic panning by maintaining a `ImVec2 view_offset`.
        - [ ] Modify `view_offset` based on user input (e.g., mouse drag on the canvas background).
        - [ ] Pass this `view_offset` to `RenderGraphNode` and `RenderEdge` to correctly position elements.
    - [ ] **Clipping:**
        - [ ] Ensure that drawing is clipped to the bounds of the graph view window/canvas. `ImDrawList` usually handles this for items added within a window context.

## 4. Initial Layout Consideration
- [ ] Node positions (`GraphNode::position`) and sizes (`GraphNode::size`) are assumed to be pre-calculated by a separate layout algorithm.
- [ ] The rendering functions will consume these `position` and `size` values.
- [ ] The details of the layout algorithm itself (e.g., simple tree layout) will be planned and implemented in a subsequent step.

## 5. File Structure
- [ ] Plan to create `graph_renderer.h` to declare `RenderGraphNode`, `RenderEdge`, and any related helper functions or structures.
- [ ] Plan to create `graph_renderer.cpp` to implement these functions.
- [ ] These files will encapsulate the graph rendering logic.