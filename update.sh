#!/usr/bin/env bash
set -e

if [ -n "$1" ]; then
  VERSION="$1"
  echo "Using specified version: $VERSION"
else
  CURRENT_VERSION=$(awk '/^[[:space:]]*manifest:/ {in_manifest=1} in_manifest && /^[[:space:]]*version:/ {print $2; exit}' CATALYST.yaml | tr -d '"' | tr -d "'") # pull version from CATALYST.yaml

  if [ -z "$CURRENT_VERSION" ]; then
    echo "Error: Could not determine current version from CATALYST.yaml" >&2
    exit 1
  fi

  IFS='.' read -r MAJOR MINOR PATCH <<< "$CURRENT_VERSION" # split CURRENT_VERSION into MAJOR, MINOR, PATCH

  if [ -z "$MAJOR" ] || [ -z "$MINOR" ] || [ -z "$PATCH" ]; then
    echo "Error: Version '$CURRENT_VERSION' is not in MAJOR.MINOR.PATCH format" >&2
    exit 1
  fi

  PATCH=$((PATCH + 1))
  VERSION="${MAJOR}.${MINOR}.${PATCH}"
  echo "Auto-incrementing version: $CURRENT_VERSION -> $VERSION"
fi

BRANCH_NAME="dev/$VERSION"

# 1. Create and checkout the dev branch
git switch -c "$BRANCH_NAME"

# 2. Update CATALYST.yaml to match MAJOR.MINOR.PATCH (macOS/Linux compatible)
if [[ "$OSTYPE" == "darwin"* ]]; then
  sed -i '' -E "s/^(    version:).*/\1 $VERSION/" CATALYST.yaml
else
  sed -i -E "s/^(    version:).*/\1 $VERSION/" CATALYST.yaml
fi

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
