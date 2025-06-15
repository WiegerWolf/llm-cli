#!/usr/bin/env python3
"""
Analyzes the Git commit history to identify "hot-spot" files that have
been modified frequently.

This script tallies the number of commits each source file has been a part of,
and lists those that exceed a given threshold. This helps identify candidates
for refactoring or further modularization.

It respects the same file extensions and directory exclusions as loc_report.py.
"""

import argparse
import subprocess
from collections import Counter
from pathlib import Path

# Define the target file extensions
SOURCE_EXTENSIONS = {'.cpp', '.c', '.cc', '.h', '.hpp'}

# Define directories to exclude from the scan
EXCLUDE_DIRS = {'extern', 'build'}


def get_git_modified_files():
    """
    Fetches all file paths from the git log.
    """
    try:
        # Get all files ever committed to the repository
        result = subprocess.run(
            ['git', 'log', '--name-only', '--pretty=format:'],
            capture_output=True, text=True, check=True
        )
        return result.stdout.splitlines()
    except (subprocess.CalledProcessError, FileNotFoundError) as e:
        print(f"Error executing git command: {e}")
        return []


def is_project_source_file(file_path_str):
    """
    Checks if a file is a project source file, respecting exclusions.
    """
    if not file_path_str.strip():
        return False

    file_path = Path(file_path_str)
    
    # Check if the file has one of the target extensions
    if file_path.suffix not in SOURCE_EXTENSIONS:
        return False

    # Check if the file is in an excluded directory
    if any(part in EXCLUDE_DIRS for part in file_path.parts):
        return False
        
    return True


def main():
    """
    Main function to run the refinement candidate analysis.
    """
    parser = argparse.ArgumentParser(
        description="Finds source files that have been modified frequently."
    )
    parser.add_argument(
        "--threshold",
        type=int,
        default=3,
        help="The minimum number of modifications for a file to be listed."
    )
    args = parser.parse_args()

    all_files = get_git_modified_files()
    
    # Filter for valid project source files
    project_files = [f for f in all_files if is_project_source_file(f)]

    # Count the occurrences of each file
    touch_counts = Counter(project_files)

    # Filter for files exceeding the threshold
    hot_files = {
        file: count for file, count in touch_counts.items() if count > args.threshold
    }

    if not hot_files:
        print("No files exceed threshold")
        return

    # Sort by touch count in descending order
    sorted_files = sorted(hot_files.items(), key=lambda item: item[1], reverse=True)

    # Output the table
    print(f"  Touches | File")
    print(f"----------+---------------------------")
    for file, count in sorted_files:
        print(f"   {count:<6d} | {file}")


if __name__ == "__main__":
    main()