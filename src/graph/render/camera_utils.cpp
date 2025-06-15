#include <graph/render/camera_utils.h>
#include <gui/views/graph_types.h> // full definition for GraphViewState
#include <cmath> // For powf

namespace app {
namespace graph {

namespace detail { namespace Easing {
    // Ease-out cubic function for natural deceleration
    float EaseOutCubic(float t) {
        return 1.0f - powf(1.0f - t, 3.0f);
    }
    
    // Ease-in-out cubic function for smooth start and end
    float EaseInOutCubic(float t) {
        return t < 0.5f ? 4.0f * t * t * t : 1.0f - powf(-2.0f * t + 2.0f, 3.0f) / 2.0f;
    }
    
    // Linear interpolation between two values
    float Lerp(float a, float b, float t) {
        return a + t * (b - a);
    }
    
    // Linear interpolation between two ImVec2 values
    ImVec2 LerpVec2(const ImVec2& a, const ImVec2& b, float t) {
        return ImVec2(Lerp(a.x, b.x, t), Lerp(a.y, b.y, t));
    }
} // namespace Easing
} // namespace detail

ImVec2 CameraUtils::WorldToScreen(const ImVec2& world_pos, const GraphViewState& view_state) {
    float screen_x = (world_pos.x * view_state.zoom_scale) + view_state.pan_offset.x;
    float screen_y = (world_pos.y * view_state.zoom_scale) + view_state.pan_offset.y;
    return ImVec2(screen_x, screen_y);
}

ImVec2 CameraUtils::ScreenToWorld(const ImVec2& screen_pos, const ImVec2& canvas_pos, const GraphViewState& view_state) {
    if (view_state.zoom_scale == 0.0f) return ImVec2(0,0);
    float world_x = (screen_pos.x - view_state.pan_offset.x) / view_state.zoom_scale;
    float world_y = (screen_pos.y - view_state.pan_offset.y) / view_state.zoom_scale;
    return ImVec2(world_x, world_y);
}

} // namespace graph
} // namespace app