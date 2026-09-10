#!/usr/bin/env python3
import os
import sys
import shutil
import stat
import argparse
import filecmp

def find_cob_binary():
    # 1. Environment variable override
    for env_var in ("COB_PATH", "COB_EXECUTABLE"):
        val = os.getenv(env_var)
        if val and os.path.exists(val) and os.path.isfile(val):
            return os.path.abspath(val)

    # 2. CMake bootstrap location (e.g. from scripts/ci or native bootstrap)
    bootstrap_candidates = [
        "build/macos/cob",
        "build/ci/cob",
        "build/bootstrap/cob",
    ]
    for cand in bootstrap_candidates:
        if os.path.exists(cand) and os.path.isfile(cand):
            return os.path.abspath(cand)

    # 3. Existing resolved tool binary
    existing_target = os.path.abspath("build/tools/cob")
    if os.path.exists(existing_target) and os.path.isfile(existing_target) and os.access(existing_target, os.X_OK):
        return existing_target

    # 4. PATH search
    which_cob = shutil.which("cob")
    if which_cob and os.path.exists(which_cob):
        return os.path.abspath(which_cob)

    # 5. Standard user/system installation paths
    std_paths = [
        os.path.expanduser("~/.local/bin/cob"),
        "/opt/homebrew/bin/cob",
        "/usr/local/bin/cob",
        "/usr/bin/cob",
    ]
    for p in std_paths:
        if os.path.exists(p) and os.path.isfile(p):
            return os.path.abspath(p)

    # 6. Sibling directory builds (common for local multi-repo development)
    sibling_candidates = [
        "../cob/build/common-ccache-release/cob",
        "../cob/build/common-release/cob",
        "../cob/build/common/cob",
        "../catalyst-orchestrated-builder/build/common-ccache-release/cob",
        "../catalyst-orchestrated-builder/build/common-release/cob",
    ]
    for sibling in sibling_candidates:
        if os.path.exists(sibling) and os.path.isfile(sibling):
            return os.path.abspath(sibling)

    # 7. Check if checked-out source in build/cob-source has a built binary
    cob_source_bins = [
        "build/cob-source/build/cob",
        "build/cob_source/build/cob",
    ]
    for csb in cob_source_bins:
        if os.path.exists(csb) and os.path.isfile(csb):
            return os.path.abspath(csb)

    return None

def main():
    parser = argparse.ArgumentParser(description="Resolve and stage native COB binary for Catalyst builds.")
    parser.add_argument("--target", default="build/tools/cob", help="Target location for the resolved cob binary.")
    parser.add_argument("--source", help="Explicit source cob binary path.")
    args = parser.parse_args()

    target = os.path.abspath(args.target)
    src = None

    if args.source:
        if os.path.exists(args.source) and os.path.isfile(args.source):
            src = os.path.abspath(args.source)
        else:
            print(f"Error: Specified --source '{args.source}' does not exist.", file=sys.stderr)
            sys.exit(1)
    else:
        src = find_cob_binary()

    if not src:
        print("Error: Could not resolve a native 'cob' binary.\n"
              "Please provide COB via COB_PATH environment variable, ensure 'cob' is in PATH,\n"
              "or run the bootstrap build.", file=sys.stderr)
        sys.exit(1)

    # If src is already the target path, ensure permissions and exit
    if os.path.samefile(src, target) if (os.path.exists(src) and os.path.exists(target)) else False:
        os.chmod(target, os.stat(target).st_mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)
        print(f"COB binary is already staged at: {target}")
        sys.exit(0)

    # Check if target already exists and is identical to src (preserves mtime for incremental builds)
    if os.path.exists(target) and os.path.isfile(target):
        if filecmp.cmp(src, target, shallow=False):
            # Target is up-to-date; do not touch to avoid triggering downstream rebuilds
            print(f"COB binary at {target} is up-to-date with {src}")
            sys.exit(0)

    # Stage the resolved binary
    os.makedirs(os.path.dirname(target), exist_ok=True)
    shutil.copy2(src, target)
    os.chmod(target, os.stat(target).st_mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)
    print(f"Resolved native COB binary: {src} -> {target}")

if __name__ == "__main__":
    main()
