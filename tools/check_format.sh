#!/usr/bin/env bash
set -euo pipefail

# This script checks if C/C++ files in the repository are correctly formatted.
# It exits with a non-zero status code if any file is not formatted.

# Find all *.h and *.cpp files, excluding the extern/ directory.
# The find command is structured to handle this exclusion.
# Using -print0 and a while-read loop is robust against filenames with spaces.
find . -path ./extern -prune -o \( -name "*.h" -o -name "*.cpp" \) -print0 |
while IFS= read -r -d '' file; do
  # Use clang-format and diff to check for formatting changes.
  # 'diff -q' is quiet and only returns an exit code, which is what we need.
  # We compare the file with the output of clang-format.
  if ! clang-format --style=file "$file" | diff -q "$file" - &>/dev/null; then
    echo "Error: File '$file' is not formatted correctly." >&2
    # We found an unformatted file, so we can exit early.
    exit 1
  fi
done

# If the loop completes, all files are formatted correctly.
exit 0