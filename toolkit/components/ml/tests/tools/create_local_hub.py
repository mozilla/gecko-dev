#!/usr/bin/env python3
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


import argparse
import hashlib
import os
import shutil
import subprocess
import sys
import urllib.request
from pathlib import Path

import yaml

HERE = Path(__file__).resolve().parent
FETCH_FILE = (
    HERE / "../../../../../taskcluster/kinds/fetch/onnxruntime-web-fetch.yml"
).resolve()


def is_git_lfs_installed():
    try:
        output = subprocess.check_output(
            ["git", "lfs", "version"], stderr=subprocess.DEVNULL, text=True
        )
        return "git-lfs" in output.lower()
    except (subprocess.CalledProcessError, FileNotFoundError):
        return False


def compute_sha256(file_path):
    """Compute SHA-256 of a file (binary read)."""
    hasher = hashlib.sha256()
    with file_path.open("rb") as f:
        for chunk in iter(lambda: f.read(4096), b""):
            hasher.update(chunk)
    return hasher.hexdigest()


def download_wasm(fetches, fetches_dir):
    """
    Download and verify ort.jsep.wasm if needed,
    using the 'ort.jsep.wasm' entry in the YAML file.
    """
    wasm_fetch = fetches["ort.jsep.wasm"]["fetch"]
    url = wasm_fetch["url"]
    expected_sha256 = wasm_fetch["sha256"]

    filename = url.split("/")[-1]
    output_file = fetches_dir / filename

    # If the file exists and its checksum matches, skip re-download
    if output_file.exists():
        print(f"Found existing file {output_file}, verifying checksum...")
        if compute_sha256(output_file) == expected_sha256:
            print("Existing file's checksum matches. Skipping download.")
            return
        else:
            print("Checksum mismatch on existing file. Removing and re-downloading...")
            output_file.unlink()

    # Download the file
    print(f"Downloading {url} to {output_file}...")
    with urllib.request.urlopen(url) as response, open(output_file, "wb") as out_file:
        shutil.copyfileobj(response, out_file)

    # Verify SHA-256
    print(f"Verifying SHA-256 of {output_file}...")
    downloaded_sha256 = compute_sha256(output_file)
    if downloaded_sha256 != expected_sha256:
        output_file.unlink(missing_ok=True)
        raise ValueError(
            f"Checksum mismatch for {filename}! "
            f"Expected: {expected_sha256}, got: {downloaded_sha256}"
        )

    print(f"File {filename} downloaded and verified successfully!")


def list_models(fetches):
    """
    List all YAML keys where fetch.type == 'git',
    along with the path-prefix specified in the YAML.
    """
    print("Available git-based models from the YAML:\n")
    for key, data in fetches.items():
        fetch = data.get("fetch")
        if fetch and fetch.get("type") == "git":
            path_prefix = fetch.get("path-prefix", "[no path-prefix specified]")
            print(f"- {key} -> path-prefix: {path_prefix}")
    print("\n(Use `--model <key>` to clone one of these repositories.)")


