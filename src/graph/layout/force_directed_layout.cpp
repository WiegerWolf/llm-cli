#include <graph/layout/force_directed_layout.h>
#include <graph/layout/spatial_hash.h>
#include <chrono>
#include <random>
#include <iostream>
#include <algorithm>
#include <cassert>

#include <gui/views/graph_types.h>

// Bring math helpers from SpatialHash detail into this file’s scope
using detail::Distance;
using detail::Normalize;

// Threshold for displacement convergence
constexpr float kConvergenceDispThreshold = 0.05f;
namespace {
    static thread_local std::mt19937 rng{123}; // fixed seed for reproducible layouts
}

struct ForceDirectedLayoutDetail {
    static void CalculateSpringForces(ForceDirectedLayout& layout, const std::vector<std::shared_ptr<GraphNode>>& nodes) {
        for (const auto& node : nodes) {
            if (!node) continue;
            
            auto node_it = layout.node_physics_.find(node);
            if (node_it == layout.node_physics_.end()) continue;
            
            node->for_each_child([&](GraphNode* child_raw) {
                auto child_it = layout.node_physics_.find(child_raw->shared_from_this());
                if (child_it == layout.node_physics_.end()) return;
                auto& child = child_it->first;

                ImVec2 delta = ImVec2(child->position.x - node->position.x,
                                     child->position.y - node->position.y);
                float distance = Distance(node->position, child->position);

                if (distance > 0.1f) {
                    float force_magnitude = layout.params_.spring_strength * (distance - layout.params_.ideal_edge_length);

                    if (layout.params_.use_chronological_init) {
                        auto diff_duration = child->message_data.timestamp - node->message_data.timestamp;
                        long long time_diff = std::llabs(std::chrono::duration_cast<std::chrono::milliseconds>(diff_duration).count());
                        if (time_diff < 300000) {
                            force_magnitude *= 1.5f;
                        }
                    }

                    force_magnitude = std::max(-500.0f, std::min(500.0f, force_magnitude));

                    ImVec2 force_direction = Normalize(delta);
                    ImVec2 spring_force = ImVec2(force_direction.x * force_magnitude,
                                               force_direction.y * force_magnitude);

                    node_it->second.force.x += spring_force.x;
                    node_it->second.force.y += spring_force.y;
                    child_it->second.force.x -= spring_force.x;
                    child_it->second.force.y -= spring_force.y;
                }
            });
            
            if (auto parent_ptr = node->parent.lock()) {
                auto parent_it = layout.node_physics_.find(parent_ptr);
                if (parent_it != layout.node_physics_.end()) {
                    ImVec2 delta = ImVec2(parent_ptr->position.x - node->position.x,
                                          parent_ptr->position.y - node->position.y);
                    float distance = Distance(node->position, parent_ptr->position);

                    if (distance > 0.1f) {
                        float force_magnitude = layout.params_.spring_strength * (distance - layout.params_.ideal_edge_length);

                        if (layout.params_.use_chronological_init) {
                            auto diff_duration = parent_ptr->message_data.timestamp - node->message_data.timestamp;
                            long long time_diff = std::llabs(std::chrono::duration_cast<std::chrono::milliseconds>(diff_duration).count());
                            if (time_diff < 300000) {
                                force_magnitude *= 1.5f;
                            }
                        }

                        force_magnitude = std::max(-500.0f, std::min(500.0f, force_magnitude));

                        ImVec2 force_direction = Normalize(delta);
                        ImVec2 spring_force = ImVec2(force_direction.x * force_magnitude,
                                                   force_direction.y * force_magnitude);

                        node_it->second.force.x += spring_force.x * 0.5f;
                        node_it->second.force.y += spring_force.y * 0.5f;
                        parent_it->second.force.x -= spring_force.x * 0.5f;
                        parent_it->second.force.y -= spring_force.y * 0.5f;
                    }
                }
            }
        }
    }

