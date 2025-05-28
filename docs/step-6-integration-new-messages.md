# Step 6: Integration (Handling New Messages in Graph) Plan

This document outlines the plan for integrating new messages, whether from user input or worker threads, into the graph data structure.

## 1. Hooking into New Message Arrival

- [ ] **Identify Code Location:** Pinpoint the exact place in the existing codebase (likely within [`main_gui.cpp`](main_gui.cpp:0) or a callback mechanism associated with `GuiInterface`) where new `HistoryMessage` objects are processed and added to the primary `std::vector<HistoryMessage>` (e.g., `g_chat_history_vector`).
- [ ] **Introduce Graph Update Call:** Immediately after a new `HistoryMessage` is successfully added to this vector, insert a call to a new function responsible for adding this message to the graph. This function could be named, for example, `GraphManager::HandleNewHistoryMessage(const HistoryMessage& new_msg)`.

## 2. `GraphManager::HandleNewHistoryMessage` Function Logic

This function will be responsible for creating a corresponding `GraphNode` and integrating it into the graph.

- [ ] **Function Signature:** Define the function, e.g., `void GraphManager::HandleNewHistoryMessage(const HistoryMessage& new_msg)` (assuming `GraphManager` holds graph data and view state, or can access them).
- [ ] **Determine Parent Node:**
    - [ ] Initialize `GraphNode* parent_node = nullptr;`.
    - [ ] **Check for Selected Node:** Access the current graph view state (e.g., `graph_view_state.selected_node_id`). If `selected_node_id` is valid and corresponds to an existing node in `graph_data.all_nodes`, retrieve its `GraphNode*` and assign it to `parent_node`.
    - [ ] **Fallback Strategy (if no node is selected or selection is invalid):**
        - [ ] **Primary Fallback: Use Last Added Node:**
            - [ ] Maintain a `GraphNode* last_node_added_to_graph` pointer within the `GraphManager` or `YourGraphDataType`. This pointer should be updated every time a node is added to the graph, regardless of the method.
            - [ ] If `parent_node` is still `nullptr` and `last_node_added_to_graph` is not `nullptr` and is a valid node in the graph, set `parent_node = last_node_added_to_graph`.
        - [ ] **Secondary Fallback (if no nodes exist or `last_node_added_to_graph` is null): New Root Node:**
            - [ ] If `parent_node` remains `nullptr` (e.g., this is the very first message being added, or the graph was cleared and `last_node_added_to_graph` is null), the new node will become a root node.
- [ ] **Create and Link New `GraphNode`:**
    - [ ] Create a new graph node: `GraphNode* new_graph_node = new GraphNode();` (or use `std::make_unique` if managing with smart pointers: `auto new_graph_node = std::make_unique<GraphNode>();`).
    - [ ] **Populate Node Data:**
        - [ ] `new_graph_node->message_id = new_msg.id;` (or a new unique ID if `HistoryMessage::id` is not suitable for graph node IDs).
        - [ ] `new_graph_node->message_data = new_msg;` (or copy relevant fields if `HistoryMessage` structure is not directly stored).
        - [ ] `new_graph_node->label = DetermineNodeLabel(new_msg);` (e.g., a summary or type of message).
    - [ ] **Establish Parent-Child Relationship and Depth:**
        - [ ] Set `new_graph_node->parent = parent_node;`.
        - [ ] If `parent_node` is not `nullptr`:
            - [ ] Add `new_graph_node` (or its ID/pointer) to `parent_node->children`.
            - [ ] `new_graph_node->depth = parent_node->depth + 1;`.
        - [ ] Else (new node is a root):
            - [ ] `new_graph_node->depth = 0;`.
            - [ ] Add `new_graph_node` (or its ID/pointer) to the graph's list of root nodes (e.g., `graph_data.root_nodes`).
    - [ ] **Set Initial Node Properties:**
        - [ ] `new_graph_node->position`: Determine an initial position.
            - [ ] If it's a child, position it relative to `parent_node` (e.g., below it, offset).
            - [ ] If it's a new root, place it at a default starting position for new roots.
            - [ ] This position might be temporary until a layout algorithm runs.
        - [ ] `new_graph_node->size`: Set a default size (e.g., based on content or a standard size).
        - [ ] `new_graph_node->is_expanded = true;` (or `false`, depending on desired default behavior).
        - [ ] `new_graph_node->is_selected = false;` (newly added nodes are typically not selected by default).
        - [ ] `new_graph_node->color`: Set a default color, possibly based on message type or source.
    - [ ] **Add to Global Graph Data:**
        - [ ] Add the `new_graph_node` (or its `std::unique_ptr`) to the main collection of all nodes (e.g., `graph_data.all_nodes[new_graph_node->id] = std::move(new_graph_node);` or `graph_data.all_nodes_vector.push_back(new_graph_node_ptr);`).
    - [ ] **Update `last_node_added_to_graph`:**
        - [ ] `last_node_added_to_graph = new_graph_node;` (or the raw pointer if using smart pointers for storage).
- [ ] **Trigger Graph View Update and Layout:**
    - [ ] **Mark for Layout Update:** Signal that the graph structure has changed and a layout recalculation might be needed (e.g., set a flag `graph_layout_dirty = true;`).
    - [ ] **Request Redraw:** If the graph view is currently active and visible, ensure it is flagged for redrawing in the next frame to display the new node and any new edges.

## 3. State Management Considerations

- [ ] **`selected_node_id`:** Ensure that the mechanism for selecting nodes in the graph view correctly updates `graph_view_state.selected_node_id` and that this ID is reliably accessible by the `HandleNewHistoryMessage` logic.
- [ ] **`last_node_added_to_graph`:**
    - [ ] This pointer/ID must be consistently updated whenever any node is added to the graph, whether through this automated process, user interaction (context menu), or initial population.
    - [ ] Consider edge cases: What happens if `last_node_added_to_graph` points to a node that is subsequently deleted? It should ideally be nulled out or updated to a sensible alternative if the "last added" concept is critical. For this specific use case (parenting new messages), if it's null, the fallback to root or other strategies will apply.
- [ ] **Node ID Uniqueness:** Ensure that `GraphNode::message_id` (or whatever ID is used for graph nodes) is unique across all nodes in the graph. If `HistoryMessage::id` can have duplicates or is not persistent, a separate unique ID generation mechanism for graph nodes will be required.

## 4. Data Structures (Assumed)

This plan assumes data structures similar to those outlined in previous steps:

- [ ] **`HistoryMessage`:** The existing structure for messages (e.g., `struct HistoryMessage { int id; std::string content; /* ...other fields... */ };`).
- [ ] **`GraphNode`:** Structure for graph nodes (e.g., `struct GraphNode { NodeIdType id; MessageIdType message_id; HistoryMessage message_data; GraphNode* parent; std::vector<GraphNode*> children; int depth; ImVec2 position; ImVec2 size; bool is_expanded; bool is_selected; ImU32 color; std::string label; /* ...other state... */ };`).
- [ ] **`YourGraphDataType` (or `GraphManager` member):** Holds `all_nodes` (e.g., `std::map<NodeIdType, std::unique_ptr<GraphNode>>`), `root_nodes`, and `last_node_added_to_graph`.
- [ ] **`GraphViewState` (or `GraphManager` member):** Holds `selected_node_id`, zoom/pan info, etc.

This checklist provides a detailed plan for integrating new messages into the graph system programmatically.