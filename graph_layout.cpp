#include "graph_layout.h"
#include <chrono>
#include <cstdlib>
#include <algorithm>
#include <cmath>
#include <random>
#include <iostream>
#include <unordered_map>
#include <cassert>
// RNG engine shared across layout operations to avoid expensive per-node construction.
// Using static thread_local for deterministic replay and thread safety.
namespace {
    static thread_local std::mt19937 rng{123}; // fixed seed for reproducible layouts
}

// Legacy recursive layout function (kept for compatibility)
void CalculateNodePositionsRecursive(
    GraphNode* node, 
    ImVec2 current_pos,
    float x_spacing, 
    float y_spacing, 
    int depth, 
    std::map<int, float>& level_x_offset, 
    const ImVec2& canvas_start_pos) 
{
    if (!node) {
        return;
    }

    // Ensure the level_x_offset has an entry for the current depth.
    if (level_x_offset.find(depth) == level_x_offset.end()) {
        level_x_offset[depth] = 0.0f; 
    }

    // Set node's position based on canvas_start_pos, level_x_offset, depth, and y_spacing
    node->position.x = canvas_start_pos.x + level_x_offset[depth];
    node->position.y = canvas_start_pos.y + (float)depth * y_spacing;

    // Update the x-offset for the current level to position the next sibling
    float node_width = (node->size.x > 0) ? node->size.x : 100.0f;
    level_x_offset[depth] += node_width + x_spacing;

    // Recursively call for children if the node is expanded and has children
    if (node->is_expanded) {
        node->for_each_child([&](GraphNode* child) {
            CalculateNodePositionsRecursive(
                child,
                canvas_start_pos,
                x_spacing,
                y_spacing,
                depth + 1,
                level_x_offset,
                canvas_start_pos
            );
        });
    }
}

// Force-directed layout implementation
ForceDirectedLayout::ForceDirectedLayout(const LayoutParams& params)
    : params_(params), is_running_(false), current_iteration_(0) {
}

void ForceDirectedLayout::Initialize(const std::vector<std::shared_ptr<GraphNode>>& nodes, const ImVec2& canvas_center) {
    // Only initialize new nodes, keep existing positions
    is_running_ = true;
    current_iteration_ = 0;
    
    // Initialize physics data for each node
    for (const auto& node : nodes) {
        if (!node) continue;
        
        // Only initialize physics if not already present
        if (node_physics_.find(node) == node_physics_.end()) {
            NodePhysics physics;
            physics.velocity = ImVec2(0.0f, 0.0f);
            physics.force = ImVec2(0.0f, 0.0f);
            physics.is_fixed = false;
            node_physics_[node] = physics;
        }
    }
    
    // Use chronological initialization if enabled
    if (params_.use_chronological_init) {
        InitializeChronologicalPositions(nodes, canvas_center);
    } else {
        // Fallback to random positioning **only** for nodes that still lack
        // a meaningful position. Nodes that already carry valid coordinates
        // (e.g., after a previous layout run) are left untouched to avoid
        // visible "jumping" when the layout is restarted (Issue #4).
        //
        // A position is considered "unset" when both coordinates are very
        // close to the origin within a small epsilon threshold.
        const float kUnsetEpsilon = 1e-3f;

        for (const auto& node : nodes) {
            if (!node) continue;

            // Detect uninitialized coordinates with an epsilon tolerance
            bool position_unset = std::abs(node->position.x) < kUnsetEpsilon &&
                                  std::abs(node->position.y) < kUnsetEpsilon;

            if (position_unset) {
                // Use shared RNG to avoid per-node engine construction
                std::uniform_real_distribution<float> angle_dist(0.0f, 2.0f * 3.14159f);
                std::uniform_real_distribution<float> radius_dist(400.0f, 1000.0f);

                float angle = angle_dist(rng);
                float radius = radius_dist(rng);

                node->position.x = canvas_center.x + radius * std::cos(angle);
                node->position.y = canvas_center.y + radius * std::sin(angle);
            }
        }
    }
}

