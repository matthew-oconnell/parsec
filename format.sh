#!/usr/bin/env bash
set -euo pipefail

# Ensure clang-format is available
if ! command -v clang-format >/dev/null 2>&1; then
  echo "Error: clang-format not found in PATH. Please install clang-format." >&2
  exit 1
fi

# Directories to search
DIRS=("src" "test")

# Extensions to format
EXTS=("h" "hpp" "cpp")

# Build find expression
FIND_EXPR=()
for ext in "${EXTS[@]}"; do
  FIND_EXPR+=(-name "*.${ext}" -o)
done
# remove trailing -o (fix quoting so index expands)
unset "FIND_EXPR[${#FIND_EXPR[@]}-1]"

# Run clang-format in place
for dir in "${DIRS[@]}"; do
  if [[ -d "$dir" ]]; then
    find "$dir" -type f \( "${FIND_EXPR[@]}" \) -print0 |
      xargs -0 clang-format -i
  fi
done
