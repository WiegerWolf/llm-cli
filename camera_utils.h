#ifndef CAMERA_UTILS_H
#define CAMERA_UTILS_H

#include "extern/imgui/imgui.h"
#include "gui_interface/graph_types.h" // For GraphViewState

namespace CameraUtils {

// Easing functions for smooth camera animation
namespace Easing {
    float EaseOutCubic(float t);
    float EaseInOutCubic(float t);
    float Lerp(float a, float b, float t);
    ImVec2 LerpVec2(const ImVec2& a, const ImVec2& b, float t);
}

ImVec2 WorldToScreen(const ImVec2& world_pos, const GraphViewState& view_state);
ImVec2 ScreenToWorld(const ImVec2& screen_pos_absolute, const ImVec2& canvas_screen_pos_absolute, const GraphViewState& view_state);

} // namespace CameraUtils

#endif // CAMERA_UTILS_H