bool ForceDirectedLayout::UpdateLayout(const std::vector<std::shared_ptr<GraphNode>>& nodes) {
    // Terminate if we have already reached the per-frame iteration cap
    if (!is_running_ || current_iteration_ >= params_.max_iterations) {
        is_running_ = false;
        return false;
    }
    
    // Reset forces
    for (auto& pair : node_physics_) {
        pair.second.force = ImVec2(0.0f, 0.0f);
    }
    
    // Calculate forces
    CalculateSpringForces(nodes);
    CalculateRepulsiveForces(nodes);
    
    // Add chronological ordering forces if enabled
    if (params_.use_chronological_init) {
        CalculateTemporalForces(nodes);
    }
    
    // Apply forces and update positions
    ApplyForces(nodes);
    
    // ------------------------------------------------------------------
    // Adaptive convergence detection
    // ------------------------------------------------------------------
    // Measure the greatest per-node displacement (approximated via velocity
    // magnitude times Δt).  When that value falls below an empirically
    // determined threshold we assume visual stabilisation and stop iterating
    // early, saving CPU/GPU time on small graphs.
    float max_disp = 0.0f;
    for (const auto& node : nodes) {
        if (!node) continue;
        auto it = node_physics_.find(node);
        if (it == node_physics_.end()) continue;
        const NodePhysics& phys = it->second;
        float disp = std::sqrt(phys.velocity.x * phys.velocity.x +
                               phys.velocity.y * phys.velocity.y) * params_.time_step;
        max_disp = std::max(max_disp, disp);
    }
    
    if (max_disp < kConvergenceDispThreshold) {
        is_running_ = false;
        return false;
    }
    
    current_iteration_++;
    return true;
}

void ForceDirectedLayout::ComputeLayout(const std::vector<std::shared_ptr<GraphNode>>& nodes, const ImVec2& canvas_center) {
    Initialize(nodes, canvas_center);
    
    while (UpdateLayout(nodes)) {
        // Continue until convergence or max iterations
    }
}

void ForceDirectedLayout::PinNode(std::shared_ptr<GraphNode> node, bool pinned) {
    auto it = node_physics_.find(node);
    if (it != node_physics_.end()) {
        it->second.is_fixed = pinned;
        if (pinned) {
            it->second.velocity = ImVec2(0.0f, 0.0f);
        }
    }
}

void ForceDirectedLayout::SetParams(const LayoutParams& params) {
    params_ = params;
    ResetPhysicsState();
    is_running_ = true; // Immediately restart with new params
}
void ForceDirectedLayout::SetAnimationSpeed(float speed_multiplier) {
    // Clamp speed multiplier to reasonable bounds
    speed_multiplier = std::max(0.1f, std::min(3.0f, speed_multiplier));
    
    // Adjust time step based on speed multiplier
    // Base time step is 0.016f (60 FPS), scale it by the multiplier
    params_.time_step = 0.016f * speed_multiplier;
}

void ForceDirectedLayout::ResetPhysicsState() {
    // Clear all physics data to force reinitialization
    node_physics_.clear();
    is_running_ = false;
    current_iteration_ = 0;
}

