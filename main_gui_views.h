#pragma once

// Forward declarations to avoid including heavy headers
class GraphManager;
class GuiInterface;

/*
 * @brief Renders all ImGui views for the application.
 *
 * This function encapsulates all the drawing logic, including the settings panel,
 * tab bar, and graph view. It is called once per frame from the main loop.
 *
 * @param gm  A reference to the GraphManager, providing access to graph data.
 * @param gui A reference to the GuiInterface, for interacting with GUI state.
 */
void drawAllViews(GraphManager& gm, GuiInterface& gui);