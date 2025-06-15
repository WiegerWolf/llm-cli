#include "graph_manager.h"
#include "graph_renderer.h" // For GraphEditor
#include "imgui.h" // For ImVec2, ImU32, IM_COL32
#include <core/id_types.h> // Defines NodeIdType and kInvalidNodeId sentinel
#include <iostream> // For std::cout
#include <memory> // For std::make_unique, std::move
#include <algorithm> // For std::find_if
#include <cmath> // For std::max, std::min
#include <cfloat> // For FLT_MAX
#include <mutex>

// Forward declaration of helper function from main_gui.cpp
extern std::string FormatMessageForGraph(const HistoryMessage& msg, GraphManager& graph_manager);

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
    
    return ImVec2(node_width, node_height);
}

// Constructor
GraphManager::GraphManager(Database* db_manager)
    : m_db_manager(db_manager),
      last_node_added_to_graph(nullptr),
      graph_layout_dirty(false),
      force_layout(ForceDirectedLayout::LayoutParams()),
      use_force_layout(true),
      next_graph_node_id_counter(0) { // Initialize ID counter
}

// Helper to get a node by its unique graph_node_id
std::shared_ptr<GraphNode> GraphManager::GetNodeById(NodeIdType graph_node_id) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (graph_node_id == kInvalidNodeId) return nullptr;
    auto it = all_nodes.find(graph_node_id);
    if (it != all_nodes.end()) {
        return it->second;
    }
    return nullptr;
}
NodeIdType GraphManager::GetSelectedNodeId() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return graph_view_state.selected_node_id;
}

// This is the new non-locking version for internal calls that already hold a lock.
std::string GraphManager::getModelName_nolock(ModelId model_id) {
    auto it = m_model_name_cache.find(model_id);
    if (it != m_model_name_cache.end()) {
        return it->second;
    }
    std::string model_name = m_db_manager->getModelNameById(model_id).value_or("");
    m_model_name_cache[model_id] = model_name;
    return model_name;
}

// This is the public, thread-safe version that acquires locks.
std::string GraphManager::getModelName(ModelId model_id) {
    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        auto it = m_model_name_cache.find(model_id);
        if (it != m_model_name_cache.end()) {
            return it->second;
        }
    }

    std::unique_lock<std::recursive_mutex> lock(m_mutex);
    auto it = m_model_name_cache.find(model_id);
    if (it != m_model_name_cache.end()) {
        return it->second;
    }
    std::string model_name = m_db_manager->getModelNameById(model_id).value_or("");
    m_model_name_cache[model_id] = model_name;
    return model_name;
}

void GraphManager::PopulateGraphFromHistory(const std::vector<HistoryMessage>& history_messages, Database& db_manager) {
    std::unique_lock<std::recursive_mutex> lock(m_mutex);
    all_nodes.clear();
    root_nodes.clear();
    last_node_added_to_graph = nullptr;
    graph_view_state.selected_node_id = kInvalidNodeId;
    next_graph_node_id_counter = 0;

    std::shared_ptr<GraphNode> previous_node_ptr = nullptr;

    for (const auto& msg : history_messages) {
        NodeIdType current_g_node_id = next_graph_node_id_counter++;
        auto new_node_shared_ptr = std::make_shared<GraphNode>(current_g_node_id, msg);
        new_node_shared_ptr->label = FormatMessageForGraph(msg, *this);
        new_node_shared_ptr->parent = previous_node_ptr;
        if (previous_node_ptr) {
            previous_node_ptr->add_child(new_node_shared_ptr);
            new_node_shared_ptr->depth = previous_node_ptr->depth + 1;
        } else {
            root_nodes.push_back(new_node_shared_ptr);
            new_node_shared_ptr->depth = 0;
        }

        new_node_shared_ptr->size = CalculateNodeSize(new_node_shared_ptr->label);
        new_node_shared_ptr->is_expanded = true;
        new_node_shared_ptr->content_needs_refresh = true;
        new_node_shared_ptr->position = ImVec2(0.0f, 0.0f);
        all_nodes[new_node_shared_ptr->graph_node_id] = new_node_shared_ptr;
        previous_node_ptr = new_node_shared_ptr;
        last_node_added_to_graph = new_node_shared_ptr;
    }
    force_layout.ResetPhysicsState();
    graph_layout_dirty = true;
    graph_view_state.user_interrupted_auto_pan = false;
}

