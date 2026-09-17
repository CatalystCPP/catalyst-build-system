#!/usr/bin/env python3
"""Exercise native shared-library scaffolding, consumption, and installation.

Installed applications use an explicit DYLD_LIBRARY_PATH. Catalyst install copies
artifacts; it does not currently rewrite install names or package-relative rpaths.
This tests relocation with that supported loader setup, not automatic relocation.
"""

import argparse
import os
from pathlib import Path
import platform
import shlex
import shutil
import subprocess
import tempfile


def run(args, cwd, env, expected=None):
    command = [str(arg) for arg in args]
    print(f"[{cwd}] {shlex.join(command)}", flush=True)
    result = subprocess.run(
        command, cwd=cwd, env=env, text=True, stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT, check=False,
    )
    print(result.stdout, end="", flush=True)
    if result.returncode:
        raise RuntimeError(f"Command exited with {result.returncode}")
    if expected is not None and expected not in result.stdout:
        raise RuntimeError(f"Expected output {expected!r} was missing")
    return result.stdout


def exercise(catalyst, root, backend, env):
    source = root / "source"
    library = source / "library"
    consumer = source / "consumer"
    prefix = root / "installed"
    unrelated = root / "unrelated"
    for directory in (library, consumer, unrelated):
        directory.mkdir(parents=True)

    run([catalyst, "init", "--name", "answer", "--type", "sharedlib"], library, env)
    # Keep the generated toolchain completely unchanged.
    (library / "include" / "answer.hpp").write_text("#pragma once\nint answer();\n")
    (library / "src" / "answer.cpp").write_text(
        '#include "answer.hpp"\nint answer() { return 42; }\n'
    )
    run([catalyst, "build", "--backend", backend], library, env)
    dylib = library / "build" / "common" / "libanswer.dylib"
    if not dylib.is_file():
        raise RuntimeError(f"Missing native shared library: {dylib}")
    if list((library / "build").rglob("*.so")):
        raise RuntimeError("Scaffolding produced an ELF-style .so on macOS")
    run(["otool", "-hv", dylib], unrelated, env, "DYLIB")
    run(["otool", "-D", dylib], unrelated, env)

    run([catalyst, "init", "--name", "consumer"], consumer, env)
    (consumer / "src" / "consumer.cpp").write_text(
        '#include "answer.hpp"\n#include <iostream>\n'
        'int main() {\n'
        '    const int value = answer();\n'
        '    std::cout << "answer=" << value << "\\n";\n'
        '    return value == 42 ? 0 : 1;\n}\n'
    )
    with (consumer / "catalyst.yaml").open("a") as manifest:
        manifest.write(
            "\ndependencies:\n"
            "  - name: answer\n"
            "    source: local\n"
            "    path: ../library\n"
            "    profiles: [common]\n"
        )
    run([catalyst, "build", "--backend", backend], consumer, env)
    run([catalyst, "run"], consumer, env, "answer=42")
    run(["otool", "-L", consumer / "build/common/consumer"], unrelated, env,
        "libanswer.dylib")

    for project in (library, consumer):
        run([catalyst, "install", "--target", prefix], project, env)
    for artifact in ("lib/libanswer.dylib", "include/answer.hpp", "bin/consumer"):
        if not (prefix / artifact).is_file():
            raise RuntimeError(f"Missing installed artifact: {artifact}")

    # Delete build trees so installed execution cannot silently load their copy.
    shutil.rmtree(source)
    installed_env = dict(env, DYLD_LIBRARY_PATH=str(prefix / "lib"))
    run([prefix / "bin/consumer"], unrelated, installed_env, "answer=42")

    relocated = root / "relocated"
    prefix.rename(relocated)
    installed_env["DYLD_LIBRARY_PATH"] = str(relocated / "lib")
    run([relocated / "bin/consumer"], unrelated, installed_env, "answer=42")

    # Negative control: prove the executable really requires our shared library.
    (relocated / "lib/libanswer.dylib").unlink()
    result = subprocess.run(
        [str(relocated / "bin/consumer")], cwd=unrelated, env=installed_env,
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
    )
    if result.returncode == 0 or "libanswer.dylib" not in result.stdout:
        raise RuntimeError("Removing the installed dylib did not cause the expected loader failure")
    print(f"{backend}: shared-library integration passed", flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--catalyst", required=True, type=Path)
    args = parser.parse_args()
    if platform.system() != "Darwin":
        parser.error("This integration test requires macOS and otool")
    catalyst = args.catalyst.resolve(strict=True)
    env = {key: value for key, value in os.environ.items()
           if not key.startswith("DYLD_") and key != "LD_LIBRARY_PATH"}
    for backend in ("cob", "ninja"):
        with tempfile.TemporaryDirectory(prefix=f"catalyst-dylib-{backend}-") as tmp:
            exercise(catalyst, Path(tmp), backend, env)


if __name__ == "__main__":
    main()
