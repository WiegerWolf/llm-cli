# Refactor Roadmap – Reduce Source Files to ≤ 500 LOC

## Objective
Bring every project-owned source file (excluding extern/ third-party code) under 500 lines of code while preserving behaviour and maintaining build stability.

## Current Metrics (sampling)
| File | LOC (approx.) |
| --- | --- |
| gui_interface/gui_interface.cpp | ≈ 1500 |
| main_gui.cpp | ≈ 1200 |
| graph_renderer.cpp | ≈ 850 |
| graph_layout.cpp | ≈ 650 |
| database.cpp | ≈ 730 |
| graph_manager.cpp | ≈ 360 |

## Partitioning Targets
### GUI Tier
* Keep `GuiInterface` as façade.
* Move rendering helpers, font handling, and theme utilities to:
  * `font_utils.cpp/.h`
  * `theme_utils.cpp/.h`
* Create event/scroll helpers in `event_dispatch.cpp`.

### Main Application Entry
* Split `main_gui.cpp` into:
  * `main_gui_core.cpp` – initialization & loop skeleton
  * `main_gui_views.cpp` – tab & widget drawing

### Graph Sub-system
* `graph_renderer.cpp` ➜
  * `graph_renderer_core.cpp`
  * `graph_drawing_utils.cpp`
  * `camera_utils.cpp`
* `graph_layout.cpp` ➜
  * `force_directed_layout.cpp`
  * `graph_layout_recursive.cpp`
  * `spatial_hash.cpp`

### Persistence Layer
* `database.cpp` ➜
  * `sqlite_connection.cpp`
  * `message_store.cpp`
  * `model_store.cpp`
  * `settings_store.cpp`

## Proposed File Map
```mermaid
graph TD
    GUI[gui_interface.cpp (core)]
    Fonts[font_utils.cpp]
    Theme[theme_utils.cpp]
    Events[event_dispatch.cpp]
    Main[main_gui_core.cpp]
    View[main_gui_views.cpp]
    DrawUtils[graph_drawing_utils.cpp]
    Camera[camera_utils.cpp]
    Renderer[graph_renderer_core.cpp]
    FLayout[force_directed_layout.cpp]
    RecLayout[graph_layout_recursive.cpp]
    SHash[spatial_hash.cpp]
    SQL[sqlite_connection.cpp]
    MsgStore[message_store.cpp]
    ModelStore[model_store.cpp]
    SettingsStore[settings_store.cpp]

    GUI --> Fonts & Theme & Events
    Main --> View & GUI
    Renderer --> DrawUtils & Camera
    FLayout --> SHash
    RecLayout --> SHash
    SQL --> MsgStore & ModelStore & SettingsStore
```

## Phased Execution
1. **Mechanical Extraction**
   * Create new files, cut-and-paste logic without changes.
   * Update `CMakeLists.txt`.
2. **API Tidying**
   * Introduce forward declarations, reduce includes.
   * Turn helper functions into `namespace detail`.
3. **Iterative Segmentation**
   * Re-measure LOC; split further if any file > 500.
   * Add unit tests for database stores and layouts.
4. **Tooling & CI Guardrails**
   * Add `clang-format` config.
   * Add `scripts/loc_report.py` to fail CI on > 500 LOC files.

## Risks & Mitigations
| Risk | Mitigation |
| --- | --- |
| Circular dependencies | Keep data-only structs in `graph_types.h`; follow arrow directions in map. |
| Build breakage | Commit each phase separately; run CI after each group. |
| Hidden static state | Audit extracted code; convert to class members where feasible. |

---

_Endorsed by team 2025-06-14_