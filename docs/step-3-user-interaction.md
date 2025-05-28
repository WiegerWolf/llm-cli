# Step 3: User Interaction (Navigation & Basic Node Interaction) Plan

## 1. Graph View State
- [ ] Define a struct/class (e.g., `GraphViewState`) to hold:
    - [ ] `ImVec2 pan_offset` (initialized to `{0.0f, 0.0f}`)
    - [ ] `float zoom_scale` (initialized to `1.0f`)
    - [ ] `int selected_node_id` (initialized to `-1` or a suitable invalid marker, e.g., `std::numeric_limits<int>::max()`)
- [ ] Instantiate this state within the primary class or scope responsible for managing the graph visualization (e.g., `GraphEditor` or `MainGui`).

## 2. Panning
- [ ] Within the ImGui window or child window designated as the graph canvas:
    - [ ] Check if the canvas area is hovered: `if (ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem | ImGuiHoveredFlags_ChildWindows))`.
    - [ ] If hovered, check for middle mouse button drag: `if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle))`.
    - [ ] If dragging, update `GraphViewState::pan_offset` by `ImGui::GetIO().MouseDelta`.
        - `pan_offset.x += ImGui::GetIO().MouseDelta.x;`
        - `pan_offset.y += ImGui::GetIO().MouseDelta.y;`
    - [ ] Apply `pan_offset` to all world-space coordinates before transforming them to screen-space during the rendering phase. (e.g., `ImVec2 screen_pos = (world_pos * zoom_scale) + pan_offset;`)

## 3. Zooming
- [ ] Within the ImGui window or child window designated as the graph canvas:
    - [ ] Check if the canvas area is hovered: `if (ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem | ImGuiHoveredFlags_ChildWindows))`.
    - [ ] If hovered, get mouse wheel input: `float wheel = ImGui::GetIO().MouseWheel;`.
    - [ ] If `wheel != 0.0f`:
        - [ ] Define a zoom sensitivity factor (e.g., `const float zoom_sensitivity = 0.1f;`).
        - [ ] Calculate the zoom multiplier: `float zoom_factor = 1.0f + wheel * zoom_sensitivity;`.
        - [ ] Get the absolute screen position of the canvas's top-left corner (let this be `canvas_screen_pos`). This might be `ImGui::GetWindowPos() + ImGui::GetWindowContentRegionMin()` for a child window, or `ImGui::GetItemRectMin()` if the canvas is an item.
        - [ ] Get current mouse position in screen coordinates: `ImVec2 mouse_pos_screen = ImGui::GetMousePos();`.
        - [ ] Calculate mouse position relative to the canvas: `ImVec2 mouse_pos_in_canvas = mouse_pos_screen - canvas_screen_pos;`.
        - [ ] Update `pan_offset` to implement zoom towards the mouse cursor:
            `pan_offset = (pan_offset - mouse_pos_in_canvas) * zoom_factor + mouse_pos_in_canvas;`
        - [ ] Update `GraphViewState::zoom_scale *= zoom_factor;`.
        - [ ] Optionally, clamp `zoom_scale` to a predefined min/max range (e.g., `zoom_scale = std::max(0.1f, std::min(zoom_scale, 10.0f));`).
    - [ ] Apply `zoom_scale` during rendering to node sizes, positions, text sizes (if dynamic), and edge coordinates, ensuring scaling occurs relative to the `pan_offset`. (e.g., `ImVec2 screen_pos = (world_pos * zoom_scale) + pan_offset;`, `ImVec2 screen_size = world_size * zoom_scale;`)

