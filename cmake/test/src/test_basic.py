from pathlib import Path

from . import cmake
from .test_feature import FEATURE_BUILD_RESULT


def test_relative_paths(FEATURE_BUILD):
    """Basic run test for feature build"""
    cmake.assert_process_success(FEATURE_BUILD)


def test_fprime_util_metadata_stable_on_reconfigure(FEATURE_BUILD):
    """Re-running CMake must not append duplicate entries to the fprime-util metadata files"""
    build_directory = Path(FEATURE_BUILD["build"])
    metadata_files = [
        path
        for name in ["build-targets", "sub-directories", "tests"]
        for path in build_directory.rglob(f"{name}.fprime-util")
        if not any(part.startswith("sub-build-") for part in path.parts)
    ]
    assert metadata_files, "No fprime-util metadata files found"
    before = {path: path.read_text() for path in metadata_files}
    for path, content in before.items():
        lines = [line for line in content.splitlines() if line]
        assert len(lines) == len(set(lines)), f"Duplicate entries in {path}:\n{content}"

    return_code, _, stderr = cmake.subprocess_helper(
        [cmake.CMAKE, "."], build_directory
    )
    assert return_code == 0, f"CMake re-configure failed:\n{''.join(stderr)}"
    for path, content in before.items():
        assert path.read_text() == content, f"{path} changed on re-configure"
