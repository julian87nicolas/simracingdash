#!/usr/bin/env bash
set -euo pipefail

# Script to remove emulator files from git and commit the change.
# Run this from the repository root.

FILES=(
  "tools/emulator"
  "scripts/build_emulator.sh"
  "scripts/run_emulator.sh"
)

echo "Dry-run: files that would be removed:"
for f in "${FILES[@]}"; do
  git ls-files "$f" || true
done

echo
read -p "Proceed to git rm these files and commit? [y/N] " yn
if [[ "$yn" != "y" && "$yn" != "Y" ]]; then
  echo "Aborted."
  exit 0
fi

# Remove files/dirs if present
for f in "${FILES[@]}"; do
  git rm -r --ignore-unmatch "$f" || true
done

git add -A
git commit -m "Remove emulator tool and obsolete emulator scripts"

echo "Committed removal. Run 'git push' to push the changes to remote." 
