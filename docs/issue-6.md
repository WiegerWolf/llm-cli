# Plan to Implement UTF-8/Unicode Support (Issue #6)

This document outlines the steps to fix the character encoding issues in the LLM Client GUI, ensuring proper display of characters like apostrophes and support for a wider range of symbols and languages. The root cause appears to be the use of the default ImGui font, which has limited Unicode glyph coverage.

## Analysis Summary

*   **Font Loading:** `gui_interface/gui_interface.cpp` currently uses the default ImGui font (`io.Fonts->AddFontDefault()`), which lacks glyphs for many non-ASCII characters. Custom font loading code is commented out.
*   **String Handling:** `std::string` is used consistently in `chat_client.cpp` and `gui_interface.cpp` for storing and transferring message content (user input, API responses). Assuming the API provides UTF-8, this data should be preserved correctly up to the rendering point.
*   **Rendering:** ImGui widgets (`ImGui::TextWrapped`, `ImGui::InputText`) rely on the loaded font atlas to render characters. Without the necessary glyphs in the font, unsupported characters are displayed as placeholders (like '?').

## Implementation Plan

-   [ ] **1. Obtain a Suitable Font:**
    *   Select a TrueType Font (`.ttf`) file with broad Unicode character support. Options include:
        *   Noto Sans (Recommended for wide coverage)
        *   DejaVu Sans
        *   ProggyClean.ttf (Bundled with ImGui, but check coverage for needed characters)
        *   A system font (less portable)
    *   Add the chosen `.ttf` file to the project repository (e.g., in a `resources/` or `assets/` directory) to ensure it's bundled with the application.

-   [ ] **2. Modify `gui_interface.cpp` (`GuiInterface::initialize`):**
    *   Locate the commented-out "Load Fonts" section (around lines 110-114).
    *   Get the `ImGuiIO& io = ImGui::GetIO();` object.
    *   Instead of `io.Fonts->AddFontDefault()`, use `io.Fonts->AddFontFromFileTTF()`:
        ```cpp
        // Example using Noto Sans placed in a 'resources' directory
        const char* font_path = "resources/NotoSans-Regular.ttf"; // Adjust path as needed
        float font_size = 16.0f; // Adjust size as needed

        // Get default glyph ranges + Latin Extended for broader European language support
        // Consider adding other ranges if specific language support (Cyrillic, Greek, CJK etc.) is required.
        ImFontConfig font_cfg;
        font_cfg.OversampleH = 2; // Improve rendering quality
        font_cfg.OversampleV = 1;
        font_cfg.PixelSnapH = true;

        // Load default ranges first (ASCII, basic Latin)
        io.Fonts->AddFontFromFileTTF(font_path, font_size, &font_cfg, io.Fonts->GetGlyphRangesDefault());

        // Merge additional ranges (e.g., Latin Extended, Cyrillic, Greek)
        // Use AddFontFromFileTTF again with MergeMode = true for subsequent ranges
        // Or prepare merged ranges manually if needed. A simpler start:
        static const ImWchar extended_ranges[] =
        {
            0x0020, 0x00FF, // Basic Latin + Latin Supplement
            0x0100, 0x017F, // Latin Extended-A
            0x0180, 0x024F, // Latin Extended-B
            // Add more ranges here if needed, e.g., Cyrillic: 0x0400, 0x052F,
            // Greek: 0x0370, 0x03FF,
            0, // Null terminator
        };
        font_cfg.MergeMode = true; // Merge new glyphs into the default font
        io.Fonts->AddFontFromFileTTF(font_path, font_size, &font_cfg, extended_ranges);
        font_cfg.MergeMode = false; // Reset merge mode

        // IMPORTANT: Build the font atlas AFTER adding all fonts/ranges
        io.Fonts->Build();
        ```
    *   Ensure `io.Fonts->Build()` is called *after* all `AddFontFromFileTTF` calls.

-   [ ] **3. Verify String Encoding Consistency:**
    *   Briefly review `curl_utils.h`'s `WriteCallback` to ensure it appends data directly to `std::string` without modification. (Seems okay based on current understanding).
    *   Confirm that `nlohmann::json` handles UTF-8 strings correctly during parsing and serialization. (Standard behavior, likely okay).
    *   Ensure no intermediate steps are accidentally truncating or misinterpreting UTF-8 byte sequences (e.g., treating them as single-byte characters).

-   [ ] **4. Test Thoroughly:**
    *   Run the application after implementing the changes.
    *   Test with input containing characters previously causing issues (e.g., `it's`, `don't`).
    *   Test with other non-ASCII characters (e.g., `€`, `£`, `é`, `ü`, `ñ`, potentially some Cyrillic/Greek if ranges were added).
    *   Verify correct rendering in both the chat history output area and the text input field.
    *   Ensure API responses containing these characters are displayed correctly.