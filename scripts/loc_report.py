#!/usr/bin/env python3
"""
Performs a line-of-code (LOC) audit on the project.

This script recursively walks the workspace, counts non-blank, non-comment lines of
code in C++ source files (*.cpp, *.h), and reports any files exceeding a
specified limit. It is designed to be used in CI/CD workflows.
"""

import os
import argparse
import sys
from pathlib import Path

# Define the target file extensions
SOURCE_EXTENSIONS = {'.cpp', '.h'}

# Define directories to exclude from the scan
EXCLUDE_DIRS = {'extern', 'build', '.git', 'resources'}


def count_loc(file_path):
    """
    Counts the non-blank, non-comment lines of code in a given file.

    This function correctly handles:
    - Blank lines.
    - Single-line comments (//...).
    - Multi-line block comments (/*...*/).
    - Code and comments on the same line.
    """
    loc = 0
    in_comment_block = False
    try:
        with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
            for line in f:
                line_content = line.strip()

                if not line_content:
                    continue

                effective_line = ''
                i = 0
                n = len(line_content)
                while i < n:
                    if in_comment_block:
                        if line_content[i:i+2] == '*/':
                            in_comment_block = False
                            i += 2
                        else:
                            i += 1
                        continue

                    if line_content[i:i+2] == '//':
                        break  # Rest of the line is a comment

                    if line_content[i:i+2] == '/*':
                        in_comment_block = True
                        i += 2
                        continue

                    effective_line += line_content[i]
                    i += 1

                if effective_line.strip():
                    loc += 1
    except IOError as e:
        print(f"Error reading file {file_path}: {e}", file=sys.stderr)
        return 0
    return loc


def gather_source_files(root_dir):
    """
    Recursively finds all source files in a directory, respecting exclusions.
    """
    source_files = []
    project_root = Path(root_dir).resolve()

    for root, dirs, files in os.walk(project_root, topdown=True):
        # Prune the search by removing excluded directories
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
        description="Generates a Line-of-Code (LOC) report for C++ files."
    )
    parser.add_argument(
        "--threshold",
        type=int,
        default=500,
        help="The maximum allowed lines of code for a single file (default: 500)."
    )
    parser.add_argument(
        "--quiet",
        action="store_true",
        help="Suppress per-file output, only show summary and failures."
    )
    args = parser.parse_args()

    project_root = Path.cwd()
    all_files = gather_source_files(str(project_root))

    violating_files = []
    if not args.quiet:
        print(f"Scanning {len(all_files)} source files with a threshold of {args.threshold} LOC...")
    
    for file_path in all_files:
        loc = count_loc(file_path)
        relative_path = os.path.relpath(file_path, project_root)

        if not args.quiet:
            print(f" - {relative_path:<50} {loc:5d} LOC")

        if loc > args.threshold:
            violating_files.append((relative_path, loc))

    if violating_files:
        print("\nERROR: The following files exceed the LOC threshold:", file=sys.stderr)
        violating_files.sort(key=lambda x: x[1], reverse=True)
        for path, loc in violating_files:
            print(f" - {path} ({loc} lines)", file=sys.stderr)
        sys.exit(1)
    else:
        if not args.quiet:
            print("\nSuccess: All files are within the LOC limit.")
        sys.exit(0)


if __name__ == "__main__":
    main()