def clone_model(key, data, fetches_dir):
    """
    Clone (or re-clone) a model if needed.

    The directory is determined by 'path-prefix' from the YAML,
    relative to --fetches-dir. Example:

      path-prefix: "onnx-models/Xenova/all-MiniLM-L6-v2/main/"

    We'll end up cloning to <fetches-dir>/onnx-models/Xenova/all-MiniLM-L6-v2/main
    """
    fetch_data = data["fetch"]
    repo_url = fetch_data["repo"]
    path_prefix = fetch_data["path-prefix"]
    revision = fetch_data.get("revision", "main")

    # Compute the final directory from --fetches-dir + path-prefix
    repo_dir = fetches_dir / path_prefix

    # Ensure parent directories exist
    repo_dir.parent.mkdir(parents=True, exist_ok=True)

    # If the target directory exists, verify that it matches the correct repo & revision
    if repo_dir.exists():
        # 1. Check if .git exists
        if not (repo_dir / ".git").is_dir():
            print(f"Directory '{repo_dir}' exists but is not a git repo. Removing it.")
            shutil.rmtree(repo_dir, ignore_errors=True)
        else:
            # 2. Check if remote origin URL matches
            try:
                existing_url = subprocess.check_output(
                    ["git", "remote", "get-url", "origin"], cwd=repo_dir, text=True
                ).strip()
            except subprocess.CalledProcessError:
                existing_url = None

            if existing_url != repo_url:
                print(
                    f"Repository at '{repo_dir}' has remote '{existing_url}' "
                    f"instead of '{repo_url}'. Removing it."
                )
                shutil.rmtree(repo_dir, ignore_errors=True)
            else:
                # 3. Check if HEAD commit matches 'revision'
                try:
                    current_revision = subprocess.check_output(
                        ["git", "rev-parse", "HEAD"],
                        cwd=repo_dir,
                        text=True,
                    ).strip()
                except subprocess.CalledProcessError:
                    current_revision = None

                # If the revision is a branch name or tag, matching HEAD exactly
                # might not always be correct. We're keeping it simple:
                # if HEAD != revision, remove & reclone.
                if current_revision != revision:
                    print(
                        f"Repo at '{repo_dir}' has HEAD {current_revision}, "
                        f"but we need '{revision}'. Removing it."
                    )
                    shutil.rmtree(repo_dir, ignore_errors=True)

    # If we removed the directory or it never existed, clone it
    if not repo_dir.exists():
        print(f"Cloning {repo_url} into '{repo_dir}'...")
        # Normal clone first
        subprocess.run(["git", "clone", repo_url, str(repo_dir)], check=True)
        # Then checkout the desired revision (branch, commit, or tag)
        subprocess.run(["git", "checkout", revision], cwd=repo_dir, check=True)
        print(f"Checked out revision '{revision}' in '{repo_dir}'.")
    else:
        print(f"{repo_dir} already exists and is up to date. Skipping clone.")


def clone_models(keys, fetches, fetches_dir):
    """
    Clone each model specified by YAML key, if fetch.type == 'git'.
    Uses the path-prefix from the YAML to determine the final directory.
    """
    if not keys:
        return

    # Initialize git lfs once (if we have at least one model)
    subprocess.run(["git", "lfs", "install"], check=True)

    for key in keys:
        if key not in fetches:
            raise ValueError(f"Model '{key}' not found in YAML.")
        data = fetches[key]
        if data.get("fetch", {}).get("type") != "git":
            raise ValueError(f"Model '{key}' is not a git fetch type.")
        clone_model(key, data, fetches_dir)


def main():
    if not is_git_lfs_installed():
        print("git lfs is required for this program to run:")
        print("\t$ sudo apt install git-lfs")
        print("\t$ sudo yum install git-lfs")
        print("\t$ brew install git-lfs")
        print()
        print("\tor see https://github.com/git-lfs/git-lfs/blob/main/README.md")
        sys.exit(1)

    parser = argparse.ArgumentParser(
        description="Download ort.jsep.wasm and optionally clone specified models."
    )

    default_dir = os.getenv("MOZ_FETCHES_DIR", None)

    parser.add_argument(
        "--fetches-dir",
        help="Directory to store the downloaded files (and cloned repos). Uses MOZ_FETCH_DIR if present.",
        default=default_dir,
    )
    parser.add_argument(
        "--list-models",
        action="store_true",
        help="List all available git-based models (keys in the YAML) and exit.",
    )
    parser.add_argument(
        "--model",
        action="append",
        help="YAML key of a model to clone (can be specified multiple times).",
    )
    args = parser.parse_args()

    # Load YAML
    with FETCH_FILE.open("r", encoding="utf-8") as f:
        fetches = yaml.safe_load(f)

    # If listing models, do so and exit
    if args.list_models:
        list_models(fetches)
        return

    if args.fetches_dir is None:
        raise ValueError(
            "Missing --fetches-dir argument or MOZ_FETCHES_DIR env var. Please specify a directory to store the downloaded files"
        )

    fetches_dir = Path(args.fetches_dir).resolve()
    fetches_dir.mkdir(parents=True, exist_ok=True)

    # Always download/verify ort.jsep.wasm
    download_wasm(fetches, fetches_dir)

    # Clone requested models
    if args.model:
        clone_models(args.model, fetches, fetches_dir)


if __name__ == "__main__":
    main()
