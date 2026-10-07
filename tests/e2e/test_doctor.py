"""Doctor is a preflight, not a build: no project executable may be run."""
import json
import os
import subprocess
import textwrap
from pathlib import Path

import pytest

CATALYST_BIN = Path(__file__).resolve().parents[2] / "build" / "catalyst"


def invoke(project, *args, env=None):
    return subprocess.run(
        [str(CATALYST_BIN), "doctor", *args], cwd=project,
        capture_output=True, text=True, env=env,
    )


def report(project, *args, env=None):
    result = invoke(project, "--json", *args, env=env)
    return result, json.loads(result.stdout)


def checks(data, id, status=None):
    return [check for check in data["checks"] if check["id"] == id
            and (status is None or check["status"] == status)]


@pytest.fixture
def project(tmp_path):
    (tmp_path / "src").mkdir()
    (tmp_path / "include").mkdir()
    (tmp_path / "build" / "common").mkdir(parents=True)
    (tmp_path / "build" / "common" / "compile_commands.json").write_text("[]")
    # Literal tool paths make the tests independent of the host compilers.
    tool = tmp_path / "tool with spaces"
    tool.write_text("#!/bin/sh\ntouch TOOL_WAS_EXECUTED\nexit 1\n")
    tool.chmod(0o755)
    (tmp_path / "tc.yaml").write_text(textwrap.dedent(f"""\
        toolchain:
          compiler:
            c:
              executable: {json.dumps(str(tool))}
            cxx:
              executable: {json.dumps(str(tool))}
          linker:
            executable: {json.dumps(str(tool))}
          archiver:
            executable: {json.dumps(str(tool))}
    """))
    (tmp_path / "catalyst.yaml").write_text(textwrap.dedent("""\
        manifest:
          name: probe
          type: BINARY
          toolchain: tc.yaml
          dirs:
            source: [src]
            include: [include]
            build: build
    """))
    return tmp_path


def append(project, yaml):
    with (project / "catalyst.yaml").open("a") as stream:
        stream.write(textwrap.dedent(yaml))


def test_healthy_report_and_text(project):
    result, data = report(project, "--strict")
    assert result.returncode == 0, result.stderr
    assert data["schema_version"] == 1
    assert data["healthy"] is True
    assert data["strict"] is True
    assert data["profiles"] == ["common"]
    assert data["root"] == str(project)
    assert data["summary"]["error"] == data["summary"]["warning"] == 0
    assert checks(data, "toolchain.cxx", "ok")
    assert not (project / "TOOL_WAS_EXECUTED").exists()
    text = invoke(project)
    assert text.returncode == 0
    assert "Catalyst doctor" in text.stdout and "PASS" in text.stdout


def test_warning_and_strict_exit_codes(project):
    (project / "build" / "common" / "compile_commands.json").unlink()
    result, data = report(project)
    assert result.returncode == 0
    assert data["healthy"] is True
    assert checks(data, "build.compile_commands", "warning")
    strict, data = report(project, "--strict")
    assert strict.returncode == 1
    assert data["healthy"] is False
    assert data["summary"]["error"] == 0


def test_accumulates_independent_errors(project):
    (project / "src").rmdir()
    (project / "tc.yaml").write_text("toolchain:\n  compiler:\n    cxx:\n      executable: catalyst-definitely-missing-cxx\n")
    append(project, """\
        meta:
          generator: invalid-backend
        dependencies:
          - name: gone
            source: local
            path: missing
          - name: bad
            source: imaginary
    """)
    result, data = report(project)
    assert result.returncode == 1
    for id in ["manifest.dirs.source", "toolchain.cxx", "build.backend", "dependencies.local", "dependencies.source"]:
        assert checks(data, id, "error"), data
    assert data["summary"]["error"] >= 5
    assert checks(data, "features.resolve", "ok")


def test_does_not_run_hooks_or_custom_dependencies(project):
    append(project, """\
        hooks:
          pre-build:
            - command: touch HOOK_WAS_EXECUTED
          pre-generate:
            - command: touch HOOK_WAS_EXECUTED
        dependencies:
          - name: custom
            source: custom
            command: touch CUSTOM_WAS_EXECUTED
    """)
    result, data = report(project)
    assert result.returncode == 0
    assert checks(data, "dependencies.custom", "warning")
    assert not (project / "HOOK_WAS_EXECUTED").exists()
    assert not (project / "CUSTOM_WAS_EXECUTED").exists()
    assert not (project / "TOOL_WAS_EXECUTED").exists()


