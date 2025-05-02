# Stage 5: Packaging, Installation & Refinement

## Goal

Finalize the build process for both CLI and GUI applications, update installation procedures, perform testing, and address any necessary refinements or bug fixes.

## Prerequisites

*   Completion of Stage 4 (Threading & Core Logic Integration).

## Steps & Checklist

*   [ ] **Refine Build System (`CMakeLists.txt`):**
    *   [ ] **Platform Checks:** Add checks for OS-specific dependencies or settings if needed (e.g., different libraries for Linux/macOS/Windows).
    *   [ ] **Resource Files:** If using external resources like fonts, ensure they are correctly handled (e.g., copied to build/install directories). Consider embedding fonts if feasible.
    *   [ ] **Installation Rules:**
        *   [ ] Verify `install(TARGETS llm-cli llm-gui ...)` correctly places executables in `${CMAKE_INSTALL_BINDIR}`.
        *   [ ] Add rules to install necessary runtime libraries if they are dynamically linked and not expected to be system-provided (e.g., GLFW libs if not linking statically or using system libs). Check ImGui backend dependencies.
        *   [ ] Add rules to install resource files (fonts, etc.) if applicable.
    *   [ ] **Optimization Flags:** Ensure appropriate compiler flags are set for Release builds (`-O2` or `-O3`).
*   [ ] **Update Installation Scripts/Documentation:**
    *   [ ] Update `install.sh` (or create platform-specific scripts) to correctly build and install both targets.
    *   [ ] Update `README.md` with instructions for building and running both `llm-cli` and `llm-gui`. Include dependencies required for the GUI version (GLFW, OpenGL drivers).
*   [ ] **Testing:**
    *   [ ] **CLI:** Run existing tests or manually test `llm-cli` to ensure no regressions were introduced.
    *   [ ] **GUI:**
        *   [ ] Manually test `llm-gui` on target platforms.
        *   [ ] Test basic chat functionality.
        *   [ ] Test all available tools via chat commands.
        *   [ ] Test edge cases (e.g., API errors, tool errors, long messages, window resizing).
        *   [ ] Test shutdown behavior.
*   [ ] **Code Refinement:**
    *   [ ] **Error Handling:** Review error handling in both GUI and worker threads. Ensure errors are reported appropriately to the user via `displayError`. Handle potential exceptions in the worker thread gracefully.
    *   [ ] **Resource Management:** Double-check resource management (memory, file handles, CURL handles, DB connections, ImGui/GLFW contexts).
    *   [ ] **UI Polish:** Improve ImGui layout, text wrapping, scrolling behavior, or add minor features (e.g., clear history button, copy text).
    *   [ ] **Code Cleanup:** Remove dead code, add comments where necessary, ensure consistent formatting.
*   [ ] **Documentation:**
    *   [ ] Update `README.md` with screenshots of the GUI (optional).
    *   [ ] Ensure all build/run instructions are clear and accurate.
