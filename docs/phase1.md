#### Phase 1 – Mechanical Extraction
1. GUI tier split  
   • Targets: gui_interface/gui_interface.cpp ⇒ new font_utils.{h,cpp}, theme_utils.{h,cpp}, event_dispatch.{h,cpp}  
   • Operations:  
     – Copy font loading, theme colors, scroll-callback code into new files untouched.  
     – In gui_interface.cpp remove copied blocks, add `#include` for new headers.  
     – Add new sources to target `gui` in CMakeLists.txt.  
   • Acceptance: `cmake --build .` succeeds; `gui_interface.cpp` ≤ 500 LOC; manual smoke-test window opens.

2. Main entry split  
   • Targets: main_gui.cpp ⇒ main_gui_core.cpp, main_gui_views.cpp  
   • Operations:  
     – Move GLFW/ImGui setup + main loop to main_gui_core.cpp.  
     – Move all ImGui drawing (settings, tab bar, graph view) to main_gui_views.cpp; expose `void drawAllViews(GraphManager&, GuiInterface&)`.  
     – Update executable target in CMakeLists.txt.  
   • Acceptance: Application functions identically; both new files ≤ 500 LOC; `test_chronological_layout` passes.

3. Renderer extraction  
   • Targets: graph_renderer.cpp ⇒ graph_renderer_core.cpp, graph_drawing_utils.{h,cpp}, camera_utils.{h,cpp}  
   • Operations:  
     – Move GraphEditor methods except helpers to *_core.cpp.  
     – Place AddTextTruncated, RenderWrappedText, triangle drawing etc. in graph_drawing_utils.  
     – Put World↔Screen, auto-pan easing, pan/zoom maths in camera_utils.  
     – Adjust includes in graph_manager.cpp and others; add new files to graph library in CMake.  
   • Acceptance: Build succeeds Release & Debug; rendered graph matches before; each new file ≤ 500 LOC.

4. Layout extraction  
   • Targets: graph_layout.cpp ⇒ force_directed_layout.{h,cpp}, graph_layout_recursive.{h,cpp}, spatial_hash.{h,cpp}  
   • Operations:  
     – Split ForceDirectedLayout class, legacy recursive layout, grid hashing utilities.  
     – Provide shared helpers (Distance, Normalize) in spatial_hash.  
     – Modify includes in graph_manager.cpp.  
   • Acceptance: All layout unit tests (`tests/force_convergence_test`) pass; new files ≤ 500 LOC.

5. Persistence extraction  
   • Targets: database.cpp ⇒ sqlite_connection.{h,cpp}, message_store.{h,cpp}, model_store.{h,cpp}, settings_store.{h,cpp}  
   • Operations:  
     – Keep only Impl ctor/dtor + exec in sqlite_connection.  
     – Migrate per-table CRUD to dedicated store files.  
     – Introduce database_fwd.h for forward decls; update includes in chat_client.cpp etc.  
     – Register new persistence library in CMake.  
   • Acceptance: All DB functions work; integration tests green; each new file ≤ 500 LOC.

---
