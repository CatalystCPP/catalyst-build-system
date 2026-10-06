import glob
import os
import re
import subprocess
import time
from pathlib import Path
import pytest

CATALYST_BIN = Path(os.getcwd()) / "build/catalyst"

@pytest.fixture(autouse=True)
def check_binary():
    assert CATALYST_BIN.exists(), f"Catalyst binary not found at {CATALYST_BIN}"

def test_explain_disabled_no_report_no_stderr(tmp_path):
    project_dir = tmp_path / "project"
    project_dir.mkdir()
    (project_dir / "src").mkdir()

    with open(project_dir / "catalyst.yaml", "w") as f:
        f.write("""
meta:
  generator: cob
manifest:
  name: demo
  type: EXECUTABLE
  dirs:
    source: [src]
    build: build
""")

    with open(project_dir / "src" / "main.cpp", "w") as f:
        f.write("int main() { return 0; }\n")

    result = subprocess.run(
        [str(CATALYST_BIN), "build"],
        cwd=project_dir,
        capture_output=True,
        text=True,
    )
    assert result.returncode == 0
    assert "[EXPLAIN]" not in result.stderr
    assert "[EXPLAIN]" not in result.stdout
    reports = list(project_dir.glob("catalyst_explain_*.md"))
    assert len(reports) == 0

def test_explain_watch_rejected_early(tmp_path):
    project_dir = tmp_path / "project"
    project_dir.mkdir()
    (project_dir / "src").mkdir()

    with open(project_dir / "catalyst.yaml", "w") as f:
        f.write("""
meta:
  generator: cob
manifest:
  name: demo
  type: EXECUTABLE
  dirs:
    source: [src]
    build: build
""")

    with open(project_dir / "src" / "main.cpp", "w") as f:
        f.write("int main() { return 0; }\n")

    result = subprocess.run(
        [str(CATALYST_BIN), "build", "--explain", "--watch"],
        cwd=project_dir,
        capture_output=True,
        text=True,
    )
    assert result.returncode != 0
    assert "--explain" in result.stderr or "--explain" in result.stdout
    # Must fail before report creation
    reports = list(project_dir.glob("catalyst_explain_*.md"))
    assert len(reports) == 0

def test_explain_creates_report_and_stderr(tmp_path):
    project_dir = tmp_path / "project"
    project_dir.mkdir()
    (project_dir / "src").mkdir()

    with open(project_dir / "catalyst.yaml", "w") as f:
        f.write("""
meta:
  generator: cob
manifest:
  name: demo
  type: EXECUTABLE
  dirs:
    source: [src]
    build: build
""")

    with open(project_dir / "src" / "main.cpp", "w") as f:
        f.write("int main() { return 0; }\n")

    env = os.environ.copy()
    env["SECRET_API_TOKEN_SHOULD_NEVER_LEAK"] = "supersecret123"

    result = subprocess.run(
        [str(CATALYST_BIN), "build", "--explain"],
        cwd=project_dir,
        capture_output=True,
        text=True,
        env=env,
    )
    assert result.returncode == 0
    # Every explanation line on stderr must have [EXPLAIN]
    explain_lines = [line for line in result.stderr.splitlines() if "[EXPLAIN]" in line]
    assert len(explain_lines) > 0

    # Ensure report was created
    reports = list(project_dir.glob("catalyst_explain_*.md"))
    assert len(reports) == 1
    report_content = reports[0].read_text()

    # Verify no ANSI escape codes in markdown report
    assert "\x1b[" not in report_content

    # Verify header
    assert "# Catalyst build explanation" in report_content

    # Verify secret is not leaked in report
    assert "SECRET_API_TOKEN_SHOULD_NEVER_LEAK" not in report_content
    assert "supersecret123" not in report_content

    # Verify absolute path presence
    assert str(project_dir.resolve()) in report_content

    # Verify key sections
    assert "Build generator:" in report_content
    assert "Expected generated build file:" in report_content
    assert "Build finished successfully." in report_content

