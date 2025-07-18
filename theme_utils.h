#pragma once

#include "extern/imgui/imgui.h" // For ImU32

// Forward declarations
enum class ThemeType;
class GuiInterface;

namespace ThemeUtils {

void applyDarkTheme();
void applyWhiteTheme();
void setTheme(GuiInterface& gui, ThemeType theme);

// Graph-specific theme colors
ImU32 GetThemeNodeColor(ThemeType theme);
ImU32 GetThemeNodeBorderColor(ThemeType theme);
ImU32 GetThemeNodeSelectedBorderColor(ThemeType theme);
ImU32 GetThemeEdgeColor(ThemeType theme);
ImU32 GetThemeBackgroundColor(ThemeType theme);
ImU32 GetThemeTextColor(ThemeType theme);
ImU32 GetThemeExpandCollapseIconColor(ThemeType theme);

} // namespace ThemeUtils