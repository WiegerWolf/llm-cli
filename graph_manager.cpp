#include "graph_manager.h"
#include "imgui.h" // For ImVec2, ImU32, IM_COL32
#include <memory> // For std::make_unique, std::move

// Implementation of PopulateGraphFromHistory
void GraphManager::PopulateGraphFromHistory(const std::vector<HistoryMessage>& history_messages) {
    // Clear Existing Graph Data
    all_nodes.clear(); // Deallocates nodes managed by std::unique_ptr
    root_node = nullptr;
    node_map.clear();

    GraphNode* previous_node_ptr = nullptr;

    for (const auto& msg : history_messages) {
        // Create a new node
        // The GraphNode constructor now takes (int id, const HistoryMessage& msg_data)
        // So we pass msg.message_id and the msg object itself.
        auto new_node_unique_ptr = std::make_unique<GraphNode>(msg.message_id, msg);
        GraphNode* current_node_ptr = new_node_unique_ptr.get();

        // message_id and message_data are now set by the GraphNode constructor.
        // current_node_ptr->message_id = msg.message_id; // Already done by constructor
        // current_node_ptr->message_data = msg; // Already done by constructor

        // Set parent and update children
        current_node_ptr->parent = previous_node_ptr;
        if (previous_node_ptr != nullptr) {
            previous_node_ptr->children.push_back(current_node_ptr);
            current_node_ptr->depth = previous_node_ptr->depth + 1;
        } else {
            // This is the first node, so it's the root
            root_node = current_node_ptr;
            current_node_ptr->depth = 0;
        }

        // Set initial visual properties (placeholders)
        current_node_ptr->position = ImVec2(50.0f, static_cast<float>(current_node_ptr->depth) * 120.0f);
        current_node_ptr->size = ImVec2(200.0f, 80.0f); // Adjusted height slightly

        // Set state flags (already set by constructor defaults, but can be explicit)
        // current_node_ptr->is_expanded = true; // Default from constructor
        // current_node_ptr->is_selected = false; // Default from constructor
        // current_node_ptr->color = IM_COL32(200, 200, 200, 255); // Default from constructor

        // Add the new node to the manager's storage
        all_nodes.push_back(std::move(new_node_unique_ptr));
        
        // If using node_map for quick lookup by message_id
        node_map[current_node_ptr->message_id] = current_node_ptr;

        // Update previous_node_ptr for the next iteration
        previous_node_ptr = current_node_ptr;
    }

    // Future Improvement Note:
    // This linear chain is the simplest initial structure. Future enhancements
    // could involve more complex heuristics to infer branches if HistoryMessage
    // contains reply-to IDs or similar contextual clues, but this is out of
    // scope for the initial population.
}

// Placeholder for RenderGraphView - to be implemented in graph_renderer.cpp or similar
// This function is declared in graph_manager.h but defined elsewhere.
// For now, we can provide a minimal definition if it's not going to be in a separate file immediately,
// or ensure it's correctly linked if defined in graph_renderer.cpp.
// The plan suggests its full implementation is later.
// If graph_renderer.cpp is intended to hold this, this stub isn't strictly needed here.
// void RenderGraphView(GraphManager& graph_manager) {
//     // Minimal placeholder:
//     // ImGui::Text("Graph View Placeholder. %zu nodes.", graph_manager.all_nodes.size());
//     // Actual rendering logic will go into graph_renderer.cpp
// }