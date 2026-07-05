#! /bin/bash

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPOSITORY_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"

if test -z "$CMAKE"; then
    CMAKE=cmake
fi

CONFIGURATION=$1
if test -z "$CONFIGURATION"; then
    CONFIGURATION=Debug
else
    shift
fi

SOLUTION_NAME=solution_xcode_macos
SOLUTION_DIR="$REPOSITORY_DIR/solutions/$SOLUTION_NAME/$CONFIGURATION"
BIN_DIR="$REPOSITORY_DIR/solutions/bin/xcode_macos"

mkdir -p "$SOLUTION_DIR"
mkdir -p "$BIN_DIR"

"$CMAKE" \
    -G"Xcode" \
    -S "$REPOSITORY_DIR" \
    -B "$SOLUTION_DIR" \
    -DCMAKE_BUILD_TYPE:STRING="$CONFIGURATION" \
    -DCMAKE_CONFIGURATION_TYPES:STRING="$CONFIGURATION" \
    -DCMAKE_RUNTIME_OUTPUT_DIRECTORY:PATH="$BIN_DIR" \
    "$@"

exit 0
