#include <graph/layout/graph_layout_recursive.h>
#include <gui/views/graph_types.h>

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

    if (level_x_offset.find(depth) == level_x_offset.end()) {
        level_x_offset[depth] = 0.0f; 
    }

    node->position.x = canvas_start_pos.x + level_x_offset[depth];
    node->position.y = canvas_start_pos.y + (float)depth * y_spacing;

    float node_width = (node->size.x > 0) ? node->size.x : 100.0f;
    level_x_offset[depth] += node_width + x_spacing;

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