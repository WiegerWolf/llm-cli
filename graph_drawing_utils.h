#ifndef GRAPH_DRAWING_UTILS_H
#define GRAPH_DRAWING_UTILS_H

#include "extern/imgui/imgui.h"

namespace GraphDraw {

// Low-level ImDrawList helpers
void AddTextTruncated(ImDrawList* draw_list, ImFont* font, float font_size, const ImVec2& pos, ImU32 col, const char* text_begin, const char* text_end, float wrap_width, const ImVec4* cpu_fine_clip_rect);
void RenderWrappedText(ImDrawList* draw_list, ImFont* font, float font_size, const ImVec2& pos, ImU32 col, const char* text, float wrap_width, float max_height, const ImVec4* cpu_fine_clip_rect);

} // namespace GraphDraw

#endif // GRAPH_DRAWING_UTILS_H