def test_explain_collision_resolution(tmp_path):
    project_dir = tmp_path / "project"
    project_dir.mkdir()
    (project_dir / "src").mkdir()

    with open(project_dir / "catalyst.yaml", "w") as f:
        f.write("""
meta:
  generator: cob
manifest:
  name: demo
  type: EXECUTABLE
  dirs:
    source: [src]
    build: build
""")

    with open(project_dir / "src" / "main.cpp", "w") as f:
        f.write("int main() { return 0; }\n")

    # Run first build
    res1 = subprocess.run(
        [str(CATALYST_BIN), "build", "--explain"],
        cwd=project_dir,
        capture_output=True,
        text=True,
    )
    assert res1.returncode == 0
    reports1 = list(project_dir.glob("catalyst_explain_*.md"))
    assert len(reports1) == 1
    first_report = reports1[0]

    # Pre-create another dummy report with same stem or lock to simulate collision
    lock_file = project_dir / f"{first_report.name}.lock"
    # Even if we run immediately in the same second, collision resolution increments suffix
    res2 = subprocess.run(
        [str(CATALYST_BIN), "build", "--explain"],
        cwd=project_dir,
        capture_output=True,
        text=True,
    )
    assert res2.returncode == 0
    reports2 = list(project_dir.glob("catalyst_explain_*.md"))
    assert len(reports2) == 2

    # Make sure lock files are cleaned up
    locks = list(project_dir.glob("*.lock"))
    assert len(locks) == 0

def test_explain_failed_build_retains_explanations(tmp_path):
    project_dir = tmp_path / "project"
    project_dir.mkdir()
    (project_dir / "src").mkdir()

    with open(project_dir / "catalyst.yaml", "w") as f:
        f.write("""
meta:
  generator: cob
manifest:
  name: demo
  type: EXECUTABLE
  dirs:
    source: [src]
    build: build
""")

    # Deliberate compilation error
    with open(project_dir / "src" / "main.cpp", "w") as f:
        f.write("int main() { syntax_error_here; return 0; }\n")

    result = subprocess.run(
        [str(CATALYST_BIN), "build", "--explain"],
        cwd=project_dir,
        capture_output=True,
        text=True,
    )
    assert result.returncode != 0
    # Explanation report must still be generated and retained
    reports = list(project_dir.glob("catalyst_explain_*.md"))
    assert len(reports) == 1
    report_content = reports[0].read_text()
    assert "Build failed:" in report_content or "Build process failed." in report_content

def test_explain_workspace_child_aggregation(tmp_path):
    workspace_dir = tmp_path / "workspace"
    workspace_dir.mkdir()

    with open(workspace_dir / "WORKSPACE.yaml", "w") as f:
        f.write("""
memberA:
  path: memberA
  profiles: [common]
memberB:
  path: memberB
  profiles: [common]
""")

    # Member A
    (workspace_dir / "memberA").mkdir()
    (workspace_dir / "memberA" / "src").mkdir()
    with open(workspace_dir / "memberA" / "catalyst.yaml", "w") as f:
        f.write("""
meta:
  generator: cob
manifest:
  name: memberA
  type: STATICLIB
  dirs:
    source: [src]
    build: build
""")
    with open(workspace_dir / "memberA" / "src" / "a.cpp", "w") as f:
        f.write("void funcA() {}\n")

    # Member B depends on Member A
    (workspace_dir / "memberB").mkdir()
    (workspace_dir / "memberB" / "src").mkdir()
    with open(workspace_dir / "memberB" / "catalyst.yaml", "w") as f:
        f.write("""
meta:
  generator: cob
manifest:
  name: memberB
  type: EXECUTABLE
  dirs:
    source: [src]
    build: build
dependencies:
  - name: memberA
    source: local
    path: ../memberA
""")
    with open(workspace_dir / "memberB" / "src" / "main.cpp", "w") as f:
        f.write("void funcA(); int main() { funcA(); return 0; }\n")

    result = subprocess.run(
        [str(CATALYST_BIN), "build", "--workspace", "--explain"],
        cwd=workspace_dir,
        capture_output=True,
        text=True,
    )
    assert result.returncode == 0
    reports = list(workspace_dir.glob("catalyst_explain_*.md"))
    assert len(reports) == 1
    report_content = reports[0].read_text()

    # Verify workspace planning explanations
    assert "Workspace topological build order:" in report_content
    assert "Discovered workspace member 'memberA'" in report_content
    assert "Discovered workspace member 'memberB'" in report_content
    # Child spool aggregation should be present for memberA and memberB
    assert "memberA" in report_content
    assert "memberB" in report_content
