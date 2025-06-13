#include "graph_manager.h"
#include "graph_renderer.h" // For GraphEditor
#include "imgui.h" // For ImVec2, ImU32, IM_COL32
#include "id_types.h" // Defines NodeIdType and kInvalidNodeId sentinel
#include <memory> // For std::make_unique, std::move
#include <algorithm> // For std::find_if
#include <cmath> // For std::max, std::min
#include <cfloat> // For FLT_MAX

// Forward declaration of helper function from main_gui.cpp
extern std::string FormatMessageForGraph(const HistoryMessage& msg, PersistenceManager& db_manager);

// Thread-safe helper that safely computes text size even when no ImGui context is active.
static inline ImVec2 SafeCalcTextSize(const std::string& text, float wrap_width = FLT_MAX)
{
   if (ImGui::GetCurrentContext() != nullptr && ImGui::GetFont() != nullptr)
   {
       return ImGui::CalcTextSize(text.c_str(), nullptr, false, wrap_width);
   }
   constexpr float kFallbackCharW = 8.0f;
   constexpr float kFallbackCharH = 16.0f;
   return ImVec2(static_cast<float>(text.length()) * kFallbackCharW, kFallbackCharH);
}

// Helper function to calculate dynamic node size based on content
ImVec2 CalculateNodeSize(const std::string& content) {
    if (content.empty()) {
        return ImVec2(80.0f, 20.0f); // Minimal size for empty content (text-only)
    }
    
    // Calculate text size with minimal padding for text-only nodes
    float max_width = 400.0f; // Reasonable maximum width for text
    float min_width = 50.0f; // Minimal width - just enough for text
    float min_height = 20.0f; // Minimal height - just enough for text
    float padding = 10.0f; // Minimal padding (5-10px as specified)
    
    // Calculate the optimal width first
    ImVec2 single_line_size = SafeCalcTextSize(content, FLT_MAX);
    float node_width = std::max(min_width, std::min(max_width, single_line_size.x + padding));
    
    // Now calculate height with the determined width
    float effective_width = node_width - padding;
    ImVec2 wrapped_text_size = SafeCalcTextSize(content, effective_width);
    float node_height = std::max(min_height, wrapped_text_size.y + padding);
    
    // Remove extra +30px height for expand/collapse icons - size tightly around text
    // No additional height needed for text-only nodes
    
    // Remove aspect ratio constraints - let text determine natural size
    // Text-only nodes should be sized exactly to their content
    
    return ImVec2(node_width, node_height);
}

// Constructor
GraphManager::GraphManager()
    : last_node_added_to_graph(nullptr),
      graph_layout_dirty(false),
      force_layout(ForceDirectedLayout::LayoutParams()),
      use_force_layout(true),
      next_graph_node_id_counter(0) { // Initialize ID counter
    // graph_view_state is default constructed (selected_node_id = -1)
}

// Helper to get a node by its unique graph_node_id
GraphNode* GraphManager::GetNodeById(NodeIdType graph_node_id) {
    if (graph_node_id == kInvalidNodeId) return nullptr;
    auto it = all_nodes.find(graph_node_id);
    if (it != all_nodes.end()) {
        return it->second.get();
    }
    return nullptr;
}

