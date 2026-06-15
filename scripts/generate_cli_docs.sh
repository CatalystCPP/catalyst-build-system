#!/usr/bin/env bash

set -euo pipefail

echo "================================================"
echo "   Catalyst CLI Documentation Generator"
echo "================================================"

# Find the catalyst binary
CATALYST_BIN="${1:-}"
if [ -z "$CATALYST_BIN" ]; then
    echo "Searching for catalyst executable in build/ directory..."
    # Search in release directories first
    for dir in build/release build/common-release build/common-ccache-release; do
        if [ -x "$dir/catalyst" ] && [ -f "$dir/catalyst" ]; then
            CATALYST_BIN="$dir/catalyst"
            echo "Found binary: $CATALYST_BIN"
            break
        fi
    done

    # Fallback to searching the build directory, skipping build/pack/
    if [ -z "$CATALYST_BIN" ]; then
        FOUND_BINS=$(find build/ -path "build/pack" -prune -o -type f -name catalyst -executable -print 2>/dev/null || true)
        if [ -n "$FOUND_BINS" ]; then
            # Take the first one found
            CATALYST_BIN=$(echo "$FOUND_BINS" | head -n 1)
            echo "Found binary: $CATALYST_BIN"
        fi
    fi
fi

if [ -z "$CATALYST_BIN" ] || [ ! -f "$CATALYST_BIN" ] || [ ! -x "$CATALYST_BIN" ]; then
    echo "Error: Could not locate executable catalyst binary."
    echo "Please compile the project first, or provide the path to the binary:"
    echo "  $0 /path/to/catalyst"
    exit 1
fi

CLI_DOCS_DIR="docs/cli"
mkdir -p "$CLI_DOCS_DIR"

echo "Using docs directory: $CLI_DOCS_DIR"
echo "------------------------------------------------"

# Function to run help command and sanitize binary path output
clean_help() {
    local cmd_args=("$@")
    # Run the command and replace the local binary path with 'catalyst'
    "$CATALYST_BIN" "${cmd_args[@]}" --help 2>&1 | sed "s|$CATALYST_BIN|catalyst|g"
}

# 1. Parse subcommands dynamically from the main --help output
echo "Fetching subcommands from '$CATALYST_BIN'..."
subcommands=$("$CATALYST_BIN" --help 2>&1 | awk '
    BEGIN { in_sub = 0 }
    /^SUBCOMMANDS:/ { in_sub = 1; next }
    /^[A-Z]+:/ { in_sub = 0 }
    /^For more/ { in_sub = 0 }
    in_sub {
        sub(/^[ \t]+/, "")
        if ($1 != "" && $1 != "help") {
            print $1
        }
    }
')

count=0
for subc in $subcommands; do
    echo "Processing subcommand: catalyst $subc..."

    # 2. Map subcommand name to docs filename
    # E.g. profile-ls -> profile_ls.md, feature-ls -> feature_ls.md
    filename="${subc}"
    if [[ "$filename" == *"-ls"* ]]; then
        filename="${filename//-ls/_ls}"
    fi
    md_file="$CLI_DOCS_DIR/${filename}.md"

    # 3. Preserve hand-written content (anything from the first "## " heading onwards)
    manual_content=""
    if [ -f "$md_file" ]; then
        first_heading_line=$(grep -n "^## " "$md_file" | head -n 1 | cut -d: -f1 || true)
        if [ -n "$first_heading_line" ]; then
            manual_content=$(tail -n +"$first_heading_line" "$md_file")
        fi
    fi

    # Fallback to a default Examples section if no manual content was found
    if [ -z "$manual_content" ]; then
        manual_content="## Examples

\`\`\`bash
catalyst $subc
\`\`\`"
    fi

    # 4. Generate the main subcommand help block
    help_out=$(clean_help "$subc")
    new_content="# catalyst $subc

\`\`\`
$help_out
\`\`\`"

    # 5. Check if the subcommand itself has nested subcommands (e.g. catalyst add)
    child_subcs=$("$CATALYST_BIN" "$subc" --help 2>/dev/null | awk '
        BEGIN { in_sub = 0 }
        /^SUBCOMMANDS:/ { in_sub = 1; next }
        /^[A-Z]+:/ { in_sub = 0 }
        /^For more/ { in_sub = 0 }
        in_sub {
            sub(/^[ \t]+/, "")
            if ($1 != "" && $1 != "help") {
                print $1
            }
        }
    ' || true)

    # If there are nested child subcommands, append their help blocks under ### headers
    for child in $child_subcs; do
        echo "  -> Found child subcommand: catalyst $subc $child"
        child_help=$(clean_help "$subc" "$child")
        new_content="$new_content

### $child
\`\`\`
$child_help
\`\`\`"
    done

    # 6. Write out the final combined file
    cat <<EOF > "$md_file"
$new_content

$manual_content
EOF

    count=$((count + 1))
done

echo "------------------------------------------------"
echo "Success! Successfully generated/updated $count CLI documentation files in '$CLI_DOCS_DIR/'."
echo "Any custom examples, details, and handwritten descriptions starting with '## ' were preserved!"
echo "================================================"
