#include "graph_layout.h"
#include <algorithm>
#include <cmath>
#include <random>
#include <iostream>

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
    if (node->is_expanded && !node->children.empty()) {
        for (GraphNode* child : node->children) {
            if (child) {
                CalculateNodePositionsRecursive(
                    child, 
                    canvas_start_pos,
                    x_spacing, 
                    y_spacing, 
                    depth + 1, 
                    level_x_offset, 
                    canvas_start_pos
                );
            }
        }
    }
}

// Force-directed layout implementation
ForceDirectedLayout::ForceDirectedLayout(const LayoutParams& params)
    : params_(params), is_running_(false), current_iteration_(0) {
}

void ForceDirectedLayout::Initialize(const std::vector<GraphNode*>& nodes, const ImVec2& canvas_center) {
    // Only initialize new nodes, keep existing positions
    is_running_ = true;
    current_iteration_ = 0;
    
    // Initialize physics data for each node
    for (GraphNode* node : nodes) {
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
        // Fallback to random positioning for nodes without positions
        for (GraphNode* node : nodes) {
            if (!node) continue;
            
            // If node doesn't have a position yet, place it randomly
            if (node->position.x == 0.0f && node->position.y == 0.0f) {
                std::random_device rd;
                std::mt19937 gen(rd());
                std::uniform_real_distribution<float> angle_dist(0.0f, 2.0f * 3.14159f);
                std::uniform_real_distribution<float> radius_dist(400.0f, 1000.0f);
                
                float angle = angle_dist(gen);
                float radius = radius_dist(gen);
                
                node->position.x = canvas_center.x + radius * std::cos(angle);
                node->position.y = canvas_center.y + radius * std::sin(angle);
            }
        }
    }
}

bool ForceDirectedLayout::UpdateLayout(const std::vector<GraphNode*>& nodes) {
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
    
    // Check for convergence - only after a minimum number of iterations
    if (current_iteration_ > 50) { // Ensure at least 50 iterations for proper separation
        float total_energy = CalculateTotalEnergy(nodes);
        if (total_energy < params_.convergence_threshold) {
            is_running_ = false;
            return false;
        }
    }
    
    current_iteration_++;
    return true;
}

void ForceDirectedLayout::ComputeLayout(const std::vector<GraphNode*>& nodes, const ImVec2& canvas_center) {
    Initialize(nodes, canvas_center);
    
    while (UpdateLayout(nodes)) {
        // Continue until convergence or max iterations
    }
}

