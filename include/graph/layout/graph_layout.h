#ifndef GRAPH_LAYOUT_H
#define GRAPH_LAYOUT_H

#include <vector>
#include <memory>
#include "extern/imgui/imgui.h"
#include <graph/layout/force_directed_layout.h>
#include <graph/layout/graph_layout_recursive.h>

struct GraphNode;

void ApplyForceDirectedLayout(const std::vector<std::shared_ptr<GraphNode>>& nodes,
                              const ImVec2& canvas_center,
                              const ForceDirectedLayout::LayoutParams& params = ForceDirectedLayout::LayoutParams());

#endif // GRAPH_LAYOUT_H