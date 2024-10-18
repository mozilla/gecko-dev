#!/usr/bin/env python3
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import hashlib
import os
import shutil
import sys
import tempfile
from pathlib import Path
from typing import Any, Dict

import requests
import yaml


def change_to_script_directory() -> None:
    # Get the absolute path of the script
    script_path = Path(__file__).resolve()
    # Get the directory name of the script
    script_dir = script_path.parent
    # Change the current working directory to the script's directory
    os.chdir(script_dir)
    print(f"\nChanged working directory to: {script_dir}\n")


def load_yaml(file_path: Path) -> Dict[str, Any]:
    """Load and parse the YAML file."""
    with file_path.open("r") as f:
        data = yaml.safe_load(f)
    return data


def create_temp_directory() -> Path:
    """Create the translations-artifacts directory in the system's temp directory."""
    temp_dir = Path(tempfile.gettempdir()) / "translations-artifacts"
    temp_dir.mkdir(parents=True, exist_ok=True)
    return temp_dir


def download_file(url: str, dest_path: Path) -> None:
    """Download a file from a URL to the destination path."""
    with requests.get(url, stream=True) as r:
        r.raise_for_status()
        with dest_path.open("wb") as f:
            for chunk in r.iter_content(chunk_size=8192):
                if chunk:
                    f.write(chunk)


def verify_sha256(file_path: Path, expected_sha: str) -> None:
    """Verify the SHA256 hash of the downloaded file."""
    sha256 = hashlib.sha256()
    with file_path.open("rb") as f:
        for chunk in iter(lambda: f.read(8192), b""):
            sha256.update(chunk)
    computed_sha = sha256.hexdigest()
    if computed_sha.lower() != expected_sha.lower():
        raise ValueError(
            f"SHA256 mismatch for {file_path.name}: expected {expected_sha}, got {computed_sha}"
        )


def verify_size(file_path: Path, expected_size: int) -> None:
    """Verify the size of the downloaded file."""
    actual_size = file_path.stat().st_size
    if actual_size != expected_size:
        raise ValueError(
            f"Size mismatch for {file_path.name}: expected {expected_size}, got {actual_size}"
        )


def clean_up(temp_dir: Path) -> None:
    """Remove the temporary directory and its contents."""
    if temp_dir.exists() and temp_dir.is_dir():
        shutil.rmtree(temp_dir)
        print(f"Cleaned up temporary directory: {temp_dir}")


def main() -> None:
    # Change the working directory to the script's location
    change_to_script_directory()

    # Define the path to the YAML file
    yaml_relative_path = Path(
        "./../../../../../taskcluster/kinds/fetch/translations-fetch.yml"
    ).resolve()

    if not yaml_relative_path.is_file():
        print(f"YAML file not found at {yaml_relative_path}")
        sys.exit(1)

    # Load YAML data
    data = load_yaml(yaml_relative_path)

    # Create temporary directory
    temp_dir = create_temp_directory()

    # Keep track of downloaded files for cleanup in case of failure
    downloaded_files = []

    try:
        for artifact_key, artifact_info in data.items():
            description = artifact_info.get("description", "No description")
            fetch_info = artifact_info.get("fetch", {})
            artifact_name = fetch_info.get("artifact-name")
            url = fetch_info.get("url")
            expected_sha = fetch_info.get("sha256")
            expected_size = fetch_info.get("size")

            if not all([artifact_name, url, expected_sha, expected_size]):
                raise ValueError(
                    f"Missing required fetch information for artifact '{artifact_key}'."
                )

            print(f"Processing artifact: {artifact_name}")
            print(f"Description: {description}")
            print(f"Downloading from: {url}")

            temp_file_path = temp_dir / artifact_name
            temp_file_download_path = temp_file_path.with_suffix(".download")

            download_file(url, temp_file_download_path)
            downloaded_files.append(temp_file_download_path)

            verify_sha256(temp_file_download_path, expected_sha)
            verify_size(temp_file_download_path, expected_size)

            temp_file_download_path.rename(temp_file_path)
            print(f"Successfully downloaded and verified: {artifact_name}\n")

        moz_fetches_dir = str(temp_dir)
        print("All artifacts downloaded and verified...")
        print("\n⏩ NEXT STEPS ⏩\n")
        print(
            "To export MOZ_FETCHES_DIR environment variable in your current shell, run the following command:\n"
        )

        if os.name == "nt":
            # Windows Command Prompt
            print(f"❯ set MOZ_FETCHES_DIR={moz_fetches_dir}")
            # Windows PowerShell
            print(f"❯ $env:MOZ_FETCHES_DIR={moz_fetches_dir}")
        else:
            # Linux/macOS
            print(f"❯ export MOZ_FETCHES_DIR={moz_fetches_dir}")

        print()

    except Exception as e:
        print(f"\nAn error occurred: {e}")
        print("Cleaning up downloaded files...")
        clean_up(temp_dir)
        sys.exit(1)


if __name__ == "__main__":
    main()