    static void CalculateRepulsiveForces(ForceDirectedLayout& layout, const std::vector<std::shared_ptr<GraphNode>>& nodes) {
        if (nodes.empty()) return;

        layout.spatial_hash_.Insert(nodes);

        for (size_t i = 0; i < nodes.size(); ++i) {
            const auto& node1 = nodes[i];
            if (!node1) continue;

            auto node1_it = layout.node_physics_.find(node1);
            if (node1_it == layout.node_physics_.end()) continue;
            
            std::vector<int> neighbors_indices = layout.spatial_hash_.Query(node1->position, layout.params_.min_distance * 2);

            for (int j_index : neighbors_indices) {
                if (j_index <= static_cast<int>(i)) continue;
                const auto& node2 = nodes[j_index];
                if (!node2) continue;

                auto node2_it = layout.node_physics_.find(node2);
                if (node2_it == layout.node_physics_.end()) continue;

                ImVec2 delta = ImVec2(node2->position.x - node1->position.x,
                                      node2->position.y - node1->position.y);
                float distance = Distance(node1->position, node2->position);

                float combined_size = (node1->size.x + node1->size.y + node2->size.x + node2->size.y) * 0.25f;
                float effective_min_distance = std::max(layout.params_.min_distance, combined_size + 50.0f);

                if (distance > 0.1f && distance < 800.0f) {
                    float force_magnitude = layout.params_.repulsion_strength / (distance * distance);
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

    static void ApplyForces(ForceDirectedLayout& layout, const std::vector<std::shared_ptr<GraphNode>>& nodes) {
        for (const auto& node : nodes) {
            if (!node) continue;
            
            auto it = layout.node_physics_.find(node);
            if (it == layout.node_physics_.end() || it->second.is_fixed) continue;
            
            NodePhysics& physics = it->second;
            
            physics.velocity.x += physics.force.x * layout.params_.time_step;
            physics.velocity.y += physics.force.y * layout.params_.time_step;
            
            physics.velocity.x *= layout.params_.damping_factor;
            physics.velocity.y *= layout.params_.damping_factor;
            
            node->position.x += physics.velocity.x * layout.params_.time_step;
            node->position.y += physics.velocity.y * layout.params_.time_step;
            
            ConstrainToBounds(layout, node);
        }
    }

    static void ConstrainToBounds(ForceDirectedLayout& layout, const std::shared_ptr<GraphNode>& node) {
        if (!node) return;
        
        float margin = 50.0f;
        float half_width = node->size.x * 0.5f;
        float half_height = node->size.y * 0.5f;
        
        if (node->position.x - half_width < margin) {
            node->position.x = margin + half_width;
            auto it = layout.node_physics_.find(node);
            if (it != layout.node_physics_.end()) {
                it->second.velocity.x = 0.0f;
            }
        }
        if (node->position.x + half_width > layout.params_.canvas_bounds.x - margin) {
            node->position.x = layout.params_.canvas_bounds.x - margin - half_width;
            auto it = layout.node_physics_.find(node);
            if (it != layout.node_physics_.end()) {
                it->second.velocity.x = 0.0f;
            }
        }
        if (node->position.y - half_height < margin) {
            node->position.y = margin + half_height;
            auto it = layout.node_physics_.find(node);
            if (it != layout.node_physics_.end()) {
                it->second.velocity.y = 0.0f;
            }
        }
        if (node->position.y + half_height > layout.params_.canvas_bounds.y - margin) {
            node->position.y = layout.params_.canvas_bounds.y - margin - half_height;
            auto it = layout.node_physics_.find(node);
            if (it != layout.node_physics_.end()) {
                it->second.velocity.y = 0.0f;
            }
        }
    }

    static float CalculateTotalEnergy(ForceDirectedLayout& layout, const std::vector<std::shared_ptr<GraphNode>>& nodes) {
        float total_energy = 0.0f;
        
        for (const auto& node : nodes) {
            if (!node) continue;
            
            auto it = layout.node_physics_.find(node);
            if (it == layout.node_physics_.end()) continue;
            
            const NodePhysics& physics = it->second;
            total_energy += 0.5f * (physics.velocity.x * physics.velocity.x +
                                   physics.velocity.y * physics.velocity.y);
        }
        
        return total_energy;
    }

    static void InitializeChronologicalPositions(ForceDirectedLayout& layout, const std::vector<std::shared_ptr<GraphNode>>& nodes, const ImVec2& canvas_center) {
        auto sorted_nodes = SortNodesByTimestamp(nodes);
        
        if (sorted_nodes.empty()) return;
        
        float start_y = 100.0f;
        float current_y = start_y;
        
        for (size_t i = 0; i < sorted_nodes.size(); ++i) {
            const auto& node = sorted_nodes[i];
            if (!node) continue;
            
            float x_offset = static_cast<float>(node->depth) * 200.0f;
            
            std::uniform_real_distribution<float> x_jitter(-50.0f, 50.0f);
            
            node->position.x = canvas_center.x + x_offset + x_jitter(rng);
            node->position.y = current_y;
            
            current_y += layout.params_.chronological_spacing;
        }
    }

    static std::vector<std::shared_ptr<GraphNode>> SortNodesByTimestamp(const std::vector<std::shared_ptr<GraphNode>>& nodes) {
        auto sorted_nodes = nodes;
        
        std::sort(sorted_nodes.begin(), sorted_nodes.end(), [](const std::shared_ptr<GraphNode>& a, const std::shared_ptr<GraphNode>& b) {
            if (!a || !b) return false;
            return a->message_data.timestamp < b->message_data.timestamp;
        });
        
        return sorted_nodes;
    }

    static void CalculateTemporalForces(ForceDirectedLayout& layout, const std::vector<std::shared_ptr<GraphNode>>& nodes) {
        auto sorted_nodes = SortNodesByTimestamp(nodes);
        if (sorted_nodes.size() < 2) return;

        for (size_t i = 0; i < sorted_nodes.size() - 1; ++i) {
            auto& node1 = sorted_nodes[i];
            auto& node2 = sorted_nodes[i + 1];

            if (!node1 || !node2) continue;

            auto it1 = layout.node_physics_.find(node1);
            auto it2 = layout.node_physics_.find(node2);

            if (it1 != layout.node_physics_.end() && it2 != layout.node_physics_.end()) {
                float y_diff = node2->position.y - node1->position.y;
                
                // Apply a force to maintain vertical order (older nodes above newer nodes)
                if (y_diff < layout.params_.chronological_spacing * 0.8f) { // Less than ideal spacing
                    float force_magnitude = layout.params_.temporal_strength * (layout.params_.chronological_spacing - y_diff);
                    it1->second.force.y -= force_magnitude;
                    it2->second.force.y += force_magnitude;
                }

                // Also maintain some horizontal alignment based on conversation depth
                float x_diff = (node2->position.x - (float)node2->depth * 200.0f) - (node1->position.x - (float)node1->depth * 200.0f);
                float horizontal_force = -x_diff * layout.params_.temporal_strength * 0.1f;
                it1->second.force.x += horizontal_force;
                it2->second.force.x -= horizontal_force;
            }
        }
    }

    static std::vector<std::pair<std::shared_ptr<GraphNode>, std::shared_ptr<GraphNode>>> GetChronologicalNeighbors(const std::vector<std::shared_ptr<GraphNode>>& sorted_nodes) {
        std::vector<std::pair<std::shared_ptr<GraphNode>, std::shared_ptr<GraphNode>>> neighbors;
        if (sorted_nodes.size() < 2) return neighbors;

        for (size_t i = 0; i < sorted_nodes.size() - 1; ++i) {
            neighbors.push_back({sorted_nodes[i], sorted_nodes[i+1]});
        }
        return neighbors;
    }
};

ForceDirectedLayout::LayoutParams::LayoutParams()
    : spring_strength(0.05f),
      repulsion_strength(50000.0f),
      damping_factor(0.85f),
      min_distance(200.0f),
      ideal_edge_length(400.0f),
      time_step(0.008f),
      max_iterations(500),
      convergence_threshold(0.1f),
      canvas_bounds(ImVec2(2000.0f, 1500.0f)),
      temporal_strength(0.1f),
      vertical_bias(0.3f),
      chronological_spacing(150.0f),
      use_chronological_init(true) {}

ForceDirectedLayout::ForceDirectedLayout(const LayoutParams& params)
    : params_(params), is_running_(false), current_iteration_(0), spatial_hash_(params.min_distance) {}

void ForceDirectedLayout::Initialize(const std::vector<std::shared_ptr<GraphNode>>& nodes, const ImVec2& canvas_center) {
    is_running_ = true;
    current_iteration_ = 0;
    
    for (const auto& node : nodes) {
        if (!node) continue;
        
        if (node_physics_.find(node) == node_physics_.end()) {
            NodePhysics physics;
            physics.velocity = ImVec2(0.0f, 0.0f);
            physics.force = ImVec2(0.0f, 0.0f);
            physics.is_fixed = false;
            node_physics_[node] = physics;
        }
    }
    
    if (params_.use_chronological_init) {
        ForceDirectedLayoutDetail::InitializeChronologicalPositions(*this, nodes, canvas_center);
    } else {
        const float kUnsetEpsilon = 1e-3f;

        for (const auto& node : nodes) {
            if (!node) continue;

            bool position_unset = std::abs(node->position.x) < kUnsetEpsilon &&
                                  std::abs(node->position.y) < kUnsetEpsilon;

            if (position_unset) {
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
    if (!is_running_ || current_iteration_ >= params_.max_iterations) {
        is_running_ = false;
        return false;
    }
    
    for (auto& pair : node_physics_) {
        pair.second.force = ImVec2(0.0f, 0.0f);
    }
    
    ForceDirectedLayoutDetail::CalculateSpringForces(*this, nodes);
    ForceDirectedLayoutDetail::CalculateRepulsiveForces(*this, nodes);
    
    if (params_.use_chronological_init) {
        ForceDirectedLayoutDetail::CalculateTemporalForces(*this, nodes);
    }
    
    ForceDirectedLayoutDetail::ApplyForces(*this, nodes);
    
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

bool ForceDirectedLayout::IsRunning() const {
    return is_running_;
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
    spatial_hash_ = SpatialHash(params_.min_distance);
    ResetPhysicsState();
    is_running_ = true;
}

void ForceDirectedLayout::SetAnimationSpeed(float speed_multiplier) {
    speed_multiplier = std::max(0.1f, std::min(3.0f, speed_multiplier));
    params_.time_step = 0.016f * speed_multiplier;
}

void ForceDirectedLayout::ResetPhysicsState() {
    node_physics_.clear();
    is_running_ = false;
    current_iteration_ = 0;
}