// Implementation of PopulateGraphFromHistory
void GraphManager::PopulateGraphFromHistory(const std::vector<HistoryMessage>& history_messages, PersistenceManager& db_manager) {
    all_nodes.clear();
    root_nodes.clear();
    last_node_added_to_graph = nullptr;
    graph_view_state.selected_node_id = kInvalidNodeId;
    next_graph_node_id_counter = 0; // Reset ID counter

    GraphNode* previous_node_ptr = nullptr;

    for (const auto& msg : history_messages) {
        NodeIdType current_g_node_id = next_graph_node_id_counter++;
        // Constructor now takes (graph_node_id, HistoryMessage)
        auto new_node_unique_ptr = std::make_unique<GraphNode>(current_g_node_id, msg);
        GraphNode* current_node_ptr = new_node_unique_ptr.get();

        // Use the helper function to format message content consistently with linear view
        current_node_ptr->label = FormatMessageForGraph(msg, db_manager);

        current_node_ptr->parent = previous_node_ptr;
        if (previous_node_ptr != nullptr) {
            previous_node_ptr->children.push_back(current_node_ptr);
            current_node_ptr->depth = previous_node_ptr->depth + 1;
        } else {
            root_nodes.push_back(current_node_ptr);
            current_node_ptr->depth = 0;
        }

        // Calculate node size based on formatted content to ensure proper display
        current_node_ptr->size = CalculateNodeSize(current_node_ptr->label);
        
        // Ensure new nodes are visible and expanded by default for immediate content display
        current_node_ptr->is_expanded = true;
        current_node_ptr->content_needs_refresh = true; // Mark for immediate content refresh
        
        // Initialize position to zero - will be set by layout algorithm
        current_node_ptr->position = ImVec2(0.0f, 0.0f);

        // Key for all_nodes is now graph_node_id
        all_nodes[current_node_ptr->graph_node_id] = std::move(new_node_unique_ptr);
        
        previous_node_ptr = current_node_ptr;
        last_node_added_to_graph = current_node_ptr;
    }
    // Reset physics state when populating from history to ensure fresh animation
    force_layout.ResetPhysicsState();
    graph_layout_dirty = true;
    
    // Reset user interruption flag when populating from history so auto-pan can work
    graph_view_state.user_interrupted_auto_pan = false;
}

// Implementation of HandleNewHistoryMessage
// current_selected_graph_node_id is GraphNode::graph_node_id or -1
void GraphManager::HandleNewHistoryMessage(const HistoryMessage& new_msg, NodeIdType current_selected_graph_node_id, PersistenceManager& db_manager) {
    GraphNode* parent_node = nullptr;

    // 1. Determine Parent Node
    if (current_selected_graph_node_id != kInvalidNodeId) {
        parent_node = GetNodeById(current_selected_graph_node_id);
    }

    if (!parent_node && last_node_added_to_graph) {
        if (all_nodes.count(last_node_added_to_graph->graph_node_id)) { // Check using graph_node_id
            parent_node = last_node_added_to_graph;
        } else {
            last_node_added_to_graph = nullptr;
        }
    }
    
    // 2. Create and Link New GraphNode
    NodeIdType new_g_node_id = next_graph_node_id_counter++;
    // Constructor now takes (graph_node_id, HistoryMessage)
    auto new_graph_node_unique_ptr = std::make_unique<GraphNode>(new_g_node_id, new_msg);
    GraphNode* new_graph_node = new_graph_node_unique_ptr.get();

    // Use the helper function to format message content consistently with linear view
    new_graph_node->label = FormatMessageForGraph(new_msg, db_manager);

    new_graph_node->parent = parent_node;
    if (parent_node) {
        parent_node->children.push_back(new_graph_node);
        new_graph_node->depth = parent_node->depth + 1;
    } else {
        new_graph_node->depth = 0;
        root_nodes.push_back(new_graph_node);
    }

    // Calculate node size based on the formatted content to ensure proper display
    // Force recalculation to ensure new content is properly sized
    new_graph_node->size = CalculateNodeSize(new_graph_node->label);
    
    // Force immediate content refresh by ensuring the node is marked as needing visual update
    // This ensures new nodes display their full content immediately without manual refresh
    new_graph_node->is_expanded = true; // Ensure new nodes are visible by default
    new_graph_node->content_needs_refresh = true; // Mark for immediate content refresh
    
    // Initialize position to zero - will be set by layout algorithm
    new_graph_node->position = ImVec2(0.0f, 0.0f);

    // Key for all_nodes is now graph_node_id
    all_nodes[new_graph_node->graph_node_id] = std::move(new_graph_node_unique_ptr);

    last_node_added_to_graph = new_graph_node;
    // Reset physics state when adding new nodes to ensure animation restarts
    force_layout.ResetPhysicsState();
    graph_layout_dirty = true;
    
    // Reset user interruption flag when a new node is added so auto-pan can work again
    graph_view_state.user_interrupted_auto_pan = false;
    
    // Note: Auto-pan will be triggered after layout is updated and the node has a valid position
    // This is handled in the main GUI loop or wherever TriggerAutoPanToNewestNode is called
}

