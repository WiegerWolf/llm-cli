#include <graph/render/graph_renderer.h>
#include <graph/utils/graph_drawing_utils.h>
#include <graph/render/camera_utils.h>

// This file is now a lightweight coordinator.
// - It includes the necessary headers.
// - It contains the main `RenderGraphView` function which is the entry point for the GUI.
// - It creates the `GraphEditor` instance and calls its methods.
//
// The actual implementation of the GraphEditor methods is now in:
// - `graph_renderer_core.cpp` (core logic, selection, popups)
// - `graph_drawing_utils.cpp` (low-level drawing helpers)
// - `camera_utils.cpp` (pan, zoom, coordinate transformations)