void GraphManager::HandleNewHistoryMessage(const HistoryMessage& new_msg, NodeIdType current_selected_graph_node_id, Database& db_manager) {
    std::unique_lock<std::recursive_mutex> lock(m_mutex);
    std::shared_ptr<GraphNode> parent_node = nullptr;

    if (current_selected_graph_node_id != kInvalidNodeId) {
        auto it = all_nodes.find(current_selected_graph_node_id);
        if (it != all_nodes.end()) {
            parent_node = it->second;
        }
    }

    if (!parent_node && last_node_added_to_graph) {
        if (all_nodes.count(last_node_added_to_graph->graph_node_id)) { 
            auto it = all_nodes.find(last_node_added_to_graph->graph_node_id);
            if (it != all_nodes.end()) {
                parent_node = it->second;
            }
        } else {
            last_node_added_to_graph = nullptr;
        }
    }
    
    NodeIdType new_g_node_id = next_graph_node_id_counter++;
    auto new_graph_node_shared_ptr = std::make_shared<GraphNode>(new_g_node_id, new_msg);
    new_graph_node_shared_ptr->label = FormatMessageForGraph(new_msg, *this);
    new_graph_node_shared_ptr->parent = parent_node;
    if (parent_node) {
        parent_node->add_child(new_graph_node_shared_ptr);
        new_graph_node_shared_ptr->depth = parent_node->depth + 1;
    } else {
        new_graph_node_shared_ptr->depth = 0;
        root_nodes.push_back(new_graph_node_shared_ptr);
    }

    new_graph_node_shared_ptr->size = CalculateNodeSize(new_graph_node_shared_ptr->label);
    new_graph_node_shared_ptr->is_expanded = true;
    new_graph_node_shared_ptr->content_needs_refresh = true;
    new_graph_node_shared_ptr->position = ImVec2(0.0f, 0.0f);
    all_nodes[new_graph_node_shared_ptr->graph_node_id] = new_graph_node_shared_ptr;
    last_node_added_to_graph = new_graph_node_shared_ptr;
    force_layout.ResetPhysicsState();
    graph_layout_dirty = true;
    graph_view_state.user_interrupted_auto_pan = false;
}

GraphNode* GraphManager::CreateNode(NodeIdType parent_id, MessageType type, const std::string& content) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    auto parent_node = GetNodeById(parent_id);
    if (!parent_node) {
        return nullptr; // Or handle error appropriately
    }

    HistoryMessage new_msg;
    new_msg.type = type;
    new_msg.content = content;
    new_msg.timestamp = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now());
    new_msg.parent_id = parent_node->message_data.message_id;

    NodeIdType new_g_node_id = next_graph_node_id_counter++;
    auto new_node = std::make_shared<GraphNode>(new_g_node_id, new_msg);
    new_node->parent = parent_node;
    new_node->depth = parent_node->depth + 1;
    new_node->label = FormatMessageForGraph(new_msg, *this);
    new_node->size = CalculateNodeSize(new_node->label);

    parent_node->add_child(new_node);
    all_nodes[new_g_node_id] = new_node;
    last_node_added_to_graph = new_node;
    graph_layout_dirty = true;

    return new_node.get();
}

