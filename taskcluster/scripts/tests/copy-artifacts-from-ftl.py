#!/usr/bin/env python3

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
This script is designed to automate the process of fetching artifacts (either baseline profile or crash logs)
from Google Cloud Storage (GCS) for devices in Firebase TestLab.
It is intended to be run as part of a Taskcluster job following a scheduled test task, or as part of
a Taskcluster that runs baseline profile generation on Firebase TestLab.
The script requires the presence of a `matrix_ids.json` artifact in the results directory
and the availability of the `gsutil` command in the environment.

The script performs the following operations:
- Loads the `matrix_ids.json` artifact to identify the GCS paths for the artifacts.
- In the case of crash logs, identifies failed devices based on the outcomes specified in the `matrix_ids.json` artifact.
- Fetches the specified artifact type (baseline profiles or crash logs) from the specified GCS paths.
- Copies the fetched artifacts to the current worker artifact results directory.

The script is configured to log its operations and errors, providing visibility into its execution process.
It uses the `gsutil` command-line tool to interact with GCS, ensuring compatibility with the GCS environment.

Usage:
    python3 copy-artifacts-from-ftl.py <artifact_type>

    artifact_type: "baseline_profile" or "crash_log"

Requirements:
    - The `matrix_ids.json` artifact must be present in the results directory.
    - The `gsutil` command must be available in the environment.
    - The script should be run after a scheduled test task in a Taskcluster job or as part of a
        scheduled baseline profile task in a Taskcluster job

Output:
    - Artifacts are copied to the current worker artifact results directory.
