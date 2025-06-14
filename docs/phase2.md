#### Phase 2 – API Tidying
1. Header decoupling  
   • Targets: all new headers.  
   • Operations: Replace heavy includes with forward declarations; move internal helpers to `namespace detail`.  
   • Acceptance: Run `include-what-you-use` → ≤ 3 warnings/file; build passes.

2. Public interface polish  
   • Targets: font_utils, camera_utils, stores.  
   • Operations: Wrap helpers inside `app::gui`, `app::graph`, `app::db` namespaces; convert free functions to `class` statics where appropriate.  
   • Acceptance: Doxygen run shows zero “undocumented public member” warnings.

3. Include-graph validation  
   • Targets: renderer, layout, persistence cpp files.  
   • Operations: Run tools/include_graph.py and eliminate reverse edges.  
   • Acceptance: Script output “0 reverse edges”.

---