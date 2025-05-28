# Step 1: Data Structure Implementation Plan

This document outlines the plan for defining the necessary data structures for the graph-based dialog interface.

## 1. Update `HistoryMessage` Structure

The existing `HistoryMessage` struct needs a unique identifier to link it with `GraphNode` instances.

- [ ] **Modify `HistoryMessage` in `gui_interface.h`:**
    - [ ] Add a unique identifier field. A simple integer ID is recommended for now.
      ```c++
      // In struct HistoryMessage (gui_interface.h)
      int message_id; // Or a suitable unique ID type like std::string for UUIDs
      // MessageType type;
      // std::string content;
      // std::optional<std::string> model_id;
      ```
    - [ ] **Plan ID Generation:** Determine and document the strategy for assigning `message_id` values.
        - Consider if existing messages (if any are persisted and loaded) need migration or if IDs are only for new messages.
        - For new messages, a simple incrementing counter managed by the message creation logic could suffice initially.

## 2. Define `GraphNode` Structure

A new structure, `GraphNode`, will represent each message in the graph.

- [ ] **Create a new header file for graph-related types:**
    - [ ] Create `graph_types.h` (e.g., in a `graph` subdirectory or alongside `gui_interface.h`).
    - [ ] This file will house the `GraphNode` definition and any other graph-specific types in the future.

- [ ] **Define the `GraphNode` struct in `graph_types.h`:**
    - [ ] Include necessary headers:
      ```c++
      #pragma once

      #include <vector>
      #include <string> // Potentially, if HistoryMessage::message_id becomes std::string
      #include "imgui.h" // For ImVec2
      #include "gui_interface/gui_interface.h" // For HistoryMessage

      // Forward declare GraphNode if parent/children pointers cause issues with direct include
      // struct GraphNode; // Likely not needed if definition is self-contained here
      ```
    - [ ] Define the `GraphNode` struct with the following members:
      ```c++
      struct GraphNode {
          // Core Data
          int message_id;                 // Links to HistoryMessage::message_id
          HistoryMessage message_data;    // A copy of the message content and metadata

          // Visual Properties
          ImVec2 position;                // On-screen position for rendering
          ImVec2 size;                    // Calculated size of the node for layout and rendering

          // State Flags
          bool is_expanded;               // If the node's children/alternatives are visible
          bool is_selected;               // If the node is currently selected by the user

          // Relational Pointers for Graph Structure
          GraphNode* parent;                       // Pointer to the parent node in the primary branch
          std::vector<GraphNode*> children;          // Pointers to direct children in the primary branch
          std::vector<GraphNode*> alternative_paths; // Pointers to nodes representing alternative paths/branches from this message

          // Layout Helper
          int depth;                       // Depth in the graph, useful for layout algorithms
          
          // Constructor (optional, but good practice for initialization)
          GraphNode(int id, const HistoryMessage& msg_data)
              : message_id(id), message_data(msg_data),
                position(ImVec2(0,0)), size(ImVec2(0,0)), // Default visual properties
                is_expanded(true), is_selected(false),   // Default states
                parent(nullptr), depth(0) {}             // Default relational/layout properties
      };
      ```

## 3. Considerations for Integration (To be addressed in later planning stages)

While this plan focuses on data structure definition, keep in mind these future aspects:
- **Memory Management:** `GraphNode` instances will likely be dynamically allocated. Plan for their creation, ownership (e.g., using `std::unique_ptr` or a central graph manager), and deletion to avoid memory leaks.
- **Graph Management:** Decide where the collection of `GraphNode`s (e.g., `std::vector<std::unique_ptr<GraphNode>>` or `std::map<int, GraphNode*>`) will reside and how it will be managed. This might be a new class or part of an existing UI management component.
- **Synchronization:** If `HistoryMessage` instances in the old `std::vector<HistoryMessage>` and `GraphNode::message_data` need to be kept in sync, plan the mechanism. Alternatively, the graph might become the primary source of truth for display.

This checklist provides the foundational steps for implementing the data structures. Subsequent planning will cover their integration and usage.