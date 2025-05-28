#include "graph_layout.h"
#include <algorithm>
#include <cmath>
#include <random>

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
        
        // If node doesn't have a position yet, place it strategically
        if (node->position.x == 0.0f && node->position.y == 0.0f) {
            // Always use random placement for more dramatic animation
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_real_distribution<float> angle_dist(0.0f, 2.0f * 3.14159f);
            std::uniform_real_distribution<float> radius_dist(400.0f, 1000.0f); // Larger radius for more movement
            
            float angle = angle_dist(gen);
            float radius = radius_dist(gen);
            
            node->position.x = canvas_center.x + radius * std::cos(angle);
            node->position.y = canvas_center.y + radius * std::sin(angle);
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

// Convenience function
void ApplyForceDirectedLayout(const std::vector<GraphNode*>& nodes, 
                             const ImVec2& canvas_center,
                             const ForceDirectedLayout::LayoutParams& params) {
    ForceDirectedLayout layout(params);
    layout.ComputeLayout(nodes, canvas_center);
}