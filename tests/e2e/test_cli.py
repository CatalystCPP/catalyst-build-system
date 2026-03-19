import os
import subprocess
import shutil
from pathlib import Path
import pytest

CATALYST_BIN = Path(os.getcwd()) / "build/catalyst"

@pytest.fixture(autouse=True)
def check_binary():
    assert CATALYST_BIN.exists(), f"Catalyst binary not found at {CATALYST_BIN}"

def test_workspace_build(tmp_path):
    workspace_dir = tmp_path / "test_workspace"
    workspace_dir.mkdir()

    # 1. Create WORKSPACE.yaml
    with open(workspace_dir / "WORKSPACE.yaml", "w") as f:
        f.write("""
libB:
  path: libB
  profiles: [common]
appA:
  path: appA
  profiles: [common]
""")

    # 2. Create libB
    (workspace_dir / "libB").mkdir()
    (workspace_dir / "libB" / "include").mkdir()
    (workspace_dir / "libB" / "src").mkdir()

    with open(workspace_dir / "libB" / "catalyst.yaml", "w") as f:
        f.write("""
meta:
  generator: cbe
manifest:
  name: libB
  type: STATICLIB
  dirs:
    source: [src]
    include: [include]
    build: build
""")

    with open(workspace_dir / "libB" / "include" / "libB.hpp", "w") as f:
        f.write("""
#pragma once
void hello_from_B();
""")

    with open(workspace_dir / "libB" / "src" / "libB.cpp", "w") as f:
        f.write("""
#include "libB.hpp"
#include <iostream>

void hello_from_B() {
    std::cout << "Hello from libB!" << std::endl;
}
""")

    # 3. Create appA
    (workspace_dir / "appA").mkdir()
    (workspace_dir / "appA" / "src").mkdir()

    with open(workspace_dir / "appA" / "catalyst.yaml", "w") as f:
        f.write("""
meta:
  generator: cbe
manifest:
  name: appA
  type: binary
  dirs:
    source: [src]
    build: build
dependencies:
  - name: libB
    source: local
    path: ../libB
""")

    with open(workspace_dir / "appA" / "src" / "main.cpp", "w") as f:
        f.write("""
#include "libB.hpp"
int main() {
    hello_from_B();
    return 0;
}
""")

    # 4. Run catalyst build
    env = os.environ.copy()
    result = subprocess.run([str(CATALYST_BIN), "build", "--workspace"], cwd=workspace_dir, capture_output=True, text=True, env=env)
    assert result.returncode == 0, f"Build failed with error: {result.stderr}"

    # 5. Run the result
    app_bin = workspace_dir / "appA" / "build" / "appA"
    assert app_bin.exists(), "appA binary not found!"
    
    run_result = subprocess.run([str(app_bin)], capture_output=True, text=True)
    assert run_result.returncode == 0
    assert "Hello from libB!" in run_result.stdout

def test_init_and_build(tmp_path):
    project_dir = tmp_path / "new_project"
    project_dir.mkdir()
    
    # 1. Init project
    env = os.environ.copy()
    result = subprocess.run([str(CATALYST_BIN), "init"], cwd=project_dir, capture_output=True, text=True, env=env)
    assert result.returncode == 0, f"Init failed: {result.stderr}"
    
    assert (project_dir / "catalyst.yaml").exists()
    assert (project_dir / "src" / "new_project.cpp").exists()
    
    # 2. Build project
    result = subprocess.run([str(CATALYST_BIN), "build"], cwd=project_dir, capture_output=True, text=True, env=env)
    assert result.returncode == 0, f"Build failed: {result.stderr}\\nStdout: {result.stdout}"
    
    # 3. Run project
    app_bin = project_dir / "build" / "new_project"
    assert app_bin.exists(), "new_project binary not found!"
    
    run_result = subprocess.run([str(app_bin)], capture_output=True, text=True)
    assert run_result.returncode == 0
    assert "Hello, Catalyst!" in run_result.stdout

def test_invalid_yaml_handles_gracefully(tmp_path):
    project_dir = tmp_path / "bad_project"
    project_dir.mkdir()
    
    with open(project_dir / "CATALYST.yaml", "w") as f:
        f.write("manifest:\n  name: [invalid_yaml")
        
    env = os.environ.copy()
    result = subprocess.run([str(CATALYST_BIN), "build"], cwd=project_dir, capture_output=True, text=True, env=env)
    assert result.returncode != 0
    assert "error" in result.stderr.lower() or "error" in result.stdout.lower() or "failed" in result.stderr.lower() or "failed" in result.stdout.lower()
