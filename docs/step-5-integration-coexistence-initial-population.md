# Step 5: Integration (Coexistence & Initial Population) Plan

## 1. UI for View Coexistence
- [ ] **Chosen Method:** Use ImGui Tabs. This provides a clear and standard way for users to switch between the linear message view and the new graph view.
- [ ] In the main UI rendering loop (likely within `RenderMainGUIDetails()` or a similar function in [`main_gui.cpp`](main_gui.cpp:0)):
    - [ ] Implement `if (ImGui::BeginTabBar("ViewModeTabBar", ImGuiTabBarFlags_None))` to create the tab bar.
    - [ ] Add a tab for the "Linear View":
        - [ ] `if (ImGui::BeginTabItem("Linear View"))`
        - [ ] Place the existing code responsible for rendering the linear message history here.
        - [ ] `ImGui::EndTabItem();`
    - [ ] Add a tab for the "Graph View":
        - [ ] `if (ImGui::BeginTabItem("Graph View"))`
        - [ ] Call the main rendering function for the graph interface (e.g., `RenderGraphView()`). This function will handle drawing nodes, edges, and user interactions within the graph.
        - [ ] The initial population of the graph (`PopulateGraphFromHistory`) should be triggered if the graph is empty and this tab is selected for the first time (or based on the chosen timing strategy).
        - [ ] `ImGui::EndTabItem();`
    - [ ] `ImGui::EndTabBar();`
- [ ] Manage active view state: ImGui tabs inherently manage which tab is active. No explicit separate state variable is immediately necessary for just switching views, but internal state for the graph view itself (e.g., zoom, pan) will be managed by the graph rendering/logic components.

## 2. Initial Graph Population Function
- [ ] Define a function, for example in a new `graph_manager.cpp` (or an existing relevant GUI logic file), with the signature:
    `void PopulateGraphFromHistory(const std::vector<HistoryMessage>& history_messages, GraphManager& graph_manager)`
    (Assuming `GraphManager` is a class that encapsulates `YourGraphDataType`).
    - `GraphManager` (or `YourGraphDataType` if a simpler struct is used initially) should contain:
        - `std::vector<std::unique_ptr<GraphNode>> all_nodes;` (owns all nodes)
        - `GraphNode* root_node = nullptr;` (or a `std::vector<GraphNode*> root_nodes;` if multiple roots are possible, though linear history implies one primary root initially).
        - Potentially an `std::unordered_map<MessageId, GraphNode*> node_map;` for quick lookup by `message_id`.
- [ ] **Clear Existing Graph Data:**
    - [ ] Inside `PopulateGraphFromHistory`, before populating:
        - [ ] `graph_manager.all_nodes.clear();` (this will also deallocate nodes if using `std::unique_ptr`).
        - [ ] `graph_manager.root_node = nullptr;`.
        - [ ] `graph_manager.node_map.clear();` (if using a map).
        - This ensures that re-populating (e.g., via a refresh button) starts with a clean slate.
