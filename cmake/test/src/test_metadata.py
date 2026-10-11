####
# test_metadata.py:
#
# Tests that the fprime-util metadata files (*.fprime-util) are rewritten, not accumulated, on re-configure.
####
import platform

from . import cmake
from . import settings

METADATA_FIXTURE = cmake.get_build(
    "METADATA_BUILD", settings.REF_APP_PATH, make_targets=[]
)


def test_metadata_stable_after_reconfigure(METADATA_BUILD):
    """Every *.fprime-util file (and its presence) is unchanged by re-configures; the roots hold one copy of each entry"""
    assert METADATA_BUILD["cmake"][0] == 0, "CMake generation failed"
    build = METADATA_BUILD["build"]
    first = {path: path.read_bytes() for path in build.rglob("*.fprime-util")}
    for _ in range(2):
        return_code, _, stderr = cmake.run_cmake(
            METADATA_BUILD["source"],
            build,
            {"FPRIME_SUB_BUILD_JOBS": f"{cmake.CPUS}"},
        )
        assert return_code == 0, f"CMake re-configure failed:\n{''.join(stderr)}"
    assert {path: path.read_bytes() for path in build.rglob("*.fprime-util")} == first
    assert (
        build / "F-Prime" / "build-targets.fprime-util"
    ).read_bytes() == f"Platform{platform.system()}\nall\n".encode()
    assert not (build / "build-targets.fprime-util").exists()
