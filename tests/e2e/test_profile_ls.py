"""Profile discovery's human-readable and machine-readable contracts."""
import json
import subprocess
from pathlib import Path

import pytest

CATALYST_BIN = Path(__file__).resolve().parents[2] / "build" / "catalyst"


def profiles(directory, *args):
    return subprocess.run(
        [str(CATALYST_BIN), "profile-ls", *args],
        cwd=directory, capture_output=True, text=True,
    )


def test_sorted_unique_profiles(tmp_path):
    (tmp_path / "CATALYST.yaml").write_text("release: {}\ndebug: {}\ncommon: {}\n")
    for name in ["catalyst.yaml", "catalyst_debug.yaml", "catalyst_asan.yaml"]:
        (tmp_path / name).write_text("{}\n")
    (tmp_path / "catalyst_ignored.yaml").mkdir()
    (tmp_path / "catalyst_.yaml").write_text("{}\n")
    expected = ["asan", "common", "debug", "release"]
    text = profiles(tmp_path)
    assert text.returncode == 0, text.stderr
    assert text.stdout.splitlines() == expected
    machine = profiles(tmp_path, "--json")
    assert machine.returncode == 0, machine.stderr
    assert json.loads(machine.stdout) == expected


def test_empty_directory_json(tmp_path):
    result = profiles(tmp_path, "--json")
    assert result.returncode == 0, result.stderr
    assert json.loads(result.stdout) == []


def test_json_escapes_profile_names(tmp_path):
    expected = ['quote"name', 'slash\\name', 'true', '雪']
    (tmp_path / "CATALYST.yaml").write_text(
        "\n".join(f"{json.dumps(name)}: {{}}" for name in expected) + "\n"
    )
    result = profiles(tmp_path, "--json")
    assert result.returncode == 0, result.stderr
    assert json.loads(result.stdout) == sorted(expected)


@pytest.mark.parametrize("contents", ["profiles: [\n", "- debug\n- release\n"])
@pytest.mark.parametrize("args", [(), ("--json",)])
def test_invalid_manifest_fails_without_partial_output(tmp_path, contents, args):
    (tmp_path / "CATALYST.yaml").write_text(contents)
    (tmp_path / "catalyst_debug.yaml").write_text("{}\n")
    result = profiles(tmp_path, *args)
    assert result.returncode != 0
    assert result.stdout == ""
    assert "CATALYST.yaml" in result.stderr
