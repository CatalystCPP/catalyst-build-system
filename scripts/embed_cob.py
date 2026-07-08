#!/usr/bin/env python3
import os
import sys
import shutil
import subprocess
import argparse

def main():
    parser = argparse.ArgumentParser(description="Embed cob binary into C++ files.")
    parser.add_argument("--cob-path", help="Explicit path to the cob executable.")
    args = parser.parse_args()

    cob_path = args.cob_path
    if cob_path:
        if not os.path.exists(cob_path):
            resolved = shutil.which(cob_path)
            if resolved:
                cob_path = resolved
            else:
                print(f"Error: Explicitly specified --cob-path '{cob_path}' could not be found.", file=sys.stderr)
                sys.exit(1)

    # 1. Find cob executable locally if not explicitly provided
    if not cob_path:
        cob_path = os.getenv("COB_PATH")
    if not cob_path:
        cob_path = shutil.which("cob")
    if not cob_path:
        # Check standard location
        fallback = os.path.expanduser("~/.local/bin/cob")
        if os.path.exists(fallback):
            cob_path = fallback

    # 2. Fallback to catalyst download if not found locally
    if not cob_path:
        print("Warning: 'cob' executable not found locally.")
        catalyst_path = shutil.which("catalyst")
        if not catalyst_path:
            print("Error: 'catalyst' not found in PATH. Cannot run fallback download.", file=sys.stderr)
            sys.exit(1)

        print("Running fallback: catalyst download to build cob from source...")
        download_target = os.path.abspath("build/cob_download")
        os.makedirs(download_target, exist_ok=True)

        # Determine remote and branch
        remote_url = "https://github.com/CatalystCPP/catalyst-orchestrated-builder"
        branch_name = "main"

        cmd = ["catalyst", "download", "--backend", "cob", "--target", download_target, remote_url, branch_name] # < cob is installed on gh actions runner
        print(f"Executing: {' '.join(cmd)}")

        res = subprocess.run(cmd)
        if res.returncode != 0:
            print("Error: Failed to download and build cob.", file=sys.stderr)
            sys.exit(1)

        # Check if binary exists in the target
        expected_bin = os.path.join(download_target, "bin", "cob")
        if os.name == "nt":
            expected_bin += ".exe"

        if os.path.exists(expected_bin):
            cob_path = expected_bin
        else:
            print(f"Error: Successfully ran download, but could not find binary at {expected_bin}", file=sys.stderr)
            sys.exit(1)

    print(f"Embedding cob binary from: {cob_path}")

    with open(cob_path, "rb") as f:
        data = f.read()

    # Create target directories
    os.makedirs("include/catalyst", exist_ok=True)
    os.makedirs("src/utils", exist_ok=True)

    header_path = "include/catalyst/cob_embedded.hpp"
    cpp_path = "src/utils/cob_embedded.cpp"

    header_content = """#pragma once
#include <cstddef>

namespace catalyst::embedded {

/// @brief Embedded cob binary data.
extern const unsigned char cob_binary[];

/// @brief Length of the embedded cob binary data.
extern const std::size_t cob_binary_len;
} // namespace catalyst::embedded
"""

    with open(header_path, "w") as f:
        f.write(header_content)
    print(f"Generated {header_path}")

    # Generate cpp file.
    # We will write in chunks to avoid memory issues with huge strings.
    print(f"Generating {cpp_path}...")
    with open(cpp_path, "w") as f:
        f.write('#include "catalyst/cob_embedded.hpp"\n\n')
        f.write('namespace catalyst::embedded {\n')
        f.write('alignas(16) const unsigned char cob_binary[] = {\n')

        # Write bytes in chunks of 16 per line
        for i in range(0, len(data), 16):
            chunk = data[i:i+16]
            hex_str = ", ".join(f"0x{b:02x}" for b in chunk)
            if i + 16 < len(data):
                f.write(f"    {hex_str},\n")
            else:
                f.write(f"    {hex_str}\n")

        f.write('};\n')
        f.write(f'const std::size_t cob_binary_len = {len(data)};\n')
        f.write('} // namespace catalyst::embedded\n')
    print(f"Generated {cpp_path}")

if __name__ == "__main__":
    main()
