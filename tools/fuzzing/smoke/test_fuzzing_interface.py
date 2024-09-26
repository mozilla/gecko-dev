import json
import os
import os.path
from pathlib import Path
from platform import system
from shutil import copy, move
from subprocess import TimeoutExpired, run

import mozunit
from moztest.selftest import fixtures
from pytest import mark, param, skip

MOZ_AUTOMATION = os.getenv("MOZ_AUTOMATION", "0") == "1"


def _find_ffbin():
    fetch_dir = Path(os.getenv("MOZ_FETCHES_DIR", ""))

    mozinfo_json = fetch_dir / "target.mozinfo.json"
    if mozinfo_json.is_file():
        mozinfo = json.loads(mozinfo_json.read_text())
        if mozinfo["buildtype"] == "tsan":
            # TSan builds not supported, see bug 1592250"
            return None

    ffbin = fixtures.binary().replace("$MOZ_FETCHES_DIR", str(fetch_dir)).strip('"')
    if MOZ_AUTOMATION and (fetch_dir / "gtest" / "gtest_bin" / "gtest").is_dir():
        ffdir = Path(ffbin).parent
        # move gtest deps to required location
        copy(
            str(fetch_dir / "gtest" / "dependentlibs.list.gtest"),
            str(ffdir / "dependentlibs.list.gtest"),
        )
        move(str(fetch_dir / "gtest" / "gtest_bin" / "gtest"), str(ffdir))

    if not os.path.exists(ffbin):
        raise AssertionError("Missing Firefox build. Build it or set GECKO_BINARY_PATH")

    return ffbin


FF_BIN = _find_ffbin() if system() == "Linux" else None


def _available_targets():
    if system() != "Linux" or FF_BIN is None:
        # OS or build type not supported
        return

    try:
        results = run(
            [FF_BIN, "--headless"],
            env={"FUZZER": "list"},
            capture_output=True,
            check=False,
            timeout=5,
        )
    except TimeoutExpired:
        yield param(None, marks=skip(reason="Requires '--enable-fuzzing' build"))
        return

    if results.returncode != 0 and b"Couldn't load XPCOM" in results.stderr:
        yield param(None, marks=skip(reason="Fuzzing interface requires gtests"))
        return
    assert results.returncode == 0, results.stderr

    has_targets = False
    for line in results.stdout.split(b"\n"):
        if not has_targets:
            if line == b"===== Targets =====":
                has_targets = True
            continue
        if has_targets and line == b"===== End of list =====":
            break
        yield param(line.decode())
    assert has_targets, "No fuzzing targets found!"


@mark.parametrize("target", _available_targets())
def test_fuzzing_interface_smoke(tmp_path, target):
    result = run(
        [FF_BIN, str(tmp_path), "-runs=10"],
        env={"FUZZER": target},
        capture_output=True,
        check=False,
        timeout=300,
    )
    assert result.returncode == 0


if __name__ == "__main__":
    mozunit.main()
