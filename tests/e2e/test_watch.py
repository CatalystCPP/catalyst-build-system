import os
import subprocess
import time
from pathlib import Path
import pytest
import signal

CATALYST_BIN = Path(os.getcwd()) / "build/catalyst"

@pytest.fixture
def test_project(tmp_path):
    project_dir = tmp_path / "watch_project"
    project_dir.mkdir()
    
    # 1. Init project
    env = os.environ.copy()
    subprocess.run([str(CATALYST_BIN), "init"], cwd=project_dir, capture_output=True, text=True, env=env)
    
    # Ensure it builds once
    subprocess.run([str(CATALYST_BIN), "build"], cwd=project_dir, capture_output=True, text=True, env=env)
    
    return project_dir

def test_build_watch(test_project):
    env = os.environ.copy()
    
    # Start catalyst build --watch
    process = subprocess.Popen(
        [str(CATALYST_BIN), "build", "--watch"],
        cwd=test_project,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        env=env,
        preexec_fn=os.setsid # To kill the whole process group
    )
    
    try:
        # Give it some time to start watching
        time.sleep(2)
        
        # Modify a source file
        src_file = test_project / "src" / "watch_project.cpp"
        with open(src_file, "a") as f:
            f.write("\n// triggered change\n")
        
        # Wait for rebuild
        time.sleep(3)
        
        # Kill the process
        os.killpg(os.getpgid(process.pid), signal.SIGTERM)
        stdout, stderr = process.communicate(timeout=5)
        
        assert "File changed" in stdout or "File changed" in stderr
        assert "Rebuild successful" in stdout or "Rebuild successful" in stderr
        
    except Exception as e:
        os.killpg(os.getpgid(process.pid), signal.SIGKILL)
        raise e
