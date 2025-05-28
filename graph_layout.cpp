#include "graph_layout.h"
#include <algorithm> // For std::max if needed, though not directly in this function

// Assuming GraphNode is defined in "gui_interface/graph_types.h" and includes:
// struct GraphNode {
//     ImVec2 position;
//     ImVec2 size; // Assumed to be pre-determined
//     bool is_expanded;
//     std::vector<GraphNode*> children;
//     // other members
// };

void CalculateNodePositionsRecursive(
    GraphNode* node, 
    ImVec2 current_pos, // As noted in .h, this might be less relevant if using absolute positioning
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
    // If not, it means this is the first node being placed at this depth (or the first in a new branch at this depth).
    // Initialize it to 0, as positions are relative to canvas_start_pos.x.
    if (level_x_offset.find(depth) == level_x_offset.end()) {
        level_x_offset[depth] = 0.0f; 
    }

    // Set node's position based on canvas_start_pos, level_x_offset, depth, and y_spacing
    // As per docs/step-7-layout-performance.md:
    // - node->position.x = canvas_start_pos.x + level_x_offset[depth]
    // - node->position.y = canvas_start_pos.y + depth * y_spacing
    node->position.x = canvas_start_pos.x + level_x_offset[depth];
    node->position.y = canvas_start_pos.y + (float)depth * y_spacing;

    // Update the x-offset for the current level to position the next sibling
    // As per docs/step-7-layout-performance.md:
    // - level_x_offset[depth] += node->size.x + x_spacing
    // Ensure node->size is valid (e.g., width > 0)
    float node_width = (node->size.x > 0) ? node->size.x : 100.0f; // Default width if size.x is zero or invalid
    level_x_offset[depth] += node_width + x_spacing;

    // Recursively call for children if the node is expanded and has children
    if (node->is_expanded && !node->children.empty()) {
        // The `current_pos` for children could be node->position, but the logic
        // primarily relies on `level_x_offset` for the children's depth.
        // The problem description states: "The current_pos for the child's recursive call 
        // can be node->position or canvas_start_pos depending on how level_x_offset is managed...
        // The key is that level_x_offset at depth + 1 correctly places the child."
        // We will pass canvas_start_pos as current_pos for consistency, as the absolute positioning
        // is handled by canvas_start_pos and the updated level_x_offset for the new depth.
        
        // Store the starting x_offset for the children's level before processing them.
        // This is important if we want children to be laid out starting from a common x-point
        // relative to the parent, or just continue the global level_x_offset.
        // The current simple left-to-right approach uses the global level_x_offset.
        
        for (GraphNode* child : node->children) {
            if (child) {
                // The child's depth will be depth + 1.
                // The current_pos for the child's recursive call is not strictly used for x,y setting
                // if level_x_offset and depth * y_spacing are used directly.
                // Passing canvas_start_pos to maintain consistency with how root nodes might be called.
                CalculateNodePositionsRecursive(
                    child, 
                    canvas_start_pos, // Or node->position, but logic uses canvas_start_pos + offsets
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