# System Patterns *Optional*

This file documents recurring patterns and standards used in the project.
It is optional, but recommended to be updated as the project evolves.
2025-05-04 01:43:08 - Log of updates made.
2025-05-04 01:44:13 - Added initial architectural patterns from README.md.

*

## Coding Patterns

*   

## Architectural Patterns

*   **Separation of Concerns:** Core logic (`chat_client`, `database`) is separated from UI (`cli_interface`, `gui_interface`).
*   **Interface Abstraction:** `ui_interface.h` provides a base class for different UIs.
*   **Modular Tools:** Tool implementations reside in `tools_impl/` and are registered centrally (`tools.cpp`).
*   **Build System:** CMake manages dependencies and build process (`CMakeLists.txt`, `build.sh`, `install.sh`).

## Testing Patterns

*