void ForceDirectedLayout::CalculateSpringForces(const std::vector<std::shared_ptr<GraphNode>>& nodes) {
    for (const auto& node : nodes) {
        if (!node) continue;
        
        auto node_it = node_physics_.find(node);
        if (node_it == node_physics_.end()) continue;
        
        // Apply spring forces to connected nodes (parent-child relationships)
        node->for_each_child([&](GraphNode* child_raw) {
            auto child_it = node_physics_.find(child_raw->shared_from_this()); // Find requires shared_ptr
            if (child_it == node_physics_.end()) return;
            auto& child = child_it->first;

            ImVec2 delta = ImVec2(child->position.x - node->position.x,
                                 child->position.y - node->position.y);
            float distance = Distance(node->position, child->position);

            if (distance > 0.1f) { // Avoid division by zero
                // Spring force: F = k * (distance - ideal_length) * direction
                float force_magnitude = params_.spring_strength * (distance - params_.ideal_edge_length);

                // Enhance spring force for chronologically adjacent nodes
                if (params_.use_chronological_init) {
                    auto diff_duration = child->message_data.timestamp - node->message_data.timestamp;
                    long long time_diff = std::llabs(std::chrono::duration_cast<std::chrono::milliseconds>(diff_duration).count());
                    // If messages are close in time (within 5 minutes), strengthen the connection
                    if (time_diff < 300000) { // 5 minutes in milliseconds
                        force_magnitude *= 1.5f; // Stronger attraction for temporally close messages
                    }
                }

                // Cap spring force to prevent instability
                force_magnitude = std::max(-500.0f, std::min(500.0f, force_magnitude));

                ImVec2 force_direction = Normalize(delta);
                ImVec2 spring_force = ImVec2(force_direction.x * force_magnitude,
                                           force_direction.y * force_magnitude);

                // Apply equal and opposite forces
                node_it->second.force.x += spring_force.x;
                node_it->second.force.y += spring_force.y;
                child_it->second.force.x -= spring_force.x;
                child_it->second.force.y -= spring_force.y;
            }
        });
        
        // Also apply spring forces to parent (bidirectional)
        if (auto parent_ptr = node->parent.lock()) {
            auto parent_it = node_physics_.find(parent_ptr);
            if (parent_it != node_physics_.end()) {
                ImVec2 delta = ImVec2(parent_ptr->position.x - node->position.x,
                                      parent_ptr->position.y - node->position.y);
                float distance = Distance(node->position, parent_ptr->position);

                if (distance > 0.1f) {
                    float force_magnitude = params_.spring_strength * (distance - params_.ideal_edge_length);

                    // Enhance spring force for chronologically adjacent nodes
                    if (params_.use_chronological_init) {
                        auto diff_duration = parent_ptr->message_data.timestamp - node->message_data.timestamp;
                        long long time_diff = std::llabs(std::chrono::duration_cast<std::chrono::milliseconds>(diff_duration).count());
                        if (time_diff < 300000) { // 5 minutes in milliseconds
                            force_magnitude *= 1.5f;
                        }
                    }

                    force_magnitude = std::max(-500.0f, std::min(500.0f, force_magnitude));

                    ImVec2 force_direction = Normalize(delta);
                    ImVec2 spring_force = ImVec2(force_direction.x * force_magnitude,
                                               force_direction.y * force_magnitude);

                    // Apply forces (weaker for parent to maintain hierarchy)
                    node_it->second.force.x += spring_force.x * 0.5f;
                    node_it->second.force.y += spring_force.y * 0.5f;
                    parent_it->second.force.x -= spring_force.x * 0.5f;
                    parent_it->second.force.y -= spring_force.y * 0.5f;
                }
            }
        }
    }
}