@pytest.mark.parametrize("manifest", ["[\n", "- debug\n", "common: nope\n"])
def test_bad_centralized_manifest_is_json_diagnostic(tmp_path, manifest):
    (tmp_path / "CATALYST.yaml").write_text(manifest)
    result, data = report(tmp_path)
    assert result.returncode == 1
    assert data["summary"]["error"] >= 1
    assert data["healthy"] is False


def test_common_uses_catalyst_yaml_not_catalyst_common(project):
    # A bogus alternative must never be read, even when it exists.
    (project / "catalyst_common.yaml").write_text("invalid: [\n")
    result, data = report(project, "--strict")
    assert result.returncode == 0, result.stderr
    selected = checks(data, "profiles.selected", "ok")
    assert len(selected) == 1
    assert selected[0]["message"] == "Profile 'common' from catalyst.yaml"


def test_common_does_not_fall_back_to_catalyst_common(tmp_path):
    (tmp_path / "catalyst_common.yaml").write_text("manifest:\n  type: INTERFACE\n")
    result, data = report(tmp_path)
    assert result.returncode == 1
    errors = checks(data, "profiles.selected", "error")
    assert len(errors) == 1
    assert "catalyst.yaml" in errors[0]["message"]
    assert "catalyst_common.yaml" not in errors[0]["message"]


def test_missing_profiles_are_all_reported(tmp_path):
    result, data = report(tmp_path, "-p", "debug", "release")
    assert result.returncode == 1
    assert data["profiles"] == ["common", "debug", "release"]
    assert len(checks(data, "profiles.selected", "error")) == 3


def test_centralized_precedence_and_split_fallback(project):
    # The centralized common wins; debug falls back to the split file.
    (project / "CATALYST.yaml").write_text("common:\n  manifest:\n    type: INTERFACE\n    dirs:\n      source: []\n      include: []\n      build: build\n")
    (project / "catalyst_debug.yaml").write_text("manifest:\n  name: from-debug\n")
    result, data = report(project, "-p", "debug")
    assert result.returncode == 0, result.stderr
    assert checks(data, "profiles.shadowed", "warning")
    assert any("from-debug" in check["message"] for check in checks(data, "manifest.type", "ok"))
    assert len(checks(data, "profiles.selected", "ok")) == 2


def test_inherited_toolchain_cycle(project):
    (project / "tc.yaml").write_text("toolchain:\n  extends: tc2.yaml\n")
    (project / "tc2.yaml").write_text("toolchain:\n  extends: tc.yaml\n")
    result, data = report(project)
    assert result.returncode == 1
    assert checks(data, "toolchain.resolve", "error")
    assert checks(data, "features.resolve", "ok")


def test_interface_does_not_require_compilers(project):
    manifest = project / "catalyst.yaml"
    manifest.write_text(manifest.read_text().replace("BINARY", "INTERFACE"))
    (project / "tc.yaml").write_text("toolchain:\n  compiler:\n    cxx:\n      executable: absent-catalyst-compiler\n")
    result, data = report(project, "--strict")
    assert result.returncode == 0, result.stderr
    assert not checks(data, "toolchain.cxx")


def test_non_executable_tool_is_rejected(project):
    (project / "tool with spaces").chmod(0o644)
    result, data = report(project)
    assert result.returncode == 1
    assert checks(data, "toolchain.cxx", "error")


def test_manifest_executable_is_not_shell_code(project):
    (project / "tc.yaml").write_text("toolchain:\n  compiler:\n    cxx:\n      executable: 'c++; touch SHELL_WAS_EXECUTED'\n")
    result, data = report(project)
    assert result.returncode == 1
    assert checks(data, "toolchain.cxx", "error")
    assert not (project / "SHELL_WAS_EXECUTED").exists()


def test_build_path_blocked_by_file(project):
    (project / "build" / "common" / "compile_commands.json").unlink()
    (project / "build" / "common").rmdir()
    (project / "build").rmdir()
    (project / "build").write_text("blocking file")
    result, data = report(project)
    assert result.returncode == 1
    assert checks(data, "build.directory", "error")


def test_missing_build_directory_is_not_created(project):
    (project / "build" / "common" / "compile_commands.json").unlink()
    (project / "build" / "common").rmdir()
    (project / "build").rmdir()
    result, data = report(project)
    assert result.returncode == 0
    assert checks(data, "build.directory", "ok")
    assert not (project / "build").exists()


