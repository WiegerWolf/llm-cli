# Source-Tree Restructure Plan

## Overview
This document outlines the proposed logical directory structure, migration mapping, and supporting guidelines to improve source-tree navigability while keeping each file under 500 LOC.

## Proposed Directory Tree
```
include/                       ← Public API headers (installable)
  core/
  gui/
    render/
    views/
  graph/
    layout/
    render/
    utils/
  db/
    drivers/
    stores/
  cli/
  utils/
  net/

src/                           ← Private .cpp + internal *.hpp
  core/
  gui/
    render/
    views/
  graph/
    layout/
    render/
    utils/
  db/
    drivers/
    stores/
  cli/
  utils/
  net/
  tools_impl/                  ← MCP helper tools (kept as-is)

resources/                     ← Fonts, images, etc.
tests/                         ← Mirrors include/ hierarchy
docs/
scripts/
extern/                        ← 3rd-party (read-only)
CMakeLists.txt                 ← Root; delegates with add_subdirectory()
```

## Header Placement Rules
1. Anything intended for consumption by **other targets** lives in `include/<domain>/`.
2. Internal headers (`*.hpp`) stay beside their implementation inside `src/`.
3. Public headers include **only** other public headers to avoid leaking internals.

## CMake Organisation
* Root `CMakeLists.txt` performs one `add_subdirectory(src/<domain>)` per leaf folder.  
* Each `src/<domain>/CMakeLists.txt` builds a `STATIC` (or `OBJECT`) library and links to its lower-layer siblings.  
* `tests/` is added after all libs; each test links to the logical unit it exercises.  
* Installation step: `install(DIRECTORY include/ DESTINATION include)`.

## Migration Mapping (Representative)
| Old path | → | New path |
| --- | --- | --- |
| `main_gui_core.cpp` | → | `src/core/main_gui_core.cpp` |
| `main_gui_views.cpp` | → | `src/gui/views/main_gui_views.cpp` |
| `main_gui_views.h` | → | `include/gui/main_gui_views.h` |
| `gui_interface/gui_interface.cpp` | → | `src/gui/gui_interface.cpp` |
| `gui_interface/gui_interface.h` | → | `include/gui/gui_interface.h` |
| `event_dispatch.cpp / .h` | → | `src/core/event_dispatch.cpp` / `include/core/event_dispatch.h` |
| `font_utils.cpp / .h` | → | `src/gui/render/font_utils.cpp` / `include/gui/font_utils.h` |
| `theme_utils.cpp / .h` | → | `src/gui/render/theme_utils.cpp` / `include/gui/theme_utils.h` |
| `camera_utils.cpp / .h` | → | `src/graph/utils/camera_utils.cpp` / `include/graph/camera_utils.h` |
| `graph_renderer_core.cpp` | → | `src/graph/render/graph_renderer_core.cpp` |
| `graph_renderer.h` | → | `include/graph/graph_renderer.h` |
| `graph_drawing_utils.cpp / .h` | → | `src/graph/render/graph_drawing_utils.cpp` / `include/graph/graph_drawing_utils.h` |
| `force_directed_layout.cpp / .h` | → | `src/graph/layout/force_directed_layout.cpp` / `include/graph/force_directed_layout.h` |
| `graph_layout_recursive.cpp / .h` | → | `src/graph/layout/graph_layout_recursive.cpp` / `include/graph/graph_layout_recursive.h` |
| `spatial_hash.cpp / .h` | → | `src/graph/utils/spatial_hash.cpp` / `include/graph/spatial_hash.h` |
| `graph_manager.cpp / .h` | → | `src/graph/graph_manager.cpp` / `include/graph/graph_manager.h` |
| `database.cpp / .h` | → | `src/db/drivers/database.cpp` / `include/db/database.h` |
| `sqlite_connection.cpp / .h` | → | `src/db/drivers/sqlite_connection.cpp` / `include/db/sqlite_connection.h` |
| `message_store.cpp / .h` | → | `src/db/stores/message_store.cpp` / `include/db/message_store.h` |
| `model_store.cpp / .h` | → | `src/db/stores/model_store.cpp` / `include/db/model_store.h` |
| `settings_store.cpp / .h` | → | `src/db/stores/settings_store.cpp` / `include/db/settings_store.h` |
| `chat_client_api.cpp / .h` | → | `src/net/chat_client_api.cpp` / `include/net/chat_client_api.h` |
| `chat_client_models.cpp / .h` | → | `src/net/chat_client_models.cpp` / `include/net/chat_client_models.h` |
| `cli_interface.cpp / .h` | → | `src/cli/cli_interface.cpp` / `include/cli/cli_interface.h` |
| `main_cli.cpp` | → | `src/cli/main_cli.cpp` |
| `tools_impl/*` | → | `src/tools_impl/*` |
| `tests/*` | → | `tests/` (paths updated to new headers) |

**Pattern rules**  
* `*_utils.*` not already mapped → `src/utils/` + `include/utils/`  
* File names starting with `graph_` but not renderer/layout → `src/graph/` + `include/graph/`  
* `*_store.*` → `src/db/stores/` + `include/db/`  
* `*_connection.*` → `src/db/drivers/` + `include/db/`  
* `*_layout.*` → `src/graph/layout/` + `include/graph/`  
* `*_renderer.*` → `src/graph/render/` + `include/graph/`  

## Rationale (Key Decisions)
* **core/** centralises bootstrap & cross-cutting services, limiting include churn.  
* **gui/** mirrors MVC; `render/` vs `views/` cleanly separates low-level ImGui helpers from high-level widget code.  
* **graph/** nested sub-areas (`layout`, `render`, `utils`) minimise cyclic dependencies.  
* **db/** splits `drivers` (low-level SQL) from `stores` (domain persistence) showing dependency direction.  
* **include/ vs src/** provides clear public/private split, easing packaging and speeding incremental builds.  
* Directory-local CMake targets yield fine-grained linkage and simpler dependency graphs.  

## Risks & Mitigations
| Risk | Mitigation |
| --- | --- |
| Broken include paths | Add transitional shim headers during phased move; remove after CI passes. |
| New cyclic dependencies | Enforce “upper layer includes lower” rule; run `include-what-you-use` in CI. |
| CMake target sprawl | Template a minimal `CMakeLists.txt` stub for each folder; code-review additions. |
| Large migration patch hides regressions | Migrate domain-by-domain; keep unit tests & LOC guard unchanged. |
| Developer confusion | Update `README.md`; generate Doxygen from `include/` to provide navigation map. |