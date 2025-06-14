#ifndef GRAPH_LAYOUT_RECURSIVE_H
#define GRAPH_LAYOUT_RECURSIVE_H

#include <map>
#include "extern/imgui/imgui.h"

// Forward declarations
struct GraphNode;

void CalculateNodePositionsRecursive(
    GraphNode* node,
    ImVec2 current_pos,
    float x_spacing, 
    float y_spacing, 
    int depth, 
    std::map<int, float>& level_x_offset, 
    const ImVec2& canvas_start_pos
);

#endif // GRAPH_LAYOUT_RECURSIVE_H