## 4. Node Selection
- [ ] During the node rendering loop (for each visible node `n`):
    - [ ] Calculate the node's on-screen position and size:
        - [ ] `ImVec2 node_screen_pos = (n.position * zoom_scale) + pan_offset;`
        - [ ] `ImVec2 node_screen_size = n.size * zoom_scale;` // Assuming `n.size` is in world units
    - [ ] Create a unique ID for the clickable area, e.g., `char btn_id[64]; sprintf(btn_id, "node_%d", n.id);`.
    - [ ] Position an invisible button over the node's area:
        - [ ] `ImGui::SetCursorScreenPos(node_screen_pos);`
        - [ ] `if (ImGui::InvisibleButton(btn_id, node_screen_size))`:
            - [ ] If a different node was previously selected (`GraphViewState::selected_node_id != n.id` and `GraphViewState::selected_node_id != -1`):
                - [ ] Find the previously selected node in your graph data structure and set its `is_selected = false`.
            - [ ] Set current `n.is_selected = true`.
            - [ ] Update `GraphViewState::selected_node_id = n.id`.
    - [ ] (Alternative for deselection): If a click occurs on the canvas background (not on any node), deselect the currently selected node:
        - [ ] `if (ImGui::IsWindowClicked(ImGuiMouseButton_Left) && !ImGui::IsAnyItemHovered() && GraphViewState::selected_node_id != -1)`:
            - [ ] Find node by `GraphViewState::selected_node_id` and set `is_selected = false`.
            - [ ] `GraphViewState::selected_node_id = -1;`
- [ ] Ensure `GraphNode::is_selected` flags are consistently managed.

## 5. Display Full Message Content
- [ ] **Chosen Method:** A separate, resizable ImGui child window or a dedicated section/panel within the main UI, titled "Node Details" or similar.
    - [ ] **Rationale:** Provides a stable and predictable area for potentially large content, avoids graph occlusion, allows for scrolling, and is a common UI pattern for detail views.
- [ ] In the UI update function, after the graph canvas rendering:
    - [ ] `ImGui::Begin("Node Details");` (Or `ImGui::BeginChild` for an embedded panel).
    - [ ] `if (GraphViewState::selected_node_id != -1)`:
        - [ ] Retrieve the `GraphNode` (e.g., `selected_node_ptr`) corresponding to `GraphViewState::selected_node_id` from your graph data.
        - [ ] `if (selected_node_ptr)`:
            - [ ] Display Node ID: `ImGui::Text("ID: %d", selected_node_ptr->id);`
            - [ ] Display full message content: `ImGui::TextWrapped("Content: %s", selected_node_ptr->message_data.content.c_str());`
            - [ ] (Optional) Display other relevant information from `message_data` (e.g., sender, timestamp).
        - [ ] `else`:
            - [ ] `ImGui::TextWrapped("Error: Selected node data not found.");`
    - [ ] `else`:
        - [ ] `ImGui::TextWrapped("Select a node to view its details.");`
    - [ ] `ImGui::End();` (Or `ImGui::EndChild`).

## 6. Expand/Collapse Nodes
- [ ] During node rendering (e.g., within the function that renders a single `GraphNode`):
    - [ ] `if (!node.children_ids.empty())`: (Or check a `has_children` flag if available).
        - [ ] Define size and relative position for the expand/collapse icon (e.g., a small square or circle on the node's border).
            - `ImVec2 icon_size(10.0f * zoom_scale, 10.0f * zoom_scale);` // Ensure icon scales with zoom
            - `ImVec2 icon_local_pos(node.size.x - icon_size.x * 0.5f / zoom_scale - 2.0f, node.size.y * 0.5f - icon_size.y * 0.5f / zoom_scale);` // Example: right edge, centered vertically, in node's local space
            - `ImVec2 icon_screen_pos = (node.position + icon_local_pos) * zoom_scale + pan_offset;`
        - [ ] `ImGui::SetCursorScreenPos(icon_screen_pos);`
        - [ ] Render a clickable element for the icon:
            - [ ] `const char* icon_label = node.is_expanded ? "[-]" : "[+]";`
            - [ ] Create a unique ID for the button: `char exp_btn_id[64]; sprintf(exp_btn_id, "expcol_%d", node.id);`
            - [ ] `if (ImGui::Button(exp_btn_id, icon_size))`: // Or `ImGui::SmallButton`
                - [ ] `node.is_expanded = !node.is_expanded;`
                - [ ] (Flag that the graph layout may need to be recomputed or updated).
        - [ ] (Alternative) Draw the icon manually and check for clicks in its bounding box.
- [ ] The graph layout algorithm and rendering loop must:
    - [ ] Only calculate layout for and render child nodes if their parent `node.is_expanded` is `true`.
    - [ ] Adjust layout dynamically if expanding/collapsing nodes changes the overall graph structure, or mark the layout as "dirty" to be recalculated.