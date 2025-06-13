#ifndef GRAPH_LAYOUT_H
#define GRAPH_LAYOUT_H

#include <vector>
#include <map>
#include <unordered_map> // for spatial buckets
#include <cstdint>       // for fixed-width integer hashing
#include <string>
#include "extern/imgui/imgui.h"
#include "gui_interface/graph_types.h"

// Forward declaration if GraphNode is not fully defined via graph_types.h or for header hygiene
// struct GraphNode; 

/**
 * @brief Legacy recursive layout function (kept for compatibility)
 * 
 * Recursively calculates the positions of graph nodes using a simple top-down tree layout.
 * This function is being replaced by the force-directed layout algorithm.
 */
void CalculateNodePositionsRecursive(
    GraphNode* node, 
    ImVec2 current_pos,
    float x_spacing, 
    float y_spacing, 
    int depth, 
    std::map<int, float>& level_x_offset, 
    const ImVec2& canvas_start_pos
);

/**
 * @brief Force-directed layout algorithm for graph nodes
 * 
 * Implements a physics-based layout using spring forces for connected nodes
 * and repulsive forces between all nodes to prevent overlap.
 */
class ForceDirectedLayout {
public:
    struct LayoutParams {
        float spring_strength;      // Attractive force strength for connected nodes
        float repulsion_strength;   // Repulsive force strength between all nodes
        float damping_factor;       // Velocity damping to stabilize simulation
        float min_distance;         // Minimum distance between nodes
        float ideal_edge_length;    // Ideal distance for connected nodes
        float time_step;            // Simulation time step (60 FPS)
        int max_iterations;         // Maximum iterations per update
        float convergence_threshold; // Stop when total force is below this
        ImVec2 canvas_bounds;       // Layout bounds
        
        // Chronological layout parameters
        float temporal_strength;    // Strength of chronological ordering forces
        float vertical_bias;        // Bias to maintain top-to-bottom chronological order
        float chronological_spacing; // Vertical spacing between chronologically adjacent messages
        bool use_chronological_init; // Whether to use chronological initialization
        
        // Constructor with default values
        LayoutParams()
            : spring_strength(0.05f)       // Reduce from 0.1 (weaker attraction)
            , repulsion_strength(50000.0f) // Increase from 10000 (5x stronger)
            , damping_factor(0.85f)        // Reduce from 0.9 (less damping)
            , min_distance(200.0f)         // Increase from 150px
            , ideal_edge_length(400.0f)    // Increase from 300px
            , time_step(0.008f)            // Reduce from 0.016 (prevent overshooting)
            , max_iterations(500)          // Much higher for longer animation
            , convergence_threshold(0.1f)  // Lower from 0.5 (longer animation)
            , canvas_bounds(ImVec2(2000.0f, 1500.0f))
            , temporal_strength(0.1f)      // Moderate temporal force strength
            , vertical_bias(0.3f)          // Moderate vertical bias
            , chronological_spacing(150.0f) // Default vertical spacing between messages
            , use_chronological_init(true) // Enable chronological initialization by default
        {}
    };

private:
    struct NodePhysics {
        ImVec2 velocity = ImVec2(0.0f, 0.0f);
        ImVec2 force = ImVec2(0.0f, 0.0f);
        bool is_fixed = false; // For pinning nodes in place
    };

    LayoutParams params_;
    std::map<GraphNode*, NodePhysics> node_physics_;
    bool is_running_ = false;
    int current_iteration_ = 0;
// Helper: pack 2D grid cell coordinates into 64-bit key for unordered_map buckets
    static constexpr uint64_t PackCell(int32_t x, int32_t y)
    {
        return (static_cast<uint64_t>(static_cast<uint32_t>(x)) << 32) |
               static_cast<uint32_t>(y);
    }

public:
    ForceDirectedLayout(const LayoutParams& params = LayoutParams());
    
    /**
     * @brief Initialize the layout with a set of nodes
     * @param nodes Vector of all nodes to layout
     * @param canvas_center Center point of the layout area
     */
    void Initialize(const std::vector<GraphNode*>& nodes, const ImVec2& canvas_center);
    
    /**
     * @brief Perform one iteration of the force-directed simulation
     * @param nodes Vector of all nodes to update
     * @return true if simulation should continue, false if converged
     */
    bool UpdateLayout(const std::vector<GraphNode*>& nodes);
    
    /**
     * @brief Run the complete layout algorithm until convergence
     * @param nodes Vector of all nodes to layout
     * @param canvas_center Center point of the layout area
     */
    void ComputeLayout(const std::vector<GraphNode*>& nodes, const ImVec2& canvas_center);
    
    /**
     * @brief Check if the layout simulation is currently running
     */
    bool IsRunning() const { return is_running_; }
    
    /**
     * @brief Get current layout parameters
     */
    const LayoutParams& GetParams() const { return params_; }
    
    /**
     * @brief Update layout parameters
     */
    void SetParams(const LayoutParams& params) { params_ = params; }
    
    /**
     * @brief Set animation speed multiplier (affects time step)
     */
    void SetAnimationSpeed(float speed_multiplier);
    
    /**
     * @brief Reset the physics state for all nodes
     */
    void ResetPhysicsState();
    
    /**
     * @brief Pin a node at its current position (prevent movement)
     */
    void PinNode(GraphNode* node, bool pinned = true);

private:
    /**
     * @brief Calculate attractive forces between connected nodes
     */
    void CalculateSpringForces(const std::vector<GraphNode*>& nodes);
    
    /**
     * @brief Calculate repulsive forces between all node pairs
     */
    void CalculateRepulsiveForces(const std::vector<GraphNode*>& nodes);
    
    /**
     * @brief Calculate chronological ordering forces
     */
    void CalculateTemporalForces(const std::vector<GraphNode*>& nodes);
    
    /**
     * @brief Apply forces to update node positions
     */
    void ApplyForces(const std::vector<GraphNode*>& nodes);
    
    /**
     * @brief Keep nodes within canvas bounds
     */
    void ConstrainToBounds(GraphNode* node);
    
    /**
     * @brief Calculate distance between two points
     */
    float Distance(const ImVec2& a, const ImVec2& b);
    
    /**
     * @brief Normalize a vector
     */
    ImVec2 Normalize(const ImVec2& vec);
    
    /**
     * @brief Calculate total kinetic energy of the system
     */
    float CalculateTotalEnergy(const std::vector<GraphNode*>& nodes);
    
    /**
     * @brief Initialize nodes with chronological positioning
     */
    void InitializeChronologicalPositions(const std::vector<GraphNode*>& nodes, const ImVec2& canvas_center);
    
    /**
     * @brief Sort nodes by chronological order (timestamp)
     */
    std::vector<GraphNode*> SortNodesByTimestamp(const std::vector<GraphNode*>& nodes);
    
    /**
     * @brief Find chronologically adjacent nodes for temporal forces
     */
    std::vector<std::pair<GraphNode*, GraphNode*>> GetChronologicalNeighbors(const std::vector<GraphNode*>& sorted_nodes);
};

/**
 * @brief Convenience function to apply force-directed layout to a graph
 * @param nodes Vector of all nodes to layout
 * @param canvas_center Center point of the layout area
 * @param params Layout parameters (optional)
 */
void ApplyForceDirectedLayout(const std::vector<GraphNode*>& nodes, 
                             const ImVec2& canvas_center,
                             const ForceDirectedLayout::LayoutParams& params = ForceDirectedLayout::LayoutParams());

#endif // GRAPH_LAYOUT_H