#include "graph_drawing_utils.h"
#include "extern/imgui/imgui_internal.h" // For ImGui::CalcTextSize, ImGui::PushClipRect, ImTextCharToUtf8
#include <string>
#include <cstring> // For strlen
#include <algorithm> // For std::min, std::max

namespace GraphDraw {

// Helper to add text with truncation using ImFont::CalcWordWrapPositionA
void AddTextTruncated(ImDrawList* draw_list, ImFont* font, float font_size, const ImVec2& pos, ImU32 col, const char* text_begin, const char* text_end, float wrap_width, const ImVec4* cpu_fine_clip_rect) {
    if (text_begin == text_end || font_size <= 0.0f || font == nullptr)
        return;

    if (wrap_width <= 0.0f) {
        draw_list->AddText(font, font_size, pos, col, text_begin, text_end, 0.0f, cpu_fine_clip_rect);
        return;
    }

    float scale = font->FontSize > 0 ? font_size / font->FontSize : 1.0f;
    const char* end_of_fit_text_ptr = font->CalcWordWrapPositionA(scale, text_begin, text_end, wrap_width);

    if (end_of_fit_text_ptr == text_begin) {
        const char* ellipsis = "...";
        ImVec2 ellipsis_size = font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, ellipsis);
        if (ellipsis_size.x <= wrap_width) {
            draw_list->AddText(font, font_size, pos, col, ellipsis, ellipsis + strlen(ellipsis), 0.0f, cpu_fine_clip_rect);
        }
        return;
    }

    if (end_of_fit_text_ptr == text_end) {
        draw_list->AddText(font, font_size, pos, col, text_begin, text_end, 0.0f, cpu_fine_clip_rect);
        return;
    }

    const char* ellipsis = "...";
    ImVec2 ellipsis_size = font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, ellipsis);
    float width_for_text_before_ellipsis = wrap_width - ellipsis_size.x;
    std::string final_text_str;

    if (width_for_text_before_ellipsis > 0) {
        const char* end_of_text_part_ptr = font->CalcWordWrapPositionA(scale, text_begin, text_end, width_for_text_before_ellipsis);
        if (end_of_text_part_ptr > text_begin) {
            final_text_str.assign(text_begin, end_of_text_part_ptr);
            final_text_str += ellipsis;
        } else {
            if (ellipsis_size.x <= wrap_width) {
                final_text_str = ellipsis;
            }
        }
    } else {
        if (ellipsis_size.x <= wrap_width) {
            final_text_str = ellipsis;
        }
    }

    if (!final_text_str.empty()) {
        draw_list->AddText(font, font_size, pos, col, final_text_str.c_str(), final_text_str.c_str() + final_text_str.length(), 0.0f, cpu_fine_clip_rect);
    }
}

// Helper function to render wrapped text within a bounded area
void RenderWrappedText(ImDrawList* draw_list, ImFont* font, float font_size, const ImVec2& pos, ImU32 col, const char* text, float wrap_width, float max_height, const ImVec4* cpu_fine_clip_rect) {
    if (!text || font_size <= 0.0f || font == nullptr || wrap_width <= 10.0f) // Ensure minimum wrap width
        return;

    // Use ImGui's built-in text wrapping which is more reliable
    ImVec2 text_size = ImGui::CalcTextSize(text, nullptr, false, wrap_width);
    
    // If the text fits in one line and within bounds, render it directly
    if (text_size.y <= max_height) {
        // Force immediate rendering with explicit parameters to avoid caching issues
        // Use AddText with explicit text_end parameter to ensure fresh rendering
        const char* text_end = text + strlen(text);
        draw_list->AddText(font, font_size, pos, col, text, text_end, wrap_width, cpu_fine_clip_rect);
        return;
    }
    
    // For multi-line text that exceeds max_height, we need to truncate
    float scale = font->FontSize > 0 ? font_size / font->FontSize : 1.0f;
    float line_height = font_size * 1.2f;
    const int max_lines = std::max(1, (int)(max_height / line_height));
    
    const char* text_ptr = text;
    const char* text_end = text + strlen(text);
    ImVec2 current_pos = pos;
    int line_count = 0;
    
    while (text_ptr < text_end && line_count < max_lines) {
        // Handle explicit newlines first
        const char* newline_pos = text_ptr;
        while (newline_pos < text_end && *newline_pos != '\n') {
            newline_pos++;
        }
        
        // Find the end of the current line that fits within wrap_width
        const char* line_end = font->CalcWordWrapPositionA(scale, text_ptr, newline_pos, wrap_width);
        
        if (line_end == text_ptr && text_ptr < text_end) {
            // If we can't fit even one character, force at least one character
            line_end = text_ptr + 1;
            if (line_end > text_end) line_end = text_end;
        }
        
        // Check if this is the last line we can fit and there's more text
        bool is_last_line = (line_count == max_lines - 1) && (line_end < text_end || (text_ptr < text_end && *text_ptr == '\n'));
        
        if (is_last_line) {
            // Reserve space for ellipsis
            const char* ellipsis = "...";
            ImVec2 ellipsis_size = font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, ellipsis);
            float available_width = wrap_width - ellipsis_size.x;
            
            if (available_width > 0) {
                line_end = font->CalcWordWrapPositionA(scale, text_ptr, newline_pos, available_width);
                if (line_end == text_ptr && text_ptr < text_end) {
                    line_end = text_ptr + 1;
                    if (line_end > text_end) line_end = text_end;
                }
            }
            
            // Render the truncated line with explicit parameters to force fresh rendering
            if (line_end > text_ptr) {
                draw_list->AddText(font, font_size, current_pos, col, text_ptr, line_end, 0.0f, cpu_fine_clip_rect);
            }
            
            // Add ellipsis with explicit text_end to force fresh rendering
            ImVec2 ellipsis_pos = ImVec2(current_pos.x + font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, text_ptr, line_end).x, current_pos.y);
            const char* ellipsis_end = ellipsis + strlen(ellipsis);
            draw_list->AddText(font, font_size, ellipsis_pos, col, ellipsis, ellipsis_end, 0.0f, cpu_fine_clip_rect);
            break;
        } else {
            // Render this line normally with explicit text_end to force fresh rendering
            if (line_end > text_ptr) {
                draw_list->AddText(font, font_size, current_pos, col, text_ptr, line_end, 0.0f, cpu_fine_clip_rect);
            }
        }
        
        // Move to next line
        current_pos.y += line_height;
        line_count++;
        text_ptr = line_end;
        
        // Skip whitespace at the beginning of the next line (but not newlines)
        while (text_ptr < text_end && (*text_ptr == ' ' || *text_ptr == '\t')) {
            text_ptr++;
        }
        
        // Handle explicit newlines
        if (text_ptr < text_end && *text_ptr == '\n') {
            text_ptr++;
            // Skip additional whitespace after newline
            while (text_ptr < text_end && (*text_ptr == ' ' || *text_ptr == '\t')) {
                text_ptr++;
            }
        }
    }
}

} // namespace GraphDraw