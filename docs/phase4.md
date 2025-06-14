#### Phase 4 – Tooling & CI Guardrails
1. clang-format enforcement  
   • Targets: all C/C++; new `.clang-format`.  
   • Operations: add LLVM style with ColumnLimit = 100; script `tools/check_format.sh`; GitHub Action `format.yml`.  
   • Acceptance: running script shows no diff; CI “Format” job passes.

2. LOC checker integration  
   • Target: scripts/loc_report.py.  
   • Operations: implement LOC counting (skip blanks/comments); exit non-zero on threshold breach; add workflow `loc.yml`.  
   • Acceptance: pushing a 510-line file fails CI; reverting passes.

3. Build & test matrix  
   • Target: `.github/workflows/build.yml`.  
   • Operations: build & test on ubuntu-latest, macos-latest, windows-latest in Release/Debug; invoke LOC checker & format checker.  
   • Acceptance: jobs green on main.

4. Pre-commit hooks  
   • Target: `.githooks/pre-commit`.  
   • Operations: run clang-format -n and loc_report.py; provide installation note in README.  
   • Acceptance: commits blocked if violations found.