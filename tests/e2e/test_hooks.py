import json
import os
import subprocess
import shutil
import time
from pathlib import Path
import pytest

CATALYST_BIN = Path(os.getcwd()) / "build/catalyst"

@pytest.fixture(autouse=True)
def check_binary():
    assert CATALYST_BIN.exists(), f"Catalyst binary not found at {CATALYST_BIN}"

def test_codegen_hooks(tmp_path):
    project_dir = tmp_path / "codegen_test"
    project_dir.mkdir()
    (project_dir / "src").mkdir()

    with open(project_dir / "CATALYST.yaml", "w") as f:
        f.write("""
common:
  meta:
    generator: cob
  manifest:
    name: codegen_test
    type: binary
    dirs:
      source: [src]
      build: build
  hooks:
    pre-generate:
      codegen:
        cmd: "python3 gen.py in.txt src/out.cpp"
        input: ["in.txt"]
        output: ["src/out.cpp"]
""")

    with open(project_dir / "gen.py", "w") as f:
        f.write("""
import sys
with open(sys.argv[1], 'r') as f_in:
    content = f_in.read()
with open(sys.argv[2], 'w') as f_out:
    f_out.write('// Generated file\\n')
    f_out.write('int ' + content.strip() + ' = 42;\\n')
    f_out.write('int main() { return 0; }\\n')
""")

    with open(project_dir / "in.txt", "w") as f:
        f.write("my_var")

    # Run catalyst build
    env = os.environ.copy()

    # 1st build: should run the codegen hook
    result1 = subprocess.run([str(CATALYST_BIN), "build"], cwd=project_dir, capture_output=True, text=True, env=env)
    assert result1.returncode == 0, f"Build failed: {result1.stderr}"

    out_cpp = project_dir / "src/out.cpp"
    assert out_cpp.exists(), "Codegen output file was not created"

    with open(out_cpp, 'r') as f:
        content = f.read()
        assert "my_var" in content

    mtime1 = out_cpp.stat().st_mtime

    time.sleep(1) # Ensure mtime difference

    # 2nd build: should NOT run the codegen hook because in.txt is older
    result2 = subprocess.run([str(CATALYST_BIN), "build"], cwd=project_dir, capture_output=True, text=True, env=env)
    assert result2.returncode == 0, f"Build failed: {result2.stderr}"

    mtime2 = out_cpp.stat().st_mtime
    assert mtime1 == mtime2, "Codegen hook ran unexpectedly"

    time.sleep(1)

    # 3rd build: Modify input file, should run codegen hook
    with open(project_dir / "in.txt", "w") as f:
        f.write("my_var2")

    result3 = subprocess.run([str(CATALYST_BIN), "build"], cwd=project_dir, capture_output=True, text=True, env=env)
    assert result3.returncode == 0, f"Build failed: {result3.stderr}"

    mtime3 = out_cpp.stat().st_mtime
    assert mtime3 > mtime2, "Codegen hook did not run when input was modified"

    with open(out_cpp, 'r') as f:
        content = f.read()
        assert "my_var2" in content


def test_codegen_hooks_always_run_without_io(tmp_path):
    """When input/output fields are omitted, codegen should always run."""
    project_dir = tmp_path / "codegen_always"
    project_dir.mkdir()
    (project_dir / "src").mkdir()

    with open(project_dir / "CATALYST.yaml", "w") as f:
        f.write("""
common:
  meta:
    generator: cob
  manifest:
    name: codegen_always
    type: binary
    dirs:
      source: [src]
      build: build
  hooks:
    pre-generate:
      codegen:
        cmd: "python3 gen.py src/out.cpp"
""")

    with open(project_dir / "gen.py", "w") as f:
        f.write("""
import sys
with open(sys.argv[1], 'w') as f_out:
    f_out.write('// Generated\\n')
    f_out.write('int main() { return 0; }\\n')
""")

    env = os.environ.copy()

    # 1st build
    result1 = subprocess.run([str(CATALYST_BIN), "build"], cwd=project_dir, capture_output=True, text=True, env=env)
    assert result1.returncode == 0, f"Build failed: {result1.stderr}"

    out_cpp = project_dir / "src/out.cpp"
    assert out_cpp.exists()
    mtime1 = out_cpp.stat().st_mtime

    time.sleep(1)

    # 2nd build: should still run (no input/output tracking)
    result2 = subprocess.run([str(CATALYST_BIN), "build"], cwd=project_dir, capture_output=True, text=True, env=env)
    assert result2.returncode == 0, f"Build failed: {result2.stderr}"

    mtime2 = out_cpp.stat().st_mtime
    assert mtime2 > mtime1, "Codegen hook should have run again without input/output fields"


