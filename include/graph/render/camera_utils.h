#ifndef CAMERA_UTILS_H
#define CAMERA_UTILS_H

#include "extern/imgui/imgui.h"

// Forward declarations
struct GraphViewState;

namespace app {
namespace graph {

/*
 * Utility helpers for camera transformations in the graph view.
 * All methods are static; an instance of CameraUtils is never created.
 */
namespace detail {
    // Easing functions for smooth camera animation
    namespace Easing {
        float EaseOutCubic(float t);
        float EaseInOutCubic(float t);
        float Lerp(float a, float b, float t);
        ImVec2 LerpVec2(const ImVec2& a, const ImVec2& b, float t);
    }
} // namespace detail

// Provide public alias so callers can still use CameraUtils::Easing unchanged
namespace Easing = detail::Easing;

class CameraUtils {
public:
    // Converts world coordinates to screen coordinates.
    static ImVec2 WorldToScreen(const ImVec2& world_pos, const GraphViewState& view_state);

    // Converts screen coordinates to world coordinates.
    static ImVec2 ScreenToWorld(const ImVec2& screen_pos_absolute,
                                const ImVec2& canvas_screen_pos_absolute,
                                const GraphViewState& view_state);
};

} // namespace graph
} // namespace app


#endif // CAMERA_UTILS_H