- [ ] **Iterate and Create Nodes (Linear Chain Strategy):**
    - [ ] Initialize `GraphNode* previous_node_ptr = nullptr;`.
    - [ ] Loop through each `const HistoryMessage& msg` in `history_messages` (using an index if needed for the first message):
        - [ ] Create a new node: `auto new_node_unique_ptr = std::make_unique<GraphNode>(); GraphNode* current_node_ptr = new_node_unique_ptr.get();`
        - [ ] Populate `current_node_ptr->message_id` from `msg.id` (This assumes `HistoryMessage` has an `id` field as planned in `step-1-data-structure.md`. If `HistoryMessage` uses its index or another unique identifier, adapt accordingly).
        - [ ] Copy relevant data from `msg` to `current_node_ptr->message_data` (e.g., `current_node_ptr->message_data.text = msg.text; current_node_ptr->message_data.sender = msg.sender;` etc., based on `HistoryMessage` and `GraphNode::MessageContent` structure).
        - [ ] Set `current_node_ptr->parent = previous_node_ptr;`.
        - [ ] If `previous_node_ptr` is not null:
            - [ ] Add `current_node_ptr` to `previous_node_ptr->children.push_back(current_node_ptr);`.
            - [ ] Set `current_node_ptr->depth = previous_node_ptr->depth + 1;`.
        - [ ] Else (if `previous_node_ptr` is null, this is the first node):
            - [ ] This `current_node_ptr` is the root of the initial linear chain. Set `graph_manager.root_node = current_node_ptr;`.
            - [ ] Set `current_node_ptr->depth = 0;`.
        - [ ] Set initial `current_node_ptr->position` (placeholder, e.g., `ImVec2(50.0f, current_node_ptr->depth * 120.0f)`). This will be overridden by a layout algorithm.
        - [ ] Set initial `current_node_ptr->size` (placeholder, e.g., `ImVec2(200.0f, 60.0f)`). This might be dynamically calculated later based on content.
        - [ ] Set `current_node_ptr->is_expanded = true;` (default for linear chain, all visible).
        - [ ] Set `current_node_ptr->is_selected = false;`.
        - [ ] Set `current_node_ptr->color` to a default value.
        - [ ] Add the new node to the manager: `graph_manager.all_nodes.push_back(std::move(new_node_unique_ptr));`.
        - [ ] If using `node_map`: `graph_manager.node_map[current_node_ptr->message_id] = current_node_ptr;`.
        - [ ] Update `previous_node_ptr = current_node_ptr;` for the next iteration.
    - [ ] **Future Improvement Note:** Mention that this linear chain is the simplest initial structure. Future enhancements could involve more complex heuristics to infer branches if `HistoryMessage` contains reply-to IDs or similar contextual clues, but this is out of scope for the initial population.
- [ ] **Timing of Population:**
    - [ ] **Initial Call:** Call `PopulateGraphFromHistory` once when the "Graph View" tab is first activated, if `graph_manager.all_nodes` is empty. This avoids unnecessary processing if the user never opens the graph view.
    - [ ] **Refresh Mechanism:**
        - [ ] Implement a "Refresh Graph" button within the "Graph View" tab.
        - [ ] Clicking this button will call `PopulateGraphFromHistory` again, rebuilding the graph from the current `history_messages`. This is useful if the underlying history can change while the graph view is open or if a reset is desired.

## 3. Graph Data Management
- [ ] **Authoritative Storage:** The `GraphManager` class (or a similar dedicated structure) will be responsible for storing and managing all `GraphNode` objects.
    - [ ] The primary storage will be `std::vector<std::unique_ptr<GraphNode>> all_nodes;` within `GraphManager`. This ensures proper ownership and automatic memory management of nodes.
    - [ ] `GraphNode* root_node;` (or `std::vector<GraphNode*> root_nodes;`) will provide entry points for graph traversal.
    - [ ] An optional `std::unordered_map<MessageId, GraphNode*> node_map;` can be maintained for O(1) average time access to nodes by their ID.
- [ ] **Accessibility:**
    - [ ] An instance of `GraphManager` should be accessible to:
        - The graph rendering functions (to draw nodes and edges).
        - User interaction handling logic for the graph (to modify node states like selection, expansion, or to initiate actions like creating new replies).
        - Layout algorithms (to read node data and update their positions).
        - The `PopulateGraphFromHistory` function itself.
    - This might involve passing a reference or pointer to the `GraphManager` instance to these components, or making it a member of a main GUI state class.
- [ ] **Data Source Confirmation:**
    - [ ] Confirm that the `std::vector<HistoryMessage> history_messages;` is the correct and accessible data source. This is typically a member of the main application or GUI state class (e.g., in [`main_gui.cpp`](main_gui.cpp:0)). A const reference to this vector should be passed to `PopulateGraphFromHistory`.
    - [ ] Ensure `HistoryMessage` (from [`gui_interface.h`](gui_interface/gui_interface.h:0)) contains (or will be updated to contain, as per `step-1`) a unique `id` (e.g., `MessageId`) and the necessary content to populate `GraphNode::MessageContent`.