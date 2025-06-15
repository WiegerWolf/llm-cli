#include "gtest/gtest.h"
#include "camera_utils.h"
#include "gui_interface/graph_types.h" // For GraphViewState

TEST(CameraUtilsTest, PanAndZoomInvariants) {
    GraphViewState view_state;
    view_state.pan_offset = ImVec2(0, 0);
    view_state.zoom_scale = 1.0f;

    ImVec2 world_point(100, 200);
    ImVec2 canvas_pos(50, 50);

    // Initial state
    ImVec2 screen_point = app::graph::CameraUtils::WorldToScreen(world_point, view_state);
    ImVec2 world_point_rt = app::graph::CameraUtils::ScreenToWorld(screen_point, ImVec2(0,0), view_state);
    EXPECT_NEAR(world_point.x, world_point_rt.x, 1e-3);
    EXPECT_NEAR(world_point.y, world_point_rt.y, 1e-3);

    // Zoom
    view_state.zoom_scale = 2.0f;
    screen_point = app::graph::CameraUtils::WorldToScreen(world_point, view_state);
    world_point_rt = app::graph::CameraUtils::ScreenToWorld(screen_point, ImVec2(0,0), view_state);
    EXPECT_NEAR(world_point.x, world_point_rt.x, 1e-3);
    EXPECT_NEAR(world_point.y, world_point_rt.y, 1e-3);
    
    // Pan
    view_state.pan_offset = ImVec2(30, -40);
    screen_point = app::graph::CameraUtils::WorldToScreen(world_point, view_state);
    world_point_rt = app::graph::CameraUtils::ScreenToWorld(screen_point, ImVec2(0,0), view_state);
    EXPECT_NEAR(world_point.x, world_point_rt.x, 1e-3);
    EXPECT_NEAR(world_point.y, world_point_rt.y, 1e-3);
}