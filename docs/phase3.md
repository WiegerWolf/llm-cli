#### Phase 3 – Iterative Segmentation
1. LOC re-audit  
   • Target: all src (exclude extern/).  
   • Operations: run `scripts/loc_report.py --limit 500`; if any file > 500 LOC, split into *_impl.cpp or *_helpers.cpp.  
   • Acceptance: Report ends “All files ≤ 500 LOC”.

2. Unit-test expansion  
   • New tests: db_message_store_test.cpp, layout_force_directed_test.cpp, camera_utils_test.cpp.  
   • Operations: add fixtures, link against gtest, update tests/CMakeLists.txt.  
   • Acceptance: `ctest -R "(db_|layout_|camera_)"` passes; coverage ≥ 80 % for new modules.

3. Continuous refinement  
   • Target: files touched > 3× in PR history.  
   • Operations: schedule further extraction or renaming to domain-specific modules.  
   • Acceptance: reviewer checklist signed; CI green.

---