"""Focused logger integration tests; no Catalyst dependency fetch/build required."""
import json
import os
from pathlib import Path
import shutil
import subprocess

import pytest

ROOT = Path(__file__).resolve().parents[2]


@pytest.fixture(scope="module", params=[0, 1])
def logger_binary(tmp_path_factory, request):
    compiler = shutil.which("clang++")
    if not compiler:
        pytest.skip("clang++ required")
    directory = tmp_path_factory.mktemp("logger")
    source = directory / "main.cpp"
    source.write_text('''
#include "catalyst/utils/log/log.hpp"
#include <cstdlib>
#include <iostream>
int main(int argc, char **argv) {
    catalyst::logger.info("{}", argc > 1 ? argv[1] : "test");
    const char *path = std::getenv("CATALYST_LOG_PATH");
    std::cout << "PATH=" << (path ? path : "") << std::endl;
    if (argc > 2) { std::string line; std::getline(std::cin, line); }
}
''')
    binary = directory / "logger"
    subprocess.run([
        compiler, "-std=c++23", f"-DFF_catalyst__uniform_logs={request.param}",
        f"-DFF_catalyst__log_machine_info={request.param}", "-I", str(ROOT / "include"),
        str(source), str(ROOT / "src/utils/log/log.cpp"), "-o", str(binary),
    ], check=True, capture_output=True, text=True)
    return binary


def environment(**extra):
    env = os.environ.copy()
    env.pop("CATALYST_MACHINE", None)
    env.pop("CATALYST_LOG_PATH", None)
    env.update(extra)
    return env


def run(binary, directory, message="test", **env):
    result = subprocess.run([str(binary), message], cwd=directory,
                            env=environment(**env), capture_output=True, text=True, check=True)
    assert not result.stderr
    return Path(next(line[5:] for line in result.stdout.splitlines() if line.startswith("PATH=")))


def generations(directory):
    return sorted((directory / ".catalyst.logs").glob("*.log"))


def test_retention_and_migration(logger_binary, tmp_path):
    (tmp_path / ".catalyst.log").write_text("legacy\n")
    first = run(logger_binary, tmp_path, "first")
    assert generations(tmp_path)[0].read_text() == "legacy\n"
    for number in range(55):
        latest = run(logger_binary, tmp_path, str(number))
    assert len(generations(tmp_path)) == 51
    assert not first.exists()
    assert (tmp_path / ".catalyst.log").samefile(latest)
    for path in generations(tmp_path):
        for line in path.read_text().splitlines():
            json.loads(line)


def test_machine_calls_do_not_rotate(logger_binary, tmp_path):
    parent = run(logger_binary, tmp_path, "parent")
    child_dir = tmp_path / "child"
    child_dir.mkdir()
    for _ in range(55):
        assert run(logger_binary, child_dir, "child", CATALYST_MACHINE="",
                   CATALYST_LOG_PATH=str(parent)) == parent
    assert len(generations(tmp_path)) == 1
    assert not (child_dir / ".catalyst.logs").exists()
    assert '"message":"child"' in parent.read_text()
    assert run(logger_binary, tmp_path, CATALYST_MACHINE="1") == parent
    # Stale inherited paths must not redirect a new top-level invocation.
    assert run(logger_binary, tmp_path, CATALYST_LOG_PATH=str(parent)) != parent


def test_active_and_overlapping_invocations(logger_binary, tmp_path):
    process = subprocess.Popen([str(logger_binary), "held", "wait"], cwd=tmp_path,
                               env=environment(), stdin=subprocess.PIPE,
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    try:
        while True:
            line = process.stdout.readline()
            assert line, "logger exited before reporting its path"
            if line.startswith("PATH="):
                active = Path(line[5:].strip())
                break
        for _ in range(55):
            latest = run(logger_binary, tmp_path)
        assert active.exists()
        assert len(generations(tmp_path)) == 52
        assert run(logger_binary, tmp_path, "nested", CATALYST_MACHINE="1",
                   CATALYST_LOG_PATH=str(active)) == active
        assert '"message":"nested"' in active.read_text()
        assert (tmp_path / ".catalyst.log").samefile(latest)
    finally:
        process.communicate("\n", timeout=10)
    run(logger_binary, tmp_path)
    assert not active.exists()
    assert len(generations(tmp_path)) == 51


def test_parallel_rotation(logger_binary, tmp_path):
    processes = [subprocess.Popen([str(logger_binary)], cwd=tmp_path, env=environment(),
                                 stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
                 for _ in range(60)]
    for process in processes:
        _, error = process.communicate(timeout=30)
        assert process.returncode == 0
        assert not error
    latest = run(logger_binary, tmp_path)
    assert len(generations(tmp_path)) == 51
    assert (tmp_path / ".catalyst.log").samefile(latest)
    for path in generations(tmp_path):
        for line in path.read_text().splitlines():
            json.loads(line)
