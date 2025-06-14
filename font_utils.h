#pragma once

// Forward declarations
class GuiInterface;
class Database;

namespace app {
namespace gui {

/*
 * Utility class for font-related GUI helpers.
 * All methods are static; an instance of FontUtils is never created.
 */
class FontUtils {
public:
    // Rebuilds the ImGui font atlas at a new size.
    static void rebuildFontAtlas(GuiInterface& gui, float new_size);

    // Adjust font size by delta, persisting the change to the DB.
    static void changeFontSize(GuiInterface& gui, float delta, Database& db);

    // Reset font size to the default, persisting the change to the DB.
    static void resetFontSize(GuiInterface& gui, Database& db);

    // Sets the initial font size without rebuilding the atlas.
    static void setInitialFontSize(GuiInterface& gui, float size);

private:
    // Internal helper for (re)loading fonts into the atlas.
    static void loadFonts(GuiInterface& gui, float size);
};

} // namespace gui
} // namespace app
