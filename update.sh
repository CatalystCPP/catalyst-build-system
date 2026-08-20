#!/usr/bin/env bash
set -e

VERSION="2.0.2"
BRANCH_NAME="dev/$VERSION"

# 1. Create and checkout the dev branch
git switch -c $BRANCH_NAME

# 2. Update CATALYST.yaml line 8 to match MAJOR.MINOR.PATCH
# (Example using sed for macOS/Linux)
sed -i "8s/.*/    version: $VERSION/" CATALYST.yaml

# 3. Commit with EXACT message and push
git add CATALYST.yaml
git commit -m "bump version"
git push -u origin "$BRANCH_NAME"

# 4. Create the main PATCH PR targeting master
gh pr create \
  --base master \
  --head "$BRANCH_NAME" \
  --title "Release $VERSION" \
  --body "Automated release PR for $VERSION"
