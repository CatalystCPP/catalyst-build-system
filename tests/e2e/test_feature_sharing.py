"""Local dependency compile requirements and incremental feature consistency."""
import json
import os
from pathlib import Path
import subprocess
import textwrap

import pytest

CATALYST_BIN = Path(__file__).resolve().parents[2] / "build/catalyst"


def run_catalyst(project, *args, success=True):
    env = os.environ.copy()
    env["PATH"] = str(CATALYST_BIN.parent) + os.pathsep + env["PATH"]
    result = subprocess.run(
        [str(CATALYST_BIN), *args], cwd=project, env=env,
        capture_output=True, text=True, timeout=30,
    )
    output = result.stdout + result.stderr
    if success:
        assert result.returncode == 0, output
    else:
        assert result.returncode != 0, output
    return output


def project(root, name, kind="INTERFACE", features="", dependencies="", backend="cob"):
    path = root / name
    (path / "src").mkdir(parents=True)
    (path / "include").mkdir()
    (path / "catalyst.yaml").write_text(textwrap.dedent(f"""\
        meta:
          generator: {backend}
        manifest:
          name: {name}
          type: {kind}
          dirs:
            source: [src]
            include: [include]
            build: build
        """) + features + dependencies)
    return path


def compile_command(project):
    entry = json.loads((project / "compile_commands.json").read_text())[0]
    return entry.get("command", " ".join(entry.get("arguments", [])))


def local(name, path=None, extra=""):
    return f"  - name: {name}\n    source: local\n    path: ../{path or name}\n" + extra


FEATURES = """features:
  enabled: true
  disabled: false
  capacity:
    type: int
    default: 64
  mode:
    type: enum
    values: [off, fast, safe]
    default: fast
  label:
    type: string
    default: development
"""


@pytest.mark.parametrize("backend", ["cob", "ninja"])
def test_defaults_profiles_using_and_c_flags(tmp_path, backend):
    lib = project(tmp_path, "library", features=FEATURES)
    (lib / "catalyst_test.yaml").write_text("features:\n  capacity:\n    type: int\n    default: 128\n")
    app = project(tmp_path, "app", "BINARY", dependencies="dependencies:\n" + local(
        "alias", "library", "    profiles: [common, test]\n    using: [capacity=256, no-enabled, mode=safe]\n"
    ), backend=backend)
    checks = """\
#if FF_library__capacity != 256 || FF_library__enabled != 0 || FF_library__disabled != 0
#error incorrect dependency values
#endif
#if FF_library__mode != FF_library__mode__safe || FF_library__mode__fast != 1
#error incorrect enum values
#endif
#ifdef FF_alias__capacity
#error dependency aliases must not rename feature namespaces
#endif
static const char *label = FF_library__label;
"""
    (app / "src/main.cpp").write_text(checks + 'extern "C" int check(void);\nint main() { return check(); }\n')
    (app / "src/check.c").write_text(checks + "int check(void) { return 0; }\n")
    run_catalyst(app, "build")
    entries = json.loads((app / "compile_commands.json").read_text())
    assert len(entries) == 2
    for entry in entries:
        command = entry.get("command", " ".join(entry.get("arguments", [])))
        assert "FF_library__label" in command
        assert command.count("FF_library__capacity=") == 1
    subprocess.run([str(app / "build/common/app")], check=True)

    # A selected profile also propagates without an explicit using override.
    manifest = app / "catalyst.yaml"
    manifest.write_text(manifest.read_text().replace("    using: [capacity=256, no-enabled, mode=safe]\n", ""))
    for file in (app / "src").iterdir():
        file.write_text(file.read_text().replace("!= 256", "!= 128").replace("__enabled != 0", "__enabled != 1")
                        .replace("__mode__safe ||", "__mode__fast ||"))
    run_catalyst(app, "build")


@pytest.mark.parametrize("backend", ["cob", "ninja"])
def test_incremental_library_and_consumer_stay_consistent(tmp_path, backend):
    lib = project(tmp_path, "library", "STATICLIB", features=FEATURES, backend=backend)
    (lib / "src/lib.cpp").write_text("int capacity() { return FF_library__capacity; }\n")
    app = project(tmp_path, "app", "BINARY", dependencies="dependencies:\n" + local("library"), backend=backend)
    (app / "src/main.cpp").write_text(
        "int capacity();\nint main() { return capacity() == FF_library__capacity ? 0 : 1; }\n"
    )

    def build_and_check(value):
        run_catalyst(app, "build")
        subprocess.run([str(app / "build/common/app")], check=True)
        command = compile_command(app)
        assert f"FF_library__capacity={value}" in command

    build_and_check(64)
    build_file = app / "build/common" / ("catalyst.build" if backend == "cob" else "build.ninja")
    before = build_file.stat().st_mtime_ns
    objects = {p: p.stat().st_mtime_ns for p in (app / "build/common/obj").iterdir() if p.suffix == ".o"}
    build_and_check(64)
    assert build_file.stat().st_mtime_ns == before
    assert all(p.stat().st_mtime_ns == mtime for p, mtime in objects.items())

    manifest = lib / "catalyst.yaml"
    manifest.write_text(manifest.read_text().replace("default: 64", "default: 96"))
    build_and_check(96)
    app_manifest = app / "catalyst.yaml"
    original = app_manifest.read_text()
    app_manifest.write_text(original + "    using: [capacity=256]\n")
    build_and_check(256)
    app_manifest.write_text(original)
    build_and_check(96)