def test_codegen_hooks_variable_substitution(tmp_path):
    """$IN[i] and $OUT[i] in cmd are replaced with the corresponding paths."""
    project_dir = tmp_path / "codegen_vars"
    project_dir.mkdir()
    (project_dir / "src").mkdir()

    with open(project_dir / "CATALYST.yaml", "w") as f:
        f.write("""
common:
  meta:
    generator: cob
  manifest:
    name: codegen_vars
    type: binary
    dirs:
      source: [src]
      build: build
  hooks:
    pre-generate:
      codegen:
        cmd: "python3 gen.py $IN[0] $OUT[0]"
        input: ["in.txt"]
        output: ["src/out.cpp"]
""")

    with open(project_dir / "gen.py", "w") as f:
        f.write("""
import sys
with open(sys.argv[1], 'r') as f_in:
    content = f_in.read()
with open(sys.argv[2], 'w') as f_out:
    f_out.write('// Generated file\\n')
    f_out.write('int ' + content.strip() + ' = 42;\\n')
    f_out.write('int main() { return 0; }\\n')
""")

    with open(project_dir / "in.txt", "w") as f:
        f.write("subst_var")

    env = os.environ.copy()

    result = subprocess.run([str(CATALYST_BIN), "build"], cwd=project_dir, capture_output=True, text=True, env=env)
    assert result.returncode == 0, f"Build failed: {result.stderr}"

    out_cpp = project_dir / "src/out.cpp"
    assert out_cpp.exists(), "Codegen output file was not created"

    with open(out_cpp, 'r') as f:
        content = f.read()
        assert "subst_var" in content, "Variable substitution did not work correctly"


def test_invalid_hook_name_fails(tmp_path):
    """An unrecognized hook name under hooks: should throw an error and fail the build."""
    project_dir = tmp_path / "invalid_hook"
    project_dir.mkdir()
    (project_dir / "src").mkdir()

    with open(project_dir / "CATALYST.yaml", "w") as f:
        f.write("""
common:
  meta:
    generator: cob
  manifest:
    name: invalid_hook
    type: binary
    dirs:
      source: [src]
      build: build
  hooks:
    prebuild:  # typo, should be pre-build
      - command: "echo prebuild"
""")

    env = os.environ.copy()
    result = subprocess.run([str(CATALYST_BIN), "build"], cwd=project_dir, capture_output=True, text=True, env=env)
    assert result.returncode != 0
    assert "invalid key" in result.stderr.lower() or "invalid key" in result.stdout.lower()


def test_malformed_hook_item_fails(tmp_path):
    """A hook item without command, script, catalyst, or codegen should fail the build."""
    project_dir = tmp_path / "malformed_hook"
    project_dir.mkdir()
    (project_dir / "src").mkdir()

    with open(project_dir / "CATALYST.yaml", "w") as f:
        f.write("""
common:
  meta:
    generator: cob
  manifest:
    name: malformed_hook
    type: binary
    dirs:
      source: [src]
      build: build
  hooks:
    pre-build:
      - comand: "echo typo"  # typo, should be command
""")

    env = os.environ.copy()
    result = subprocess.run([str(CATALYST_BIN), "build"], cwd=project_dir, capture_output=True, text=True, env=env)
    assert result.returncode != 0
    assert "malformed" in result.stderr.lower() or "malformed" in result.stdout.lower()


def test_introspect_is_restricted_to_hook_processes(tmp_path):
    result = subprocess.run([str(CATALYST_BIN), "introspect"], capture_output=True, text=True)
    assert result.returncode == 1
    assert "can only be executed within a Catalyst lifecycle hook" in result.stderr

    state_file = tmp_path / "state.json"
    state_file.write_text('{"null_value":null,"items":[{"name":"first"}]}')
    env = os.environ.copy()
    env.update({"CATALYST_HOOK": "1", "CATALYST_INTROSPECT_FILE": str(state_file)})

    null_result = subprocess.run(
        [str(CATALYST_BIN), "introspect", "null_value"], capture_output=True, text=True, env=env
    )
    assert null_result.returncode == 0
    assert null_result.stdout == ""

    indexed_result = subprocess.run(
        [str(CATALYST_BIN), "introspect", "items.0.name"], capture_output=True, text=True, env=env
    )
    assert indexed_result.returncode == 0
    assert indexed_result.stdout.strip() == "first"

    missing_result = subprocess.run(
        [str(CATALYST_BIN), "introspect", "items[10]"], capture_output=True, text=True, env=env
    )
    assert missing_result.returncode == 1
    assert "index 10 out of bounds" in missing_result.stderr

    state_file.write_text("{broken")
    corrupt_result = subprocess.run([str(CATALYST_BIN), "introspect"], capture_output=True, text=True, env=env)
    assert corrupt_result.returncode == 1
    assert "failed to parse state file" in corrupt_result.stderr


