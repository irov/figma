#! /bin/bash

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPOSITORY_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"

if test -z "$CMAKE"; then
    CMAKE=cmake
fi

SOLUTION_DIR="$REPOSITORY_DIR/solutions/downloads"

mkdir -p "$SOLUTION_DIR"

"$CMAKE" -S "$REPOSITORY_DIR/cmake/Downloads" -B "$SOLUTION_DIR"
"$CMAKE" --build "$SOLUTION_DIR"

exit 0