void ForceDirectedLayout::CalculateRepulsiveForces(const std::vector<std::shared_ptr<GraphNode>>& nodes) {
    // Uniform-grid spatial hashing to reduce O(N²) pair checks to ~O(N)
    if (nodes.empty()) return;

    // Clamp cell_size to a minimum of 1.0f to avoid division by zero.
    const float cell_size = std::max(1.0f, params_.min_distance);
    assert(params_.min_distance > 0.f && "min_distance must be positive");
    std::unordered_map<uint64_t, std::vector<int>> buckets;
    buckets.reserve(nodes.size() * 2);

    // First pass – bucket each node by cell
    for (size_t idx = 0; idx < nodes.size(); ++idx) {
        const auto& n = nodes[idx];
        if (!n) continue;
        int32_t cx = static_cast<int32_t>(std::floor(n->position.x / cell_size));
        int32_t cy = static_cast<int32_t>(std::floor(n->position.y / cell_size));
        buckets[PackCell(cx, cy)].push_back(static_cast<int>(idx));
    }

    // Second pass – for each node, only compare with nodes in its cell & 8 neighbors
    for (size_t i = 0; i < nodes.size(); ++i) {
        const auto& node1 = nodes[i];
        if (!node1) continue;

        auto node1_it = node_physics_.find(node1);
        if (node1_it == node_physics_.end()) continue;

        int32_t cx = static_cast<int32_t>(std::floor(node1->position.x / cell_size));
        int32_t cy = static_cast<int32_t>(std::floor(node1->position.y / cell_size));

        for (int dx = -1; dx <= 1; ++dx) {
            for (int dy = -1; dy <= 1; ++dy) {
                uint64_t key = PackCell(cx + dx, cy + dy);
                auto bucket_it = buckets.find(key);
                if (bucket_it == buckets.end()) continue;

                const std::vector<int>& indices = bucket_it->second;
                for (int j_index : indices) {
                    if (j_index <= static_cast<int>(i)) continue; // avoid double-counting
                    const auto& node2 = nodes[j_index];
                    if (!node2) continue;

                    auto node2_it = node_physics_.find(node2);
                    if (node2_it == node_physics_.end()) continue;

                    ImVec2 delta = ImVec2(node2->position.x - node1->position.x,
                                          node2->position.y - node1->position.y);
                    float distance = Distance(node1->position, node2->position);

                    // Adaptive spacing based on node sizes
                    float combined_size = (node1->size.x + node1->size.y + node2->size.x + node2->size.y) * 0.25f;
                    float effective_min_distance = std::max(params_.min_distance, combined_size + 50.0f);

                    if (distance > 0.1f && distance < 800.0f) {
                        float force_magnitude = params_.repulsion_strength / (distance * distance);
                        if (distance < effective_min_distance) {
                            force_magnitude *= 3.0f;
                        }
                        force_magnitude = std::min(force_magnitude, 5000.0f);

                        ImVec2 force_direction = Normalize(delta);
                        ImVec2 repulsive_force = ImVec2(force_direction.x * force_magnitude,
                                                        force_direction.y * force_magnitude);

                        node1_it->second.force.x -= repulsive_force.x;
                        node1_it->second.force.y -= repulsive_force.y;
                        node2_it->second.force.x += repulsive_force.x;
                        node2_it->second.force.y += repulsive_force.y;
                    }
                }
            }
        }
    }
}

void ForceDirectedLayout::ApplyForces(const std::vector<std::shared_ptr<GraphNode>>& nodes) {
    for (const auto& node : nodes) {
        if (!node) continue;
        
        auto it = node_physics_.find(node);
        if (it == node_physics_.end() || it->second.is_fixed) continue;
        
        NodePhysics& physics = it->second;
        
        // Update velocity: v = v + F * dt
        physics.velocity.x += physics.force.x * params_.time_step;
        physics.velocity.y += physics.force.y * params_.time_step;
        
        // Apply damping: v = v * damping_factor
        physics.velocity.x *= params_.damping_factor;
        physics.velocity.y *= params_.damping_factor;
        
        // Update position: p = p + v * dt
        node->position.x += physics.velocity.x * params_.time_step;
        node->position.y += physics.velocity.y * params_.time_step;
        
        // Constrain to bounds
        ConstrainToBounds(node);
    }
}

void ForceDirectedLayout::ConstrainToBounds(const std::shared_ptr<GraphNode>& node) {
    if (!node) return;
    
    // Keep nodes within canvas bounds
    float margin = 50.0f;
    float half_width = node->size.x * 0.5f;
    float half_height = node->size.y * 0.5f;
    
    if (node->position.x - half_width < margin) {
        node->position.x = margin + half_width;
        auto it = node_physics_.find(node);
        if (it != node_physics_.end()) {
            it->second.velocity.x = 0.0f; // Stop horizontal movement
        }
    }
    if (node->position.x + half_width > params_.canvas_bounds.x - margin) {
        node->position.x = params_.canvas_bounds.x - margin - half_width;
        auto it = node_physics_.find(node);
        if (it != node_physics_.end()) {
            it->second.velocity.x = 0.0f;
        }
    }
    if (node->position.y - half_height < margin) {
        node->position.y = margin + half_height;
        auto it = node_physics_.find(node);
        if (it != node_physics_.end()) {
            it->second.velocity.y = 0.0f; // Stop vertical movement
        }
    }
    if (node->position.y + half_height > params_.canvas_bounds.y - margin) {
        node->position.y = params_.canvas_bounds.y - margin - half_height;
        auto it = node_physics_.find(node);
        if (it != node_physics_.end()) {
            it->second.velocity.y = 0.0f;
        }
    }
}

