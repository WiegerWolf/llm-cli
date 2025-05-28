#include "graph_renderer.h"
#include "extern/imgui/imgui_internal.h" // For ImGui::CalcTextSize, ImGui::PushClipRect
#include <string> // For std::string, std::to_string
#include <algorithm> // For std::min

// Helper to add text with truncation
static void AddTextTruncated(ImDrawList* draw_list, ImFont* font, float font_size, const ImVec2& pos, ImU32 col, const char* text_begin, const char* text_end, float wrap_width, const ImVec4* cpu_fine_clip_rect) {
    if (text_begin == text_end)
        return;

    ImVec2 text_size = ImGui::CalcTextSize(text_begin, text_end, false, wrap_width);

    if (wrap_width > 0 && text_size.x > wrap_width) {
        // Simple truncation: find the last character that fits
        const char* truncated_end = text_begin;
        float current_width = 0.0f;
        const char* ellipsis = "...";
        ImVec2 ellipsis_size = ImGui::CalcTextSize(ellipsis, ellipsis + 3, false, 0.0f);

        for (const char* s = text_begin; s < text_end; ++s) {
            unsigned int c = (unsigned int)*s;
            if (c < 0x80) { // Simple ASCII, advance 1 byte
                 current_width += font->GetCharAdvance((ImWchar)c);
            } else { // UTF-8, more complex, approximate or use more robust library
                // For simplicity, let's assume fixed width for non-ASCII or use a simpler check
                // This is a common simplification, proper UTF-8 truncation is complex.
                // A more robust solution would involve iterating glyphs.
                ImVec2 char_size = ImGui::CalcTextSize(s, s + 1, false, 0.0f);
                current_width += char_size.x;

            }
            if (current_width + ellipsis_size.x > wrap_width) {
                break;
            }
            truncated_end = s + 1; // Point to next char, so current char is included
        }
        
        std::string temp_text(text_begin, truncated_end);
        if (truncated_end < text_end) {
            temp_text += ellipsis;
        }
        draw_list->AddText(font, font_size, pos, col, temp_text.c_str(), temp_text.c_str() + temp_text.length(), wrap_width, cpu_fine_clip_rect);

    } else {
        draw_list->AddText(font, font_size, pos, col, text_begin, text_end, wrap_width, cpu_fine_clip_rect);
    }
}


