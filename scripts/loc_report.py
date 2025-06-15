#!/usr/bin/env python3
"""
Performs a line-of-code (LOC) audit on the project.

This script recursively walks the workspace, counts non-blank lines of code
in C++ source files, and reports any files exceeding a specified limit.
"""

import os
import argparse
from pathlib import Path

# Define the target file extensions
SOURCE_EXTENSIONS = {'.cpp', '.c', '.cc', '.h', '.hpp'}

# Define directories to exclude from the scan
EXCLUDE_DIRS = {'extern', 'build'}


def count_loc(file_path):
    """
    Counts the physical, non-blank lines in a given file.
    Comment lines are included in the count.
    """
    try:
        with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
            return sum(1 for line in f if line.strip())
    except IOError:
        return 0


def gather_source_files(root_dir):
    """
    Recursively finds all source files in a directory, respecting exclusions.
    """
    source_files = []
    for root, dirs, files in os.walk(root_dir, topdown=True):
        # Prune the search by removing excluded directories from the list
        # that os.walk() will descend into.
        dirs[:] = [d for d in dirs if d not in EXCLUDE_DIRS]

        for file in files:
            if Path(file).suffix in SOURCE_EXTENSIONS:
                file_path = os.path.join(root, file)
                source_files.append(file_path)

    return source_files


def main():
    """
    Main function to run the LOC report generation and analysis.
    """
    parser = argparse.ArgumentParser(
        description="Generates a Line-of-Code (LOC) report for the project."
    )
    parser.add_argument(
        "--limit",
        type=int,
        default=500,
        help="The maximum allowed lines of code for a single file (default: 500)."
    )
    args = parser.parse_args()

    project_root = Path.cwd()
    all_files = gather_source_files(str(project_root))

    file_locs = []
    max_loc_found = 0
    for file_path in all_files:
        loc = count_loc(file_path)
        if loc > max_loc_found:
            max_loc_found = loc
        
        relative_path = os.path.relpath(file_path, project_root)
        file_locs.append((loc, relative_path))

    # Sort files by line count in descending order
    file_locs.sort(key=lambda x: x[0], reverse=True)

    if max_loc_found > args.limit:
        print(f"{'LOC':>5} | File")
        print(f"------|----------------------------")
        for loc, path in file_locs:
            print(f"{loc:5d} | {path}")
        
        print(f"\nFiles exceed {args.limit} LOC – see list above")
        exit(1)
    else:
        print(f"All files ≤ {args.limit} LOC")
        exit(0)


if __name__ == "__main__":
    main()