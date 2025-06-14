#ifndef FORCE_DIRECTED_LAYOUT_H
#define FORCE_DIRECTED_LAYOUT_H

#include <vector>
#include <map>
#include <memory>
#include "extern/imgui/imgui.h"
#include "gui_interface/graph_types.h"
#include "spatial_hash.h"

class ForceDirectedLayout {
public:
    struct LayoutParams {
        float spring_strength;
        float repulsion_strength;
        float damping_factor;
        float min_distance;
        float ideal_edge_length;
        float time_step;
        int max_iterations;
        float convergence_threshold;
        ImVec2 canvas_bounds;
        float temporal_strength;
        float vertical_bias;
        float chronological_spacing;
        bool use_chronological_init;

        LayoutParams();
    };

private:
    struct NodePhysics {
        ImVec2 velocity = ImVec2(0.0f, 0.0f);
        ImVec2 force = ImVec2(0.0f, 0.0f);
        bool is_fixed = false;
    };

    static constexpr float kConvergenceDispThreshold = 0.01f;

    LayoutParams params_;
    std::map<std::shared_ptr<GraphNode>, NodePhysics> node_physics_;
    bool is_running_ = false;
    int current_iteration_ = 0;
    SpatialHash spatial_hash_;

public:
    ForceDirectedLayout(const LayoutParams& params = LayoutParams());
    
    void Initialize(const std::vector<std::shared_ptr<GraphNode>>& nodes, const ImVec2& canvas_center);
    bool UpdateLayout(const std::vector<std::shared_ptr<GraphNode>>& nodes);
    void ComputeLayout(const std::vector<std::shared_ptr<GraphNode>>& nodes, const ImVec2& canvas_center);
    bool IsRunning() const;
    const LayoutParams& GetParams() const { return params_; }
    void SetParams(const LayoutParams& params);
    void SetAnimationSpeed(float speed_multiplier);
    void ResetPhysicsState();
    void PinNode(std::shared_ptr<GraphNode> node, bool pinned = true);

private:
    void CalculateSpringForces(const std::vector<std::shared_ptr<GraphNode>>& nodes);
    void CalculateRepulsiveForces(const std::vector<std::shared_ptr<GraphNode>>& nodes);
    void CalculateTemporalForces(const std::vector<std::shared_ptr<GraphNode>>& nodes);
    void ApplyForces(const std::vector<std::shared_ptr<GraphNode>>& nodes);
    void ConstrainToBounds(const std::shared_ptr<GraphNode>& node);
    float CalculateTotalEnergy(const std::vector<std::shared_ptr<GraphNode>>& nodes);
    void InitializeChronologicalPositions(const std::vector<std::shared_ptr<GraphNode>>& nodes, const ImVec2& canvas_center);
    std::vector<std::shared_ptr<GraphNode>> SortNodesByTimestamp(const std::vector<std::shared_ptr<GraphNode>>& nodes);
    std::vector<std::pair<std::shared_ptr<GraphNode>, std::shared_ptr<GraphNode>>> GetChronologicalNeighbors(const std::vector<std::shared_ptr<GraphNode>>& sorted_nodes);
};

#endif // FORCE_DIRECTED_LAYOUT_H