void RenderGraphNode(ImDrawList* draw_list, const GraphNode& node, const ImVec2& view_offset, bool is_selected) {
    ImVec2 screen_pos = ImVec2(node.position.x + view_offset.x, node.position.y + view_offset.y);
    ImVec2 node_end_pos = ImVec2(screen_pos.x + node.size.x, screen_pos.y + node.size.y);

    // Node Body
    ImU32 bg_color = IM_COL32(50, 50, 50, 255); // Default background
    ImU32 border_color = IM_COL32(100, 100, 100, 255); // Default border
    float border_thickness = 1.0f;

    if (is_selected) {
        border_color = IM_COL32(255, 255, 0, 255); // Yellow border for selected
        border_thickness = 2.0f;
    }

    draw_list->AddRectFilled(screen_pos, node_end_pos, bg_color, 4.0f); // Rounded corners
    draw_list->AddRect(screen_pos, node_end_pos, border_color, 4.0f, 0, border_thickness);

    // Node Content
    float padding = 5.0f;
    ImVec2 text_pos = ImVec2(screen_pos.x + padding, screen_pos.y + padding);
    ImVec2 content_size = ImVec2(node.size.x - 2 * padding, node.size.y - 2 * padding);
    
    // Expansion Indicator - adjust content_size if indicator is present
    float indicator_size = 10.0f;
    float indicator_padding = 5.0f;
    bool has_children = !node.children.empty(); // Check the children vector

    if (has_children) {
        content_size.x -= (indicator_size + indicator_padding); // Make space for indicator on the right
    }

    ImVec4 clip_rect_vec4(screen_pos.x + padding, screen_pos.y + padding, screen_pos.x + padding + content_size.x, screen_pos.y + padding + content_size.y);
    draw_list->PushClipRect(ImVec2(clip_rect_vec4.x, clip_rect_vec4.y), ImVec2(clip_rect_vec4.z, clip_rect_vec4.w), true);

    const char* text_to_display = node.message_data.content.c_str();
    if (node.message_data.content.empty()) {
        text_to_display = "[Empty Node]";
    }
    
    // Use helper for truncated text
    AddTextTruncated(draw_list, ImGui::GetFont(), ImGui::GetFontSize(), text_pos, IM_COL32(255, 255, 255, 255), text_to_display, text_to_display + strlen(text_to_display), content_size.x, &clip_rect_vec4);

    draw_list->PopClipRect();

    // Expansion Indicator
    if (has_children) {
        ImVec2 indicator_pos_center = ImVec2(node_end_pos.x - padding - indicator_size / 2.0f, screen_pos.y + node.size.y / 2.0f);
        ImU32 indicator_color = IM_COL32(200, 200, 200, 255);

        if (node.is_expanded) { // Draw a '-' or downward triangle
            draw_list->AddTriangleFilled(
                ImVec2(indicator_pos_center.x - indicator_size / 2.0f, indicator_pos_center.y - indicator_size / 3.0f),
                ImVec2(indicator_pos_center.x + indicator_size / 2.0f, indicator_pos_center.y - indicator_size / 3.0f),
                ImVec2(indicator_pos_center.x, indicator_pos_center.y + indicator_size * 2.0f / 3.0f),
                indicator_color);
        } else { // Draw a '+' or rightward triangle
            draw_list->AddTriangleFilled(
                ImVec2(indicator_pos_center.x - indicator_size / 3.0f, indicator_pos_center.y - indicator_size / 2.0f),
                ImVec2(indicator_pos_center.x - indicator_size / 3.0f, indicator_pos_center.y + indicator_size / 2.0f),
                ImVec2(indicator_pos_center.x + indicator_size * 2.0f / 3.0f, indicator_pos_center.y),
                indicator_color);
        }
    }
}

void RenderEdge(ImDrawList* draw_list, const GraphNode& parent_node, const GraphNode& child_node, const ImVec2& view_offset) {
    // Calculate connection points (center of bottom edge of parent, center of top edge of child)
    ImVec2 parent_screen_pos = ImVec2(parent_node.position.x + view_offset.x, parent_node.position.y + view_offset.y);
    ImVec2 child_screen_pos = ImVec2(child_node.position.x + view_offset.x, child_node.position.y + view_offset.y);

    ImVec2 start_point = ImVec2(parent_screen_pos.x + parent_node.size.x / 2.0f, parent_screen_pos.y + parent_node.size.y);
    ImVec2 end_point = ImVec2(child_screen_pos.x + child_node.size.x / 2.0f, child_screen_pos.y);

    // Draw line
    draw_list->AddLine(start_point, end_point, IM_COL32(150, 150, 150, 255), 1.5f);
    
    // Optional: Arrowhead
    float arrow_size = 8.0f;
    ImVec2 dir = ImVec2(end_point.x - start_point.x, end_point.y - start_point.y);
    float len = sqrtf(dir.x*dir.x + dir.y*dir.y);
    if (len > 0) {
        dir.x /= len;
        dir.y /= len;
        ImVec2 p1 = ImVec2(end_point.x - dir.x * arrow_size - dir.y * arrow_size / 2.0f, end_point.y - dir.y * arrow_size + dir.x * arrow_size / 2.0f);
        ImVec2 p2 = ImVec2(end_point.x - dir.x * arrow_size + dir.y * arrow_size / 2.0f, end_point.y - dir.y * arrow_size - dir.x * arrow_size / 2.0f);
        draw_list->AddTriangleFilled(end_point, p1, p2, IM_COL32(150, 150, 150, 255));
    }
}