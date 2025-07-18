# Reviewer Checklist for Refactoring PRs

Before approving a pull request that involves extracting or renaming modules, please ensure the following items have been addressed:

- [ ] **File Map Updated:** The file map in `docs/refactor_plan.md` has been updated if any modules were moved or renamed.
- [ ] **CI Passes:** All continuous integration checks, including `loc_report` and unit tests, are passing.
- [ ] **Hot-Spot Files Considered:** Files touched more than 3 times have been considered for segmentation. (See output of `scripts/refine_candidates.py`).
- [ ] **No New Circular Dependencies:** The changes do not introduce any new circular dependencies between modules.