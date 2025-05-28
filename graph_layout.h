#ifndef GRAPH_LAYOUT_H
#define GRAPH_LAYOUT_H

#include <vector>
#include <map>
#include <string> // Included for std::string if GraphNode uses it, otherwise can be removed.
#include "extern/imgui/imgui.h"
#include "gui_interface/graph_types.h" // Assuming GraphNode is defined here

// Forward declaration if GraphNode is not fully defined via graph_types.h or for header hygiene
// struct GraphNode; 

/**
 * @brief Recursively calculates the positions of graph nodes using a simple top-down tree layout.
 * 
 * @param node The current node to process.
 * @param current_pos Base position for the current node (primarily for the first node at each level).
 *                    This parameter seems less used if level_x_offset and canvas_start_pos dictate absolute positions.
 *                    The problem description for CalculateNodePositionsRecursive logic uses canvas_start_pos and level_x_offset for x,
 *                    and canvas_start_pos and depth * y_spacing for y.
 * @param x_spacing Horizontal space between sibling nodes.
 * @param y_spacing Vertical space between parent and child levels.
 * @param depth Current depth of the node in the tree.
 * @param level_x_offset A map where level_x_offset[depth] stores the next available x-coordinate 
 *                       for placing a node at that depth. This is updated by the function.
 * @param canvas_start_pos The top-left starting point for the entire graph layout on the canvas.
 */
void CalculateNodePositionsRecursive(
    GraphNode* node, 
    ImVec2 current_pos, // This might be redundant if using canvas_start_pos and level_x_offset for absolute positioning
    float x_spacing, 
    float y_spacing, 
    int depth, 
    std::map<int, float>& level_x_offset, 
    const ImVec2& canvas_start_pos
);

#endif // GRAPH_LAYOUT_H