def test_feature_override_validation(project):
    append(project, """\
        features:
          level:
            type: enum
            values: [quiet, verbose]
            default: quiet
    """)
    good, data = report(project, "-f", "level=verbose")
    assert good.returncode == 0, good.stderr
    bad, data = report(project, "-f", "level=invalid")
    assert bad.returncode == 1
    assert checks(data, "features.resolve", "error")


@pytest.mark.parametrize("section", ["manifest: []", "dependencies: {}", "features: scalar", "hooks: []"])
def test_invalid_section_shapes(project, section):
    (project / "catalyst.yaml").write_text(section + "\n")
    result, data = report(project)
    assert result.returncode == 1
    assert checks(data, "profiles.shape", "error")


def test_backend_override_and_path_lookup(project):
    bin_dir = project / "bin"
    bin_dir.mkdir()
    ninja = bin_dir / "ninja"
    ninja.write_text("#!/bin/sh\ntouch NINJA_WAS_EXECUTED\n")
    ninja.chmod(0o755)
    env = {**os.environ, "PATH": str(bin_dir)}
    result, data = report(project, "--backend", "ninja", env=env)
    assert result.returncode == 0, result.stderr
    assert checks(data, "build.backend", "ok")
    assert not (project / "NINJA_WAS_EXECUTED").exists()
    ninja.unlink()
    result, data = report(project, "--backend", "ninja", env=env)
    assert result.returncode == 1
    assert checks(data, "build.backend", "error")


def test_dependency_fields_and_tools(project):
    append(project, """\
        dependencies:
          - name: git-probe
            source: git
          - name: port-probe
            source: vcpkg
          - name: conan-probe
            source: conan
    """)
    env = {**os.environ, "PATH": str(project / "empty-bin")}
    result, data = report(project, env=env)
    assert result.returncode == 1
    assert len(checks(data, "dependencies.declaration", "error")) == 4
    assert len(checks(data, "dependencies.tool", "error")) == 4
    assert checks(data, "dependencies.lock", "warning")


def test_workspace_lockfile_is_recognized(project):
    # Run inside a member; only the parent workspace has a lockfile.
    member = project / "member"
    member.mkdir()
    for path in list(project.iterdir()):
        if path != member:
            path.rename(member / path.name)
    (project / "WORKSPACE.yaml").write_text("probe:\n  path: member\n  profiles: [common]\n")
    (project / "catalyst.lock").write_text("{}\n")
    # Tool paths were absolute; preserve their old location for this test.
    (member / "tc.yaml").write_text((member / "tc.yaml").read_text().replace(str(project / "tool with spaces"), str(member / "tool with spaces")))
    project = member
    append(project, """\
        dependencies:
          - name: remote
            source: git
            url: https://example.invalid/probe.git
            version: main
    """)
    result, data = report(project)
    assert result.returncode == 0, result.stderr
    assert checks(data, "dependencies.lock", "ok")
    assert not checks(data, "dependencies.lock", "warning")
    assert not (project / "TOOL_WAS_EXECUTED").exists()


def test_verbose_logs_do_not_corrupt_json(project):
    result = subprocess.run(
        [str(CATALYST_BIN), "--verbose", "doctor", "--json"],
        cwd=project, capture_output=True, text=True,
    )
    assert result.returncode == 0, result.stderr
    data = json.loads(result.stdout)
    assert data["healthy"] is True
    assert "[DEBUG]" in result.stderr


def test_static_library_checks_archiver(project):
    manifest = project / "catalyst.yaml"
    manifest.write_text(manifest.read_text().replace("BINARY", "STATICLIB"))
    result, data = report(project, "--strict")
    assert result.returncode == 0, result.stderr
    assert checks(data, "toolchain.archiver", "ok")
    assert not checks(data, "toolchain.linker")


def test_machine_mode_does_not_inject_common(project):
    (project / "catalyst_debug.yaml").write_text((project / "catalyst.yaml").read_text())
    (project / "catalyst.yaml").unlink()
    env = {**os.environ, "CATALYST_MACHINE": "1"}
    result, data = report(project, "-p", "debug", env=env)
    assert result.returncode == 0, result.stderr
    assert data["profiles"] == ["debug"]
    assert len(checks(data, "profiles.selected", "ok")) == 1


def test_json_summary_matches_checks(project):
    (project / "include").rmdir()
    result, data = report(project)
    for status, count in data["summary"].items():
        assert count == sum(check["status"] == status for check in data["checks"])
    assert result.returncode == 0
    assert checks(data, "manifest.dirs.include", "warning")
