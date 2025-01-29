import json
import tempfile
import time
from copy import deepcopy
from pathlib import Path

import pytest
from webdriver import error


def test_content_process(configuration, geckodriver):
    def trigger_crash(driver):
        # The crash is delayed and happens after this command finished.
        driver.session.url = "about:crashcontent"

        # Bug 1943038: geckodriver fails to detect minidump files for content
        # crashes when the next command is sent immediately.
        time.sleep(1)

        # Send another command that should fail
        with pytest.raises(error.UnknownErrorException):
            driver.session.url

    run_crash_test(configuration, geckodriver, crash_callback=trigger_crash)


def test_parent_process(configuration, geckodriver):
    def trigger_crash(driver):
        with pytest.raises(error.UnknownErrorException):
            driver.session.url = "about:crashparent"

    run_crash_test(configuration, geckodriver, crash_callback=trigger_crash)


def run_crash_test(configuration, geckodriver, crash_callback):
    config = deepcopy(configuration)
    config["capabilities"]["webSocketUrl"] = True

    with tempfile.TemporaryDirectory() as tmpdirname:
        # Use a custom temporary minidump save path to only see
        # the minidump files related to this test
        driver = geckodriver(
            config=config, extra_env={"MINIDUMP_SAVE_PATH": tmpdirname}
        )

        driver.new_session()
        profile_minidump_path = (
            Path(driver.session.capabilities["moz:profile"]) / "minidumps"
        )

        crash_callback(driver)

        tmp_minidump_dir = Path(tmpdirname)
        file_map = verify_minidump_files(tmp_minidump_dir)

        # Check that for both Marionette and Remote Agent the annotations are present
        extra_data = read_extra_file(file_map[".extra"])
        assert (
            extra_data.get("Marionette") == "1"
        ), "Marionette entry is missing or invalid"
        assert (
            extra_data.get("RemoteAgent") == "1"
        ), "RemoteAgent entry is missing or invalid"

        # Remove original minidump files from the profile directory
        remove_files(profile_minidump_path, file_map.values())


def read_extra_file(path):
    """Read and parse the minidump's .extra file."""
    try:
        with path.open("rb") as file:
            data = file.read()

            # Try to decode first and replace invalid utf-8 characters.
            decoded = data.decode("utf-8", errors="replace")

            return json.loads(decoded)
    except json.JSONDecodeError as e:
        raise ValueError(f"Invalid JSON in {path}: {e}")


def remove_files(directory, files):
    """Safely remove a list of files."""
    for file in files:
        file_path = Path(directory) / file.name
        try:
            file_path.unlink()
        except FileNotFoundError:
            print(f"File not found: {file_path}")
        except PermissionError as e:
            raise ValueError(f"Permission error removing {file_path}: {e}")


def verify_minidump_files(directory):
    """Verify that .dmp and .extra files exist and return their paths."""
    minidump_files = list(Path(directory).iterdir())
    assert len(minidump_files) == 2, f"Expected 2 files, found {minidump_files}."

    required_extensions = {".dmp", ".extra"}
    file_map = {
        file.suffix: file
        for file in minidump_files
        if file.suffix in required_extensions
    }

    missing_extensions = required_extensions - file_map.keys()
    assert (
        not missing_extensions
    ), f"Missing required files with extensions: {missing_extensions}"

    return file_map
