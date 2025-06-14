#include "font_utils.h"
#include "gui_interface/gui_interface.h"
#include "database.h"
#include "resources/noto_sans_font.h"
#include <imgui.h>
#include <cmath>
#include <backends/imgui_impl_opengl3.h>
#include <algorithm> // For std::clamp

namespace FontUtils {

// Private helper to load fonts without exposing it in the header
void loadFonts(GuiInterface& gui, float size) {
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();

    float clamped_size = std::clamp(size, 8.0f, 72.0f);

    ImFontConfig font_config;
    font_config.FontDataOwnedByAtlas = false;
    gui.main_font = io.Fonts->AddFontFromMemoryTTF((void*)resources_NotoSans_Regular_ttf, sizeof(resources_NotoSans_Regular_ttf), clamped_size, &font_config);
    gui.small_font = io.Fonts->AddFontFromMemoryTTF((void*)resources_NotoSans_Regular_ttf, sizeof(resources_NotoSans_Regular_ttf), 12.0f, &font_config);

    if (!gui.main_font || !gui.small_font) {
        fprintf(stderr, "Failed to load fonts.\n");
    }
}

void rebuildFontAtlas(GuiInterface& gui, float new_size) {
    gui.current_font_size = new_size; // Set a temporary size
    loadFonts(gui, new_size);
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Build();
    ImGui_ImplOpenGL3_CreateFontsTexture();
    gui.font_rebuild_requested = false;
}

void changeFontSize(GuiInterface& gui, float delta, Database& db) {
    constexpr float min_font_size = 8.0f;
    constexpr float max_font_size = 72.0f;
    
    float new_size = std::clamp(gui.getCurrentFontSize() + delta, min_font_size, max_font_size);

    if (fabs(new_size - gui.getCurrentFontSize()) > 0.1f) {
        gui.requested_font_size = new_size;
        gui.font_rebuild_requested = true;
        db.saveSetting("font_size", std::to_string(new_size));
    }
}

void resetFontSize(GuiInterface& gui, Database& db) {
    constexpr float default_font_size = 18.0f;
    if (fabs(gui.getCurrentFontSize() - default_font_size) > 0.1f) {
        gui.requested_font_size = default_font_size;
        gui.font_rebuild_requested = true;
        db.saveSetting("font_size", std::to_string(default_font_size));
    }
}

void setInitialFontSize(GuiInterface& gui, float size) {
    constexpr float min_font_size = 8.0f;
    constexpr float max_font_size = 72.0f;
    gui.current_font_size = std::clamp(size, min_font_size, max_font_size);
}

} // namespace FontUtils