"""

import json
import logging
import os
import re
import subprocess
import sys
from enum import Enum


def setup_logging():
    """Configure logging for the script."""
    log_format = "%(message)s"
    logging.basicConfig(level=logging.INFO, format=log_format)


class Worker(Enum):
    """
    Worker paths
    """

    RESULTS_DIR = "/builds/worker/artifacts/results"
    BASELINE_PROFILE_DEST = "/builds/worker/artifacts/build/baseline-prof.txt"
    ARTIFACTS_DIR = "/builds/worker/artifacts"


class ArtifactType(Enum):
    """
    Artifact types for fetching matrix IDs, crash logs and baseline profile.
    """

    BASELINE_PROFILE = (
        "artifacts/sdcard/Android/media/org.mozilla.fenix.benchmark/*-baseline-prof.txt"
    )
    CRASH_LOG = "data_app_crash*.txt"
    MATRIX_IDS = "matrix_ids.json"


def load_matrix_ids_artifact(matrix_file_path):
    """Load the matrix IDs artifact from the specified file path.

    Args:
        matrix_file_path (str): The file path to the matrix IDs artifact.
    Returns:
        dict: The contents of the matrix IDs artifact.
    """
    try:
        with open(matrix_file_path, "r") as f:
            return json.load(f)
    except FileNotFoundError:
        exit_with_error(f"Could not find matrix file: {matrix_file_path}")
    except json.JSONDecodeError:
        exit_with_error(f"Error decoding matrix file: {matrix_file_path}")


def get_gcs_path(matrix_artifact_file):
    """
    Extract the root GCS path from the matrix artifact file.

    Args:
        matrix_artifact_file (dict): The matrix artifact file contents.
    Returns:
        str: The root GCS path extracted from the matrix artifact file.
    """
    for matrix in matrix_artifact_file.values():
        gcs_path = matrix.get("gcsPath")
        if gcs_path:
            return gcs_path
    return None


def check_gsutil_availability():
    """
    Check the availability of the `gsutil` command in the environment.
    Exit the script if `gsutil` is not available.
    """
    try:
        subprocess.run(
            ["gsutil", "--version"], capture_output=True, text=True, check=True
        )
    except Exception as e:
        exit_with_error(f"Error executing gsutil: {e}")


def fetch_artifacts(root_gcs_path, device, artifact_pattern):
    """
    Fetch artifacts from the specified GCS path pattern for the given device.

    Args:
        root_gcs_path (str): The root GCS path for the artifacts.
        device (str): The device name for which to fetch artifacts.
        artifact_pattern (str): The pattern to match the artifacts.
    Returns:
        list: A list of artifacts matching the specified pattern.
    """
    gcs_path_pattern = f"gs://{root_gcs_path.rstrip('/')}/{device}/{artifact_pattern}"

    try:
        result = subprocess.check_output(["gsutil", "ls", gcs_path_pattern], text=True)
        return result.splitlines()
    except subprocess.CalledProcessError as e:
        if "AccessDeniedException" in e.output:
            logging.error(f"Permission denied for GCS path: {gcs_path_pattern}")
        elif "network error" in e.output.lower():
            logging.error(f"Network error accessing GCS path: {gcs_path_pattern}")
        else:
            logging.error(f"Failed to list files: {e.output}")
        return []
    except Exception as e:
        logging.error(f"Error executing gsutil: {e}")
        return []


def fetch_device_names(matrix_artifact_file, only_failed=False):
    """
    Fetch the names of devices that were used based on the outcomes specified in the matrix artifact file.

    Args:
        matrix_artifact_file (dict): The matrix artifact file contents.
        only_failed (bool): If True, only return devices with failed outcomes.
    Returns:
        list: A list of device names.
    """
    devices = []
    for matrix in matrix_artifact_file.values():
        axes = matrix.get("axes", [])
        for axis in axes:
            if not only_failed or axis.get("outcome") == "failure":
                device = axis.get("device")
                if device:
                    devices.append(device)
    return devices


def gsutil_cp(artifact, dest):
    """
    Copy the specified artifact to the destination path using `gsutil`.

    Args:
        artifact (str): The path to the artifact to copy.
        dest (str): The destination path to copy the artifact to.
    Returns:
        None
    """
    logging.info(f"Copying {artifact} to {dest}")
    try:
        result = subprocess.run(
            ["gsutil", "cp", artifact, dest], capture_output=True, text=True
        )
        if result.returncode != 0:
            if "AccessDeniedException" in result.stderr:
                logging.error(f"Permission denied for GCS path: {artifact}")
            elif "network error" in result.stderr.lower():
                logging.error(f"Network error accessing GCS path: {artifact}")
            else:
                logging.error(f"Failed to list files: {result.stderr}")
    except Exception as e:
        logging.error(f"Error executing gsutil: {e}")


def parse_crash_log(log_path):
    """Parse the crash log and log any crash stacks in a specific format."""
    crashes_reported = 0
    if os.path.isfile(log_path):
        with open(log_path) as f:
            contents = f.read()
            proc = "unknown"
            match = re.search(r"Process: (.*)\n", contents, re.MULTILINE)
            if match and len(match.groups()) == 1:
                proc = match.group(1)
            match = re.search(
                r"\n([\w\.]+[:\s\w\.,!?#^\'\"]+)\s*(at\s.*\n)", contents, re.MULTILINE
            )
            if match and len(match.groups()) == 2:
                top_frame = match.group(1).rstrip() + " " + match.group(2)
                remainder = contents[match.span()[1] :]
                logging.error(f"PROCESS-CRASH | {proc} | {top_frame}{remainder}")
                crashes_reported = 1
    return crashes_reported


def process_artifacts(artifact_type):
    """
    Process the artifacts based on the specified artifact type.

    Args:
        artifact_type (ArtifactType): The type of artifact to process.
    """

    matrix_ids_artifact = load_matrix_ids_artifact(
        Worker.RESULTS_DIR.value + "/" + ArtifactType.MATRIX_IDS.value
    )
    only_get_devices_with_failure = artifact_type == ArtifactType.CRASH_LOG
    device_names = fetch_device_names(
        matrix_ids_artifact, only_get_devices_with_failure
    )

    if not device_names:
        exit_with_error("Could not find any device in matrix file.")

    root_gcs_path = get_gcs_path(matrix_ids_artifact)
    if not root_gcs_path:
        exit_with_error("Could not find root GCS path in matrix file.")

    if artifact_type == ArtifactType.BASELINE_PROFILE:
        return process_baseline_profile_artifact(root_gcs_path, device_names)
    else:
        return process_crash_artifacts(root_gcs_path, device_names)


def process_baseline_profile_artifact(root_gcs_path, device_names):
    device = device_names[0]
    artifact = fetch_artifacts(
        root_gcs_path, device, ArtifactType.BASELINE_PROFILE.value
    )[0]
    if not artifact:
        exit_with_error(f"No artifacts found for device: {device}")

    gsutil_cp(artifact, Worker.BASELINE_PROFILE_DEST.value)


def process_crash_artifacts(root_gcs_path, failed_device_names):
    crashes_reported = 0
    for device in failed_device_names:
        artifacts = fetch_artifacts(root_gcs_path, device, ArtifactType.CRASH_LOG.value)
        if not artifacts:
            logging.info(f"No artifacts found for device: {device}")
            continue

        for artifact in artifacts:
            gsutil_cp(artifact, Worker.RESULTS_DIR.value)
            crashes_reported += parse_crash_log(
                os.path.join(Worker.RESULTS_DIR.value, os.path.basename(artifact))
            )

    return crashes_reported


def exit_with_error(message):
    logging.error(message)
    sys.exit(1)


def main():
    setup_logging()
    check_gsutil_availability()

    if len(sys.argv) < 2:
        logging.error("Usage: python script_name.py <artifact_type>")
        sys.exit(1)

    artifact_type_arg = sys.argv[1]
    if artifact_type_arg == "baseline_profile":
        process_artifacts(ArtifactType.BASELINE_PROFILE)
    elif artifact_type_arg == "crash_log":
        process_artifacts(ArtifactType.CRASH_LOG)
    else:
        logging.error("Invalid artifact type. Use 'baseline_profile' or 'crash_log'.")
        sys.exit(1)


if __name__ == "__main__":
    sys.exit(main())
