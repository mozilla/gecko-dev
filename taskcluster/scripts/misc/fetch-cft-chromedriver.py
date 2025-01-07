#!/usr/bin/python3 -u
# -*- coding: utf-8 -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
This script downloads chromedriver for a given platform and then
packages the driver along with the revision and uploads the archive.
This is currently accomplished by using "last known good version" of
the chromedrivers associated with Chrome for Testing. The `Canary`
channel is specified as it is required for the Chromium-as-Release
performance tests.
"""


import argparse
import errno
import os
import shutil
import subprocess
import tempfile

import requests
from redo import retriable

CHROME_FOR_TESTING_INFO = {
    "linux": {
        "platform": "linux64",
        "dir": "cft-chromedriver-linux",
        "result": "cft-cd-linux.tar.bz2",
        "result_backup": "cft-cd-linux-backup.tar.bz2",
        "chromedriver": "chromedriver_linux64.zip",
    },
    "win64": {
        "platform": "win64",
        "dir": "cft-chromedriver-win64",
        "result": "cft-cd-win64.tar.bz2",
        "result_backup": "cft-cd-win64-backup.tar.bz2",
        "chromedriver": "chromedriver_win32.zip",
    },
    "mac": {
        "platform": "mac-x64",
        "dir": "cft-chromedriver-mac",
        "result": "cft-cd-mac.tar.bz2",
        "result_backup": "cft-cd-mac-backup.tar.bz2",
        "chromedriver": "chromedriver_mac64.zip",
    },
    "mac-arm": {
        "platform": "mac-arm64",
        "dir": "cft-chromedriver-mac",
        "result": "cft-cd-mac-arm.tar.bz2",
        "result_backup": "cft-cd-mac-arm-backup.tar.bz2",
        "chromedriver": "chromedriver_mac64.zip",
    },
}

LAST_GOOD_CFT_JSON = (
    "https://googlechromelabs.github.io/chrome-for-testing/"
    "last-known-good-versions-with-downloads.json"
)

MILESTONE_CFT_JSON = (
    "https://googlechromelabs.github.io/chrome-for-testing/"
    "latest-versions-per-milestone-with-downloads.json"
)


def log(msg):
    print("build-cft-chromedriver: %s" % msg)


@retriable(attempts=7, sleeptime=5, sleepscale=2)
def fetch_file(url, filepath):
    """Download a file from the given url to a given file."""
    size = 4096
    r = requests.get(url, stream=True)
    r.raise_for_status()

    with open(filepath, "wb") as fd:
        for chunk in r.iter_content(size):
            fd.write(chunk)


def unzip(zippath, target):
    """Unzips an archive to the target location."""
    log("Unpacking archive at: %s to: %s" % (zippath, target))
    unzip_command = ["unzip", "-q", "-o", zippath, "-d", target]
    subprocess.check_call(unzip_command)


def get_cft_metadata(endpoint=LAST_GOOD_CFT_JSON):
    """Send a request to the Chrome for Testing's last
    good json URL (default) and get the json payload which will have
    the download URLs that we need.
    """
    res = requests.get(endpoint)
    data = res.json()

    return data


def get_cd_url(data, cft_platform, channel):
    """Given the json data, get the download URL's for
    the correct platform
    """
    for p in data["channels"][channel]["downloads"]["chromedriver"]:
        if p["platform"] == cft_platform:
            return p["url"]
    raise Exception("Platform not found")


def get_chromedriver_revision(data, channel):
    """Grab revision metadata from payload"""
    return data["channels"][channel]["revision"]


def fetch_chromedriver(download_url, cft_dir):
    """Get the chromedriver for the given cft url repackage it."""

    tmpzip = os.path.join(tempfile.mkdtemp(), "cd-tmp.zip")
    log("Downloading chromedriver from %s" % download_url)
    fetch_file(download_url, tmpzip)

    tmppath = tempfile.mkdtemp()
    unzip(tmpzip, tmppath)

    # Find the chromedriver then copy it to the chromium directory
    cd_path = None
    for dirpath, _, filenames in os.walk(tmppath):
        for filename in filenames:
            if filename == "chromedriver" or filename == "chromedriver.exe":
                cd_path = os.path.join(dirpath, filename)
                break
        if cd_path is not None:
            break
    if cd_path is None:
        raise Exception("Could not find chromedriver binary in %s" % tmppath)
    log("Copying chromedriver from: %s to: %s" % (cd_path, cft_dir))
    shutil.copy(cd_path, cft_dir)


def get_backup_chromedriver(version, cft_data, cft_platform):
    """Download a backup chromedriver for the transitionary period of machine auto updates.

    If no version is specified, by default grab the N-1 version of the latest Stable channel
    chromedriver.

    """
    log("Grabbing a backup chromedriver...")
    if not version:
        log("No version specified")
        # Get latest stable version and subtract 1
        current_stable_version = cft_data["channels"]["Stable"]["version"].split(".")[0]
        version = str(int(current_stable_version) - 1)
        log("Fetching major version %s" % version)

    milestone_metadata = get_cft_metadata(MILESTONE_CFT_JSON)
    backup_revision = milestone_metadata["milestones"][version]["revision"]
    backup_version = milestone_metadata["milestones"][version]["version"].split(".")[0]

    backup_url = None
    for p in milestone_metadata["milestones"][version]["downloads"]["chromedriver"]:
        if p["platform"] == cft_platform:
            backup_url = p["url"]

    log("Found backup chromedriver")

    if not backup_url:
        raise Exception("Platform not found")

    return backup_url, backup_revision, backup_version


def get_version_from_json(data, channel):
    return data["channels"][channel]["version"].split(".")[0]


def build_cft_archive(platform, channel, backup, version):
    """Download and store a chromedriver for a given platform."""
    upload_dir = os.environ.get("UPLOAD_DIR")
    if upload_dir:
        # Create the upload directory if it doesn't exist.
        try:
            log("Creating upload directory in %s..." % os.path.abspath(upload_dir))
            os.makedirs(upload_dir)
        except OSError as e:
            if e.errno != errno.EEXIST:
                raise

    cft_platform = CHROME_FOR_TESTING_INFO[platform]["platform"]

    data = get_cft_metadata()
    if backup:
        cft_chromedriver_url, revision, payload_version = get_backup_chromedriver(
            version, data, cft_platform
        )
        tar_file = CHROME_FOR_TESTING_INFO[platform]["result_backup"]
    else:
        cft_chromedriver_url = get_cd_url(data, cft_platform, channel)
        revision = get_chromedriver_revision(data, channel)
        payload_version = get_version_from_json(data, channel)
        tar_file = CHROME_FOR_TESTING_INFO[platform]["result"]
    # Make a temporary location for the file
    tmppath = tempfile.mkdtemp()

    # Create the directory format expected for browsertime setup in taskgraph transform
    artifact_dir = CHROME_FOR_TESTING_INFO[platform]["dir"]
    if backup or channel == "Stable":
        # need to prepend the major version to the artifact dir due to how raptor browsertime
        # ensures the correct version is used with chrome stable.
        artifact_dir = payload_version + artifact_dir
    cft_dir = os.path.join(tmppath, artifact_dir)
    os.mkdir(cft_dir)

    # Store the revision number and chromedriver
    fetch_chromedriver(cft_chromedriver_url, cft_dir)
    revision_file = os.path.join(cft_dir, ".REVISION")
    with open(revision_file, "w+") as f:
        f.write(str(revision))

    tar_command = ["tar", "cjf", tar_file, "-C", tmppath, artifact_dir]
    log("Revision is %s" % revision)
    log("Added revision to %s file." % revision_file)

    log("Tarring with the command: %s" % str(tar_command))
    subprocess.check_call(tar_command)

    upload_dir = os.environ.get("UPLOAD_DIR")
    if upload_dir:
        # Move the tarball to the output directory for upload.
        log("Moving %s to the upload directory..." % tar_file)
        shutil.copy(tar_file, os.path.join(upload_dir, tar_file))

    shutil.rmtree(tmppath)


def parse_args():
    """Read command line arguments and return options."""
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--platform",
        help="Corresponding platform of CfT chromedriver to fetch.",
        required=True,
    )
    # Bug 1869592 - Add optional flag to provide CfT channel e.g. Canary, Stable, etc.
    parser.add_argument(
        "--channel",
        help="Corresponding channel of CfT chromedriver to fetch.",
        required=False,
        default="Canary",
    )
    parser.add_argument(
        "--backup",
        help="Determine if we are grabbing a backup chromedriver version.",
        required=False,
        default=False,
        action="store_true",
    )
    parser.add_argument(
        "--version",
        help="Pin the revision if necessary for current platform",
        required=False,
        default="",
    )
    return parser.parse_args()


if __name__ == "__main__":
    args = vars(parse_args())
    build_cft_archive(**args)
