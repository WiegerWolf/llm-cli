#pragma once

class GuiInterface;
class Database;

namespace FontUtils {

void loadFonts(GuiInterface& gui, float size);
void rebuildFontAtlas(GuiInterface& gui, float new_size);
void changeFontSize(GuiInterface& gui, float delta, Database& db);
void resetFontSize(GuiInterface& gui, Database& db);
void setInitialFontSize(GuiInterface& gui, float size);

} // namespace FontUtils