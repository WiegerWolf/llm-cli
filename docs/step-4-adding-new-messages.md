# Step 4: User Interaction (Adding New Messages) Plan

This document outlines the implementation plan for allowing users to add new messages to the graph, creating new nodes and branches.

## 1. Node Context Menu

The context menu will allow users to initiate actions on a specific graph node, such as adding a reply.

- [ ] **Triggering the Context Menu:**
    - [ ] In the graph node rendering loop, after rendering the visual representation of a node or an [`ImGui::InvisibleButton`](https://github.com/ocornut/imgui/blob/master/imgui.h:1337) spanning the node area:
        - [ ] Call [`ImGui::OpenPopupOnItemClick("NodeContextMenu", ImGuiPopupFlags_MouseButtonRight)`](https://github.com/ocornut/imgui/blob/master/imgui.cpp:11000) to trigger the popup when a node is right-clicked. This function should be called if [`ImGui::IsItemHovered()`](https://github.com/ocornut/imgui/blob/master/imgui.h:1405) is true and a right-click is detected. Alternatively, [`ImGui::BeginPopupContextItem()`](https://github.com/ocornut/imgui/blob/master/imgui.cpp:10969) can be used directly after the node's main interactive element.
- [ ] **Defining the Context Menu Popup:**
    - [ ] `if (ImGui::BeginPopup("NodeContextMenu"))`
        - [ ] Add a menu item for replying: `if (ImGui::MenuItem("Reply from here"))`
            - [ ] Store the ID or pointer of the source `GraphNode` (the node that was right-clicked). This is crucial for linking the new message correctly.
            - [ ] Set a flag or state variable to indicate that the "New Message Input" mechanism should be activated.
            - [ ] Trigger the new message input mechanism (e.g., open the modal dialog, see Section 2).
        - [ ] (Optional) Add other relevant menu items in the future (e.g., "Edit Message", "Delete Node").
        - [ ] `ImGui::EndPopup();`

## 2. New Message Input Mechanism

A modal dialog is recommended for a focused message input experience.

- [ ] **Chosen Method:** Use a modal dialog for new message input.
- [ ] **Triggering the Modal Dialog:**
    - [ ] When "Reply from here" is selected from the context menu:
        - [ ] Call [`ImGui::OpenPopup("NewMessageModal")`](https://github.com/ocornut/imgui/blob/master/imgui.cpp:10850) to open the modal.
        - [ ] Ensure the ID of the parent `GraphNode` (source node) is passed to or accessible by the modal dialog's logic.
- [ ] **Defining the Modal Dialog:**
    - [ ] `if (ImGui::BeginPopupModal("NewMessageModal", NULL, ImGuiWindowFlags_AlwaysAutoResize))`
        - [ ] Display the parent node's message content (or a snippet) for context, if desired.
        - [ ] Add a multi-line text input field for the new message: `static char newMessageBuffer[1024*16] = ""; ImGui::InputTextMultiline("##NewMessageInput", newMessageBuffer, sizeof(newMessageBuffer), ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 8));`
        - [ ] Add a "Submit" button: `if (ImGui::Button("Submit"))`
            - [ ] Retrieve the text from `newMessageBuffer`.
            - [ ] Validate that the message is not empty.
            - [ ] Proceed to create the new `HistoryMessage` and `GraphNode` (see Section 3).
            - [ ] Clear `newMessageBuffer`.
            - [ ] [`ImGui::CloseCurrentPopup()`](https://github.com/ocornut/imgui/blob/master/imgui.cpp:10908);
        - [ ] `ImGui::SameLine();`
        - [ ] Add a "Cancel" button: `if (ImGui::Button("Cancel"))`
            - [ ] Clear `newMessageBuffer`.
            - [ ] [`ImGui::CloseCurrentPopup()`](https://github.com/ocornut/imgui/blob/master/imgui.cpp:10908);
        - [ ] `ImGui::EndPopup();`

## 3. Creating and Adding New Node

This section details the logic for creating the data structures for the new message and integrating it into the graph.

- [ ] **On "Submit" from the New Message Input Modal:**
    - [ ] **Create `HistoryMessage` (or equivalent data structure):**
        - [ ] Instantiate a new `HistoryMessage` object.
        - [ ] Populate its content (e.g., `text`, `role`) from the `newMessageBuffer`.
        - [ ] Assign a new unique ID to the `HistoryMessage` (e.g., using a UUID generator or an incrementing counter).
        - [ ] Set other relevant fields:
            - [ ] `timestamp` (current time).
            - [ ] `message_type` (e.g., `USER_REPLY`).
            - [ ] `model_id` (if applicable, could be inherited or null).
            - [ ] `parent_id` (ID of the message in the source node).
        - [ ] Add this new `HistoryMessage` to the global message list/store (e.g., `std::vector<HistoryMessage>` in [`main_gui.cpp`](main_gui.cpp:0) or a dedicated data manager).
    - [ ] **Create `GraphNode`:**
        - [ ] Instantiate a new `GraphNode` object.
        - [ ] Set its `message_id` to the ID of the newly created `HistoryMessage`.
        - [ ] Copy or link the `HistoryMessage` data to the node's `message_data` field.
        - [ ] Set the `parent` pointer/ID to the source `GraphNode` (the node from which "Reply from here" was triggered).
        - [ ] **Initialize Visual Properties:**
            - [ ] `position`: Calculate an initial position. A simple strategy is to place it below its parent, offset by a certain amount (e.g., `parent->position + ImVec2(0, parent->size.y + 50.0f)`). More sophisticated layout adjustments can be handled later.
            - [ ] `size`: Can be a default size initially, or calculated based on the message content after rendering.
        - [ ] **Initialize State Flags:**
            - [ ] `is_expanded = true` (or `false`, depending on desired default behavior).
            - [ ] `is_selected = false`.
        - [ ] Set `depth`: `parent_node->depth + 1`.
    - [ ] **Update Graph Structure:**
        - [ ] Add the new `GraphNode` to the `children` vector (or list) of its parent `GraphNode`.
        - [ ] Add the new `GraphNode` to the global list or map of all graph nodes (e.g., `std::vector<GraphNode*>` or `std::unordered_map<NodeId, GraphNode>`).
    - [ ] **Trigger Layout/Refresh:**
        - [ ] (Conceptual) The graph layout might need a partial or full recalculation to accommodate the new node and prevent overlaps. This might involve a separate layout algorithm pass.
        - [ ] Ensure the graph view is marked as needing a redraw to display the new node and the edge connecting it to its parent.

## 4. State Management

Proper state management is essential for the UI to behave correctly.

- [ ] **Node Context Menu State:**
    - [ ] Store the ID of the node for which the context menu is currently active (or about to be opened). This helps in associating actions from the menu with the correct node. This is often implicitly handled by ImGui if using `BeginPopupContextItem`.
- [ ] **New Message Modal State:**
    - [ ] A boolean flag (e.g., `showNewMessageModal`) to control the visibility of the modal, managed by `OpenPopup` and `CloseCurrentPopup`.
    - [ ] Store the parent node ID to which the new message will be a reply. This should be set when "Reply from here" is clicked.
- [ ] **Input Buffer:**
    - [ ] The `static char newMessageBuffer` inside the modal dialog scope will hold the text. Ensure it's cleared after submission or cancellation.
- [ ] **General Graph State:**
    - [ ] Ensure that adding a node correctly updates any global graph state variables (e.g., node counts, list of active nodes).

## 5. Edge Creation (Implicit)

- [ ] When a new node is added as a child to a parent node:
    - [ ] An edge (link) is implicitly created between the parent and the new child.
    - [ ] The rendering logic for edges should automatically pick up this new parent-child relationship and draw the connecting line during the next graph render pass.