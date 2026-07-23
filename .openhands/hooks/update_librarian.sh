#!/bin/bash
# SGNode Librarian Update Script
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# SCRIPT_DIR is .openhands/hooks, so parent is .openhands, grandparent is project root
PROJECT_ROOT="$(dirname "$(dirname "$SCRIPT_DIR")")"
LIBRARIAN_DIR="$PROJECT_ROOT/.openhands/librarian"

echo "SGNode Librarian Memory Update"
echo "================================"

if ! git rev-parse --git-dir > /dev/null 2>&1; then
    echo "Not in a git repository. Skipping librarian update."
    exit 0
fi

CHANGED_FILES=$(git diff-tree --no-commit-id --name-only -r HEAD 2>/dev/null || true)

if [ -z "$CHANGED_FILES" ]; then
    echo "No changed files detected."
    exit 0
fi

echo "Changed files:"
echo "$CHANGED_FILES"
echo ""

RELEVANT_CHANGES=false
for file in $CHANGED_FILES; do
    if echo "$file" | grep -qE "\.(ino|h|cpp)$|README\.md|sg_protocol\.h$"; then
        RELEVANT_CHANGES=true
        echo "  - $file (requires update)"
    fi
done

if [ "$RELEVANT_CHANGES" = false ]; then
    echo "No source code changes detected. Skipping."
    exit 0
fi

echo ""
echo "Updating librarian memory..."

if [ -f "$LIBRARIAN_DIR/update_librarian.py" ]; then
    python3 "$LIBRARIAN_DIR/update_librarian.py"
    echo "Librarian update complete."
else
    echo "Python script not found."
    exit 1
fi