// Layout management functions
void GraphManager::UpdateLayout() {
    if (!use_force_layout) {
        return;
    }
    
    std::vector<GraphNode*> all_nodes_vec = GetAllNodes();
    if (all_nodes_vec.empty()) {
        return;
    }
    
    // Calculate canvas center dynamically based on the current ImGui viewport.
    // Falls back to a sensible default (1000×750) when no ImGui context
    // is active (e.g., during unit tests or headless builds).
    ImVec2 canvas_center(1000.0f, 750.0f);
    if (ImGui::GetCurrentContext() != nullptr) {
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        if (viewport != nullptr) {
            canvas_center = ImVec2(viewport->Size.x * 0.5f, viewport->Size.y * 0.5f);
        }
    }
    
    // If layout is dirty, initialize the simulation
    if (graph_layout_dirty) {
        force_layout.Initialize(all_nodes_vec, canvas_center);
        graph_layout_dirty = false; // Clear the dirty flag after initialization
    }
    
    // Perform one iteration of the force-directed simulation
    // This will return false when the simulation converges
    bool simulation_continues = force_layout.UpdateLayout(all_nodes_vec);
    
    // Keep the layout "running" by not setting graph_layout_dirty to false
    // until the simulation converges
    if (!simulation_continues) {
        // Simulation has converged, no need to continue updating
        // graph_layout_dirty remains false
    }
}

void GraphManager::SetLayoutParams(const ForceDirectedLayout::LayoutParams& params) {
    force_layout.SetParams(params);
    graph_layout_dirty = true; // Trigger layout recalculation
}

void GraphManager::ToggleForceLayout(bool enable) {
    use_force_layout = enable;
    if (enable) {
        graph_layout_dirty = true; // Trigger layout recalculation
    }
}

bool GraphManager::IsLayoutRunning() const {
    return force_layout.IsRunning();
}

void GraphManager::RestartLayoutAnimation() {
    // Reset all node positions to trigger a fresh layout
    for (auto& pair : all_nodes) {
        if (pair.second) {
            pair.second->position = ImVec2(0.0f, 0.0f);
        }
    }
    // Reset the physics state to ensure a fresh start
    force_layout.ResetPhysicsState();
    graph_layout_dirty = true;
}

void GraphManager::SetAnimationSpeed(float speed_multiplier) {
    force_layout.SetAnimationSpeed(speed_multiplier);
}

std::vector<GraphNode*> GraphManager::GetAllNodes() {
    std::vector<GraphNode*> nodes;
    nodes.reserve(all_nodes.size());
    
    for (auto& pair : all_nodes) {
        if (pair.second) {
            nodes.push_back(pair.second.get());
        }
    }
    
    return nodes;
}

// Auto-pan functionality
void GraphManager::TriggerAutoPanToNewestNode(class GraphEditor* graph_editor, const ImVec2& canvas_size) {
    if (!graph_editor || !last_node_added_to_graph) {
        return;
    }
    
    // Only trigger auto-pan if the node has a valid position (layout has been applied)
    if (last_node_added_to_graph->position.x == 0.0f && last_node_added_to_graph->position.y == 0.0f) {
        // Position hasn't been set by layout yet, skip auto-pan for now
        return;
    }
    
    // Trigger auto-pan to the newest node
    graph_editor->StartAutoPanToNode(last_node_added_to_graph, canvas_size);
}

// Placeholder for RenderGraphView - to be implemented in graph_renderer.cpp or similar
// The signature in graph_manager.h is: void RenderGraphView(GraphManager& graph_manager, GraphViewState& view_state);
// This definition should be in graph_renderer.cpp
// void RenderGraphView(GraphManager& graph_manager, GraphViewState& view_state) {
//     // Actual rendering logic will go into graph_renderer.cpp
// }