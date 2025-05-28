#ifndef GRAPH_RENDERER_H
#define GRAPH_RENDERER_H

#include "extern/imgui/imgui.h"
#include "gui_interface/graph_types.h" // For GraphNode

// Forward declaration if GraphNode is complex and defined elsewhere,
// or include the necessary header if it's simple.
// Assuming GraphNode is defined in "gui_interface/graph_types.h"

void RenderGraphNode(ImDrawList* draw_list, const GraphNode& node, const ImVec2& view_offset, bool is_selected);
void RenderEdge(ImDrawList* draw_list, const GraphNode& parent_node, const GraphNode& child_node, const ImVec2& view_offset);

#endif // GRAPH_RENDERER_H