def test_transitive_diamond_deduplication_and_conflict(tmp_path):
    leaf = project(tmp_path, "leaf", features=FEATURES)
    (leaf / "include/leaf.hpp").write_text("#pragma once\n#define LEAF_HEADER 1\n")
    for name in ("left", "right"):
        project(tmp_path, name, dependencies="dependencies:\n" + local("leaf"))
    app = project(tmp_path, "app", "BINARY", dependencies="dependencies:\n" + local("left") + local("right"))
    (app / "src/main.cpp").write_text(
        '#include "leaf.hpp"\nstatic_assert(FF_leaf__capacity == 64);\nint main() { return 0; }\n'
    )
    run_catalyst(app, "build")
    command = compile_command(app)
    assert command.count("FF_leaf__capacity=") == 1

    right = tmp_path / "right/catalyst.yaml"
    right.write_text(right.read_text() + "    using: [capacity=128]\n")
    output = run_catalyst(app, "generate", success=False)
    assert "Conflicting feature definition 'FF_leaf__capacity'" in output
    assert "64" in output and "128" in output
    output = run_catalyst(app, "build", success=False)
    assert "Conflicting feature definition 'FF_leaf__capacity'" in output

    # Recover after failed generation, then pick up a transitive manifest edit.
    right.write_text(right.read_text().replace("    using: [capacity=128]\n", ""))
    leaf_manifest = leaf / "catalyst.yaml"
    leaf_manifest.write_text(leaf_manifest.read_text().replace("default: 64", "default: 128"))
    main = app / "src/main.cpp"
    main.write_text(main.read_text().replace("== 64", "== 128"))
    run_catalyst(app, "build")
    assert "FF_leaf__capacity=128" in compile_command(app)


def test_cycles_and_invalid_features_fail_generation(tmp_path):
    lib = project(tmp_path, "library", features=FEATURES)
    app = project(tmp_path, "app", "BINARY", dependencies="dependencies:\n" + local(
        "library", extra="    using: [capacity=invalid]\n"
    ))
    (app / "src/main.cpp").write_text("int main() { return 0; }\n")
    output = run_catalyst(app, "generate", success=False)
    assert "not a valid integer" in output
    app_manifest = app / "catalyst.yaml"
    app_manifest.write_text(app_manifest.read_text().replace("    using: [capacity=invalid]\n", ""))
    lib_manifest = lib / "catalyst.yaml"
    lib_manifest.write_text(lib_manifest.read_text() + "dependencies:\n" + local("app"))
    output = run_catalyst(app, "generate", success=False)
    assert "Dependency cycle detected" in output


@pytest.mark.parametrize("backend", ["cob", "ninja"])
def test_local_source_changes_are_rebuilt_after_fetch(tmp_path, backend):
    lib = project(tmp_path, "library", "STATICLIB", backend=backend)
    source = lib / "src/lib.cpp"
    source.write_text("int value() { return 0; }\n")
    app = project(tmp_path, "app", "BINARY", dependencies="dependencies:\n" + local("library"), backend=backend)
    (app / "src/main.cpp").write_text("int value(); int main() { return value(); }\n")
    run_catalyst(app, "build")
    source.write_text("int value() { return 1; }\n")
    run_catalyst(app, "build")
    assert subprocess.run([str(app / "build/common/app")]).returncode == 1


def test_consumer_toolchain_formats_dependency_definitions(tmp_path):
    project(tmp_path, "library", features=FEATURES)
    app = project(tmp_path, "app", "BINARY", dependencies="dependencies:\n" + local("library"))
    manifest = app / "catalyst.yaml"
    manifest.write_text(manifest.read_text().replace("  type: BINARY", "  type: BINARY\n  toolchain: tc.yaml"))
    (app / "tc.yaml").write_text('toolchain:\n  flags:\n    define: "/D{name}={value}"\n')
    run_catalyst(app, "generate")
    buildfile = (app / "build/common/catalyst.build").read_text()
    assert "/DFF_library__capacity=64" in buildfile
    assert "-DFF_library__capacity" not in buildfile