void ForceDirectedLayout::PinNode(GraphNode* node, bool pinned) {
    auto it = node_physics_.find(node);
    if (it != node_physics_.end()) {
        it->second.is_fixed = pinned;
        if (pinned) {
            it->second.velocity = ImVec2(0.0f, 0.0f);
        }
    }
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

void ForceDirectedLayout::CalculateSpringForces(const std::vector<GraphNode*>& nodes) {
    for (GraphNode* node : nodes) {
        if (!node) continue;
        
        auto node_it = node_physics_.find(node);
        if (node_it == node_physics_.end()) continue;
        
        // Apply spring forces to connected nodes (parent-child relationships)
        for (GraphNode* child : node->children) {
            if (!child) continue;
            
            auto child_it = node_physics_.find(child);
            if (child_it == node_physics_.end()) continue;
            
            ImVec2 delta = ImVec2(child->position.x - node->position.x,
                                 child->position.y - node->position.y);
            float distance = Distance(node->position, child->position);
            
            if (distance > 0.1f) { // Avoid division by zero
                // Spring force: F = k * (distance - ideal_length) * direction
                float force_magnitude = params_.spring_strength * (distance - params_.ideal_edge_length);
                
                // Enhance spring force for chronologically adjacent nodes
                if (params_.use_chronological_init) {
                    long long time_diff = std::abs(child->message_data.timestamp - node->message_data.timestamp);
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
        }
        
        // Also apply spring forces to parent (bidirectional)
        if (node->parent) {
            auto parent_it = node_physics_.find(node->parent);
            if (parent_it != node_physics_.end()) {
                ImVec2 delta = ImVec2(node->parent->position.x - node->position.x,
                                     node->parent->position.y - node->position.y);
                float distance = Distance(node->position, node->parent->position);
                
                if (distance > 0.1f) {
                    float force_magnitude = params_.spring_strength * (distance - params_.ideal_edge_length);
                    
                    // Enhance spring force for chronologically adjacent nodes
                    if (params_.use_chronological_init) {
                        long long time_diff = std::abs(node->parent->message_data.timestamp - node->message_data.timestamp);
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

void ForceDirectedLayout::CalculateRepulsiveForces(const std::vector<GraphNode*>& nodes) {
    for (size_t i = 0; i < nodes.size(); ++i) {
        GraphNode* node1 = nodes[i];
        if (!node1) continue;
        
        auto node1_it = node_physics_.find(node1);
        if (node1_it == node_physics_.end()) continue;
        
        for (size_t j = i + 1; j < nodes.size(); ++j) {
            GraphNode* node2 = nodes[j];
            if (!node2) continue;
            
            auto node2_it = node_physics_.find(node2);
            if (node2_it == node_physics_.end()) continue;
            
            ImVec2 delta = ImVec2(node2->position.x - node1->position.x,
                                 node2->position.y - node1->position.y);
            float distance = Distance(node1->position, node2->position);
            
            // Consider node sizes for better spacing
            float combined_size = (node1->size.x + node1->size.y + node2->size.x + node2->size.y) * 0.25f;
            float effective_min_distance = std::max(params_.min_distance, combined_size + 50.0f);
            
            if (distance > 0.1f && distance < 800.0f) { // Limit repulsion range
                // Coulomb repulsion: F = k / distance^2
                float force_magnitude = params_.repulsion_strength / (distance * distance);
                
                // Stronger repulsion when too close
                if (distance < effective_min_distance) {
                    force_magnitude *= 3.0f;
                }
                
                // Cap maximum force to prevent instability
                force_magnitude = std::min(force_magnitude, 5000.0f);
                
                ImVec2 force_direction = Normalize(delta);
                ImVec2 repulsive_force = ImVec2(force_direction.x * force_magnitude,
                                              force_direction.y * force_magnitude);
                
                // Apply repulsive forces (push apart)
                node1_it->second.force.x -= repulsive_force.x;
                node1_it->second.force.y -= repulsive_force.y;
                node2_it->second.force.x += repulsive_force.x;
                node2_it->second.force.y += repulsive_force.y;
            }
        }
    }
}

void ForceDirectedLayout::ApplyForces(const std::vector<GraphNode*>& nodes) {
    for (GraphNode* node : nodes) {
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

void ForceDirectedLayout::ConstrainToBounds(GraphNode* node) {
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

float ForceDirectedLayout::CalculateTotalEnergy(const std::vector<GraphNode*>& nodes) {
    float total_energy = 0.0f;
    
    for (GraphNode* node : nodes) {
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

void ForceDirectedLayout::InitializeChronologicalPositions(const std::vector<GraphNode*>& nodes, const ImVec2& canvas_center) {
    // Sort nodes by timestamp (oldest first)
    std::vector<GraphNode*> sorted_nodes = SortNodesByTimestamp(nodes);
    
    if (sorted_nodes.empty()) return;
    
    
    // FIX: Calculate starting position - start at BOTTOM for oldest messages
    // Since the visual coordinate system appears to have newer messages higher up,
    // we need to position older messages at higher Y values (bottom of canvas)
    float canvas_height = params_.canvas_bounds.y;
    float start_y = canvas_height - 100.0f; // Start near the bottom of the canvas for oldest
    float current_y = start_y;
    
    // Position nodes chronologically from bottom to top (oldest to newest)
    for (size_t i = 0; i < sorted_nodes.size(); ++i) {
        GraphNode* node = sorted_nodes[i];
        if (!node) continue;
        
        // ALWAYS override position for chronological layout to ensure correct ordering
        // Use depth for horizontal offset to maintain conversation structure
        float x_offset = static_cast<float>(node->depth) * 200.0f;
        
        // Add some randomness to avoid perfect alignment
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> x_jitter(-50.0f, 50.0f);
        
        node->position.x = canvas_center.x + x_offset + x_jitter(gen);
        node->position.y = current_y;
        
        
        // Decrement Y position for next node (moving up for newer messages)
        current_y -= params_.chronological_spacing;
    }
}

std::vector<GraphNode*> ForceDirectedLayout::SortNodesByTimestamp(const std::vector<GraphNode*>& nodes) {
    std::vector<GraphNode*> sorted_nodes = nodes;
    
    // Sort by timestamp (chronological order - oldest first, newest last)
    // This ensures older messages (smaller timestamps) come first in the array
    // and will be positioned at the bottom of the screen (higher Y coordinates)
    std::sort(sorted_nodes.begin(), sorted_nodes.end(),
        [](const GraphNode* a, const GraphNode* b) {
            if (!a || !b) return false;
            return a->message_data.timestamp < b->message_data.timestamp;
        });
    
    return sorted_nodes;
}

std::vector<std::pair<GraphNode*, GraphNode*>> ForceDirectedLayout::GetChronologicalNeighbors(const std::vector<GraphNode*>& sorted_nodes) {
    std::vector<std::pair<GraphNode*, GraphNode*>> neighbors;
    
    // Create pairs of chronologically adjacent nodes
    for (size_t i = 0; i < sorted_nodes.size() - 1; ++i) {
        if (sorted_nodes[i] && sorted_nodes[i + 1]) {
            neighbors.emplace_back(sorted_nodes[i], sorted_nodes[i + 1]);
        }
    }
    
    return neighbors;
}

void ForceDirectedLayout::CalculateTemporalForces(const std::vector<GraphNode*>& nodes) {
    // Sort nodes by timestamp to find chronological neighbors
    std::vector<GraphNode*> sorted_nodes = SortNodesByTimestamp(nodes);
    std::vector<std::pair<GraphNode*, GraphNode*>> chronological_pairs = GetChronologicalNeighbors(sorted_nodes);
    
    // Apply temporal ordering forces between chronologically adjacent messages
    for (const auto& pair : chronological_pairs) {
        GraphNode* earlier_node = pair.first;  // Should be higher up (smaller Y)
        GraphNode* later_node = pair.second;   // Should be lower down (larger Y)
        
        if (!earlier_node || !later_node) continue;
        
        auto earlier_it = node_physics_.find(earlier_node);
        auto later_it = node_physics_.find(later_node);
        if (earlier_it == node_physics_.end() || later_it == node_physics_.end()) continue;
        
        // Calculate vertical bias force to maintain chronological order
        // FIX: In the corrected coordinate system:
        // earlier_node should have LARGER Y (lower on screen), later_node should have SMALLER Y (higher on screen)
        float vertical_delta = earlier_node->position.y - later_node->position.y;
        
        
        // If earlier message is above later message (vertical_delta < 0), apply corrective force
        if (vertical_delta < params_.chronological_spacing * 0.5f) {
            float correction_force = params_.temporal_strength * 100.0f;
            
            
            // Push earlier node down (increase Y), later node up (decrease Y)
            earlier_it->second.force.y += correction_force * params_.vertical_bias;
            later_it->second.force.y -= correction_force * params_.vertical_bias;
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
    for (GraphNode* node : nodes) {
        if (!node) continue;
        
        auto it = node_physics_.find(node);
        if (it == node_physics_.end()) continue;
        
        // Find the node's chronological position
        auto sorted_it = std::find(sorted_nodes.begin(), sorted_nodes.end(), node);
        if (sorted_it != sorted_nodes.end()) {
            size_t chronological_index = std::distance(sorted_nodes.begin(), sorted_it);
            
            // Calculate expected Y position (older messages at bottom, newer at top)
            // Use same calculation as in InitializeChronologicalPositions
            float canvas_height = params_.canvas_bounds.y;
            float expected_y = canvas_height - 100.0f - (chronological_index * params_.chronological_spacing);
            
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
void ApplyForceDirectedLayout(const std::vector<GraphNode*>& nodes,
                             const ImVec2& canvas_center,
                             const ForceDirectedLayout::LayoutParams& params) {
    ForceDirectedLayout layout(params);
    layout.ComputeLayout(nodes, canvas_center);
}