####
# test_metadata.py:
#
# Tests that the fprime-util metadata files (*.fprime-util) are rewritten, not accumulated, when a build cache is
# re-configured, and that the un-parented directories (project root, F-Prime include directory) keep the file
# presence semantics fprime-util relies on.
####
import os
import platform

from . import cmake
from . import settings

OPTIONS = {"FPRIME_SUB_BUILD_JOBS": f"{cmake.CPUS}"}

METADATA_FIXTURE = cmake.get_build(
    "METADATA_BUILD", settings.REF_APP_PATH, make_targets=[]
)
METADATA_FIND_PACKAGE_FIXTURE = cmake.get_build(
    "METADATA_FIND_PACKAGE_BUILD",
    settings.DATA_DIR / "TestFindPackageProject",
    {},
    make_targets=[],
)


def _snapshot(build):
    """{relative path: bytes} of every *.fprime-util file, sub-build caches included"""
    return {
        path.relative_to(build): path.read_bytes()
        for path in build.rglob("*.fprime-util")
    }


def _reconfigure(fixture, extra=None):
    return_code, _, stderr = cmake.run_cmake(
        fixture["source"], fixture["build"], {**OPTIONS, **(extra or {})}
    )
    assert return_code == 0, f"CMake re-configure failed:\n{''.join(stderr)}"


def test_metadata_byte_identical_after_reconfigure(METADATA_BUILD):
    """Re-configuring twice must leave every metadata file (and its presence) unchanged"""
    cmake.assert_process_success(METADATA_BUILD)
    build = METADATA_BUILD["build"]
    first = _snapshot(build)
    _reconfigure(METADATA_BUILD)
    _reconfigure(METADATA_BUILD)
    assert _snapshot(build) == first


def test_metadata_unparented_directories_exact(METADATA_BUILD):
    """Root and F-Prime directories hold exactly one copy of each entry; root build-targets stays absent"""
    build = METADATA_BUILD["build"]
    assert (
        build / "F-Prime" / "build-targets.fprime-util"
    ).read_bytes() == f"Platform{platform.system()}\nall\n".encode()
    assert not (build / "build-targets.fprime-util").exists()
    assert (build / "sub-directories.fprime-util").read_text().splitlines() == [
        os.path.realpath(settings.REF_APP_PATH / "Ref")
    ]
    assert (build / "F-Prime" / "Svc" / "build-targets.fprime-util").read_bytes() == b""


def test_metadata_root_deployment_once(METADATA_FIND_PACKAGE_BUILD):
    """A deployment registered at the project root is listed once after a re-configure"""
    build = METADATA_FIND_PACKAGE_BUILD["build"]
    first = _snapshot(build)
    _reconfigure(METADATA_FIND_PACKAGE_BUILD)
    assert _snapshot(build) == first
    assert (build / "build-targets.fprime-util").read_text().splitlines().count(
        "TestFindPackageProject"
    ) == 1


def test_metadata_stale_entry_removed(METADATA_BUILD):
    """An entry written by a previous configure disappears when its writer no longer runs"""
    build = METADATA_BUILD["build"]
    _reconfigure(METADATA_BUILD, {"BUILD_TESTING": "ON"})
    assert (build / "F-Prime" / "tests.fprime-util").read_bytes() == b".*\n"
    _reconfigure(METADATA_BUILD, {"BUILD_TESTING": "OFF"})
    assert not (build / "F-Prime" / "tests.fprime-util").exists()