float ForceDirectedLayout::Distance(const ImVec2& a, const ImVec2& b) {
    float dx = b.x - a.x;
    float dy = b.y - a.y;
    return std::sqrt(dx * dx + dy * dy);
}

ImVec2 ForceDirectedLayout::Normalize(const ImVec2& vec) {
    float length = std::sqrt(vec.x * vec.x + vec.y * vec.y);
    if (length > 0.001f) {
        return ImVec2(vec.x / length, vec.y / length);
    }
    return ImVec2(0.0f, 0.0f);
}

float ForceDirectedLayout::CalculateTotalEnergy(const std::vector<std::shared_ptr<GraphNode>>& nodes) {
    float total_energy = 0.0f;
    
    for (const auto& node : nodes) {
        if (!node) continue;
        
        auto it = node_physics_.find(node);
        if (it == node_physics_.end()) continue;
        
        const NodePhysics& physics = it->second;
        // Kinetic energy = 0.5 * m * v^2 (assuming mass = 1)
        total_energy += 0.5f * (physics.velocity.x * physics.velocity.x + 
                               physics.velocity.y * physics.velocity.y);
    }
    
    return total_energy;
}

void ForceDirectedLayout::InitializeChronologicalPositions(const std::vector<std::shared_ptr<GraphNode>>& nodes, const ImVec2& canvas_center) {
    // Sort nodes by timestamp (oldest first)
    auto sorted_nodes = SortNodesByTimestamp(nodes);
    
    if (sorted_nodes.empty()) return;
    
    
    // Start near the TOP of the canvas for the oldest (earliest) messages so that
    // chronologically earlier messages appear higher (smaller Y value) and newer ones lower.
    float start_y = 100.0f;              // Top margin
    float current_y = start_y;
    
    // Position nodes chronologically from top to bottom (oldest to newest)
    for (size_t i = 0; i < sorted_nodes.size(); ++i) {
        const auto& node = sorted_nodes[i];
        if (!node) continue;
        
        // ALWAYS override position for chronological layout to ensure correct ordering
        // Use depth for horizontal offset to maintain conversation structure
        float x_offset = static_cast<float>(node->depth) * 200.0f;
        
        // Add some randomness to avoid perfect alignment using shared RNG
        std::uniform_real_distribution<float> x_jitter(-50.0f, 50.0f);
        
        node->position.x = canvas_center.x + x_offset + x_jitter(rng);
        node->position.y = current_y;
        
        // Increment Y position for next node (moving down for newer messages)
        current_y += params_.chronological_spacing;
    }
}

std::vector<std::shared_ptr<GraphNode>> ForceDirectedLayout::SortNodesByTimestamp(const std::vector<std::shared_ptr<GraphNode>>& nodes) {
    auto sorted_nodes = nodes;
    
    // Sort by timestamp (chronological order — oldest first, newest last)
    // Older messages (smaller timestamps) come first in the array and will be
    // positioned nearer the TOP of the screen (smaller Y); newer ones lower.
    std::sort(sorted_nodes.begin(), sorted_nodes.end(),
        [](const auto& a, const auto& b) {
            if (!a || !b) return false;
            return a->message_data.timestamp < b->message_data.timestamp;
        });
    
    return sorted_nodes;
}

std::vector<std::pair<std::shared_ptr<GraphNode>, std::shared_ptr<GraphNode>>> ForceDirectedLayout::GetChronologicalNeighbors(const std::vector<std::shared_ptr<GraphNode>>& sorted_nodes) {
    std::vector<std::pair<std::shared_ptr<GraphNode>, std::shared_ptr<GraphNode>>> neighbors;
    
    // Create pairs of chronologically adjacent nodes
    for (size_t i = 0; i < sorted_nodes.size() - 1; ++i) {
        if (sorted_nodes[i] && sorted_nodes[i + 1]) {
            neighbors.emplace_back(sorted_nodes[i], sorted_nodes[i + 1]);
        }
    }
    
    return neighbors;
}