def test_hook_introspection_snapshot_and_environment(tmp_path):
    project_dir = tmp_path / "introspection"
    project_dir.mkdir()
    (project_dir / "src").mkdir()
    (project_dir / "src" / "main.cpp").write_text("int main() { return 0; }\n")
    (project_dir / "CATALYST.yaml").write_text(
        """
common:
  manifest:
    name: introspection_probe
    type: BINARY
    version: 7.8.9
    dirs:
      source: [src]
      include: []
      build: build
  dependencies:
    - name: first_dependency
      source: system
      version: latest
  features:
    enabled_by_default: true
    disabled_by_default: false
  hooks:
    pre-build:
      - script: python3 inspect_hook.py
    post-build:
      - command: python3 inspect_hook.py
"""
    )
    (project_dir / "inspect_hook.py").write_text(
        """
import json
import os
import pathlib
import stat
import subprocess

catalyst = os.environ["CATALYST_TEST_BIN"]

def query(path=None, pretty=False):
    command = [catalyst, "introspect"]
    if path is not None:
        command.append(path)
    if pretty:
        command.append("--pretty")
    return subprocess.check_output(command, text=True)

phase = os.environ["CATALYST_HOOK_NAME"]
payload = {
    "environment": {key: os.environ[key] for key in (
        "CATALYST_HOOK", "CATALYST_HOOK_NAME", "CATALYST_WORKSPACE_ROOT",
        "CATALYST_PROFILES", "CATALYST_FEATURES", "CATALYST_BUILD_DIR",
        "CATALYST_INTROSPECT_FILE",
    )},
    "snapshot_mode": stat.S_IMODE(pathlib.Path(os.environ["CATALYST_INTROSPECT_FILE"]).stat().st_mode),
    "name": query("manifest.name").strip(),
    "bracket": query("dependencies[0].name").strip(),
    "dot": query("dependencies.0.name").strip(),
    "compiler": query("_toolchain.compiler.cxx.executable").strip(),
    "compact": query("manifest.tooling.doc").strip(),
    "pretty": query("_catalyst", pretty=True),
    "whole": json.loads(query()),
}
pathlib.Path(f"{phase}.json").write_text(json.dumps(payload))
"""
    )

    env = os.environ.copy()
    env["CATALYST_TEST_BIN"] = str(CATALYST_BIN)
    result = subprocess.run(
        [str(CATALYST_BIN), "build", "--features", "disabled_by_default"],
        cwd=project_dir,
        capture_output=True,
        text=True,
        env=env,
    )
    assert result.returncode == 0, result.stderr

    snapshot_paths = set()
    for phase in ("pre-build", "post-build"):
        payload = json.loads((project_dir / f"{phase}.json").read_text())
        hook_env = payload["environment"]
        assert hook_env["CATALYST_HOOK"] == "1"
        assert hook_env["CATALYST_HOOK_NAME"] == phase
        assert hook_env["CATALYST_WORKSPACE_ROOT"] == str(project_dir)
        assert hook_env["CATALYST_PROFILES"] == "common"
        assert hook_env["CATALYST_FEATURES"] == "enabled_by_default,disabled_by_default"
        assert hook_env["CATALYST_BUILD_DIR"] == "build/common"
        snapshot_paths.add(Path(hook_env["CATALYST_INTROSPECT_FILE"]))
        assert payload["snapshot_mode"] & 0o222 == 0
        assert payload["name"] == "introspection_probe"
        assert payload["bracket"] == payload["dot"] == "first_dependency"
        assert payload["compiler"]
        assert "\n" not in payload["compact"]
        assert payload["pretty"].startswith("{\n  ")
        assert payload["whole"]["_catalyst"]["hook"] == phase
        assert payload["whole"]["_catalyst"]["features"]["disabled_by_default"] is True
        assert payload["whole"]["_toolchain"]["compiler"]["cxx"]["executable"]

    assert len(snapshot_paths) == 1
    assert all(not path.exists() for path in snapshot_paths)