void GraphManager::UpdateLayout() {
    std::vector<std::shared_ptr<GraphNode>> all_nodes_vec;
    {
        std::unique_lock<std::recursive_mutex> lock(m_mutex);
        if (!use_force_layout) {
            return;
        }
        
        all_nodes_vec.reserve(all_nodes.size());
        for (const auto& [id, node_ptr] : all_nodes) {
            all_nodes_vec.push_back(node_ptr);
        }
    }
    if (all_nodes_vec.empty()) {
        return;
    }
    
    ImVec2 canvas_center(1000.0f, 750.0f);
    if (ImGui::GetCurrentContext() != nullptr) {
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        if (viewport != nullptr) {
            canvas_center = ImVec2(viewport->Size.x * 0.5f, viewport->Size.y * 0.5f);
        }
    }
    
    if (graph_layout_dirty) {
        force_layout.Initialize(all_nodes_vec, canvas_center);
        graph_layout_dirty = false;
    }
    
    bool simulation_continues = force_layout.UpdateLayout(all_nodes_vec);
    
    if (!simulation_continues) {
    }
}

void GraphManager::SetLayoutParams(const ForceDirectedLayout::LayoutParams& params) {
    std::unique_lock<std::recursive_mutex> lock(m_mutex);
    force_layout.SetParams(params);
    graph_layout_dirty = true;
}

void GraphManager::ToggleForceLayout(bool enable) {
    std::unique_lock<std::recursive_mutex> lock(m_mutex);
    use_force_layout = enable;
    if (enable) {
        graph_layout_dirty = true;
    }
}

bool GraphManager::IsLayoutRunning() const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return force_layout.IsRunning();
}

void GraphManager::RestartLayoutAnimation() {
    std::unique_lock<std::recursive_mutex> lock(m_mutex);
    for (auto& pair : all_nodes) {
        if (pair.second) {
            pair.second->position = ImVec2(0.0f, 0.0f);
        }
    }
    force_layout.ResetPhysicsState();
    graph_layout_dirty = true;
}

void GraphManager::SetAnimationSpeed(float speed_multiplier) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    force_layout.SetAnimationSpeed(speed_multiplier);
}

void GraphManager::setAnimationPaused(bool is_paused) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    animation_paused = is_paused;
}

bool GraphManager::isAnimationPaused() const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return animation_paused;
}

std::unordered_map<NodeIdType, std::shared_ptr<GraphNode>> GraphManager::GetAllNodes() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return all_nodes;
}

std::vector<std::shared_ptr<GraphNode>> GraphManager::GetRootNodes() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return root_nodes;
}

std::shared_ptr<GraphNode> GraphManager::GetLastNodeAdded() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return last_node_added_to_graph;
}

void GraphManager::TriggerAutoPanToNewestNode(class GraphEditor* graph_editor, const ImVec2& canvas_size) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (!graph_editor || !last_node_added_to_graph) {
        return;
    }
    
    if (last_node_added_to_graph->position.x == 0.0f && last_node_added_to_graph->position.y == 0.0f) {
        return;
    }
    
    graph_editor->StartAutoPanToNode(last_node_added_to_graph, canvas_size);
}
// Accessors for thread-safe, encapsulated access
const GraphViewState& GraphManager::getGraphViewState() const {
    // This is not thread-safe, but it's used in contexts where a lock is already held.
    // For public, safe access, use getGraphViewStateSnapshot().
    return graph_view_state;
}

GraphViewState GraphManager::getGraphViewStateSnapshot() const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return graph_view_state;
}

void GraphManager::setGraphViewState(const GraphViewState& state) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    graph_view_state = state;
}

GraphViewState& GraphManager::getGraphViewStateNonConst() {
    return graph_view_state;
}
bool GraphManager::isGraphLayoutDirty() const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return graph_layout_dirty;
}

bool GraphManager::isForceLayout() const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return force_layout.IsRunning();
}

bool GraphManager::isUseForceLayout() const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return use_force_layout;
}

uint64_t GraphManager::nextGraphNodeIdCounter() const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return next_graph_node_id_counter;
}