void ForceDirectedLayout::CalculateTemporalForces(const std::vector<std::shared_ptr<GraphNode>>& nodes) {
    // Sort nodes by timestamp to find chronological neighbors
    auto sorted_nodes = SortNodesByTimestamp(nodes);
    auto chronological_pairs = GetChronologicalNeighbors(sorted_nodes);
    
    // Apply temporal ordering forces between chronologically adjacent messages
    for (const auto& pair : chronological_pairs) {
        const auto& earlier_node = pair.first;  // Should be higher up (smaller Y)
        const auto& later_node   = pair.second; // Should be lower down (larger Y)
        
        if (!earlier_node || !later_node) continue;
        
        auto earlier_it = node_physics_.find(earlier_node);
        auto later_it   = node_physics_.find(later_node);
        if (earlier_it == node_physics_.end() || later_it == node_physics_.end()) continue;
        
        // Calculate vertical bias force to maintain chronological order.
        // Correct convention: earlier_node should have SMALLER Y (higher on screen),
        // later_node should have LARGER Y (lower on screen).
        float vertical_delta = later_node->position.y - earlier_node->position.y;  // Positive if later is below earlier
        
        // If later message is not sufficiently lower (or is above) the earlier one,
        // apply corrective force to pull earlier up and push later down.
        if (vertical_delta < params_.chronological_spacing * 0.5f) {
            float correction_force = params_.temporal_strength * 100.0f;
            
            // Pull earlier node UP (decrease Y), push later node DOWN (increase Y)
            earlier_it->second.force.y -= correction_force * params_.vertical_bias;
            later_it->second.force.y   += correction_force * params_.vertical_bias;
        }
        
        // Apply weak attractive force between chronologically adjacent messages
        ImVec2 delta = ImVec2(later_node->position.x - earlier_node->position.x,
                             later_node->position.y - earlier_node->position.y);
        float distance = Distance(earlier_node->position, later_node->position);
        
        if (distance > 0.1f) {
            // Temporal spring force (much weaker than structural springs)
            float ideal_temporal_distance = params_.chronological_spacing;
            float force_magnitude = params_.temporal_strength * (distance - ideal_temporal_distance) * 0.2f;
            
            // Cap temporal force
            force_magnitude = std::max(-100.0f, std::min(100.0f, force_magnitude));
            
            ImVec2 force_direction = Normalize(delta);
            ImVec2 temporal_force = ImVec2(force_direction.x * force_magnitude,
                                         force_direction.y * force_magnitude);
            
            // Apply temporal attraction forces (weaker)
            earlier_it->second.force.x += temporal_force.x * 0.5f;
            earlier_it->second.force.y += temporal_force.y * 0.5f;
            later_it->second.force.x -= temporal_force.x * 0.5f;
            later_it->second.force.y -= temporal_force.y * 0.5f;
        }
    }
    
    // Apply general vertical bias to all nodes to maintain top-to-bottom chronological flow
    for (const auto& node : nodes) {
        if (!node) continue;
        
        auto it = node_physics_.find(node);
        if (it == node_physics_.end()) continue;
        
        // Find the node's chronological position
        auto sorted_it = std::find(sorted_nodes.begin(), sorted_nodes.end(), node);
        if (sorted_it != sorted_nodes.end()) {
            size_t chronological_index = std::distance(sorted_nodes.begin(), sorted_it);
            
            // Calculate expected Y position (older messages at TOP, newer at bottom)
            // This logic MUST match InitializeChronologicalPositions for stability.
            float expected_y = 100.0f + (chronological_index * params_.chronological_spacing);
            
            float y_deviation = node->position.y - expected_y;
            
            // Apply corrective force if node deviates from expected chronological position
            if (std::abs(y_deviation) > params_.chronological_spacing * 0.3f) {
                float correction_strength = params_.vertical_bias * 0.2f;
                it->second.force.y -= y_deviation * correction_strength;
            }
        }
    }
}

// Convenience function
void ApplyForceDirectedLayout(const std::vector<std::shared_ptr<GraphNode>>& nodes,
                             const ImVec2& canvas_center,
                             const ForceDirectedLayout::LayoutParams& params) {
    ForceDirectedLayout layout(params);
    layout.ComputeLayout(nodes, canvas_center);
}