#include "graph_layout.h"
#include "force_directed_layout.h"
#include "graph_layout_recursive.h"

// This function remains as a convenience wrapper.
void ApplyForceDirectedLayout(const std::vector<std::shared_ptr<GraphNode>>& nodes,
                              const ImVec2& canvas_center,
                              const ForceDirectedLayout::LayoutParams& params) {
    ForceDirectedLayout layout(params);
    layout.ComputeLayout(nodes, canvas_center);
}