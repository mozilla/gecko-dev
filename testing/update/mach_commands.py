# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, # You can obtain one at http://mozilla.org/MPL/2.0/.

import logging
import sys
import tempfile
from os import environ, makedirs
from pathlib import Path
from shutil import copytree, unpack_archive

import mozinfo
import mozinstall
import requests
from mach.decorators import Command, CommandArgument
from mozbuild.base import BinaryNotFoundException
from mozlog.structured import commandline

TEST_UPDATE_CHANNEL = "release-localtest"
if TEST_UPDATE_CHANNEL.startswith("release"):
    MAR_CHANNEL = "firefox-mozilla-release"
elif TEST_UPDATE_CHANNEL.startswith("beta"):
    MAR_CHANNEL = "firefox-mozilla-beta"
else:
    MAR_CHANNEL = "firefox-mozilla-central"
FX_DOWNLOAD_DIR_URL = "https://archive.mozilla.org/pub/firefox/releases/"
APP_DIR_NAME = "fx_test"
MANIFEST_LOC = "testing/update/manifest.toml"
DEFAULT_SOURCE_VERSION_POSITION = -3
# Where in the list of allowable source versions should we default to testing
DEFAULT_LOCALE = "en-US"

if environ.get("UPLOAD_DIR"):
    _ARTIFACT_DIR = Path(environ.get("UPLOAD_DIR"), "update-test")
    makedirs(_ARTIFACT_DIR, exist_ok=True)
    VERSION_INFO_FILENAME = Path(_ARTIFACT_DIR, environ.get("VERSION_LOG_FILENAME"))
else:
    VERSION_INFO_FILENAME = None


def setup_update_argument_parser():
    from marionette_harness.runtests import MarionetteArguments
    from mozlog.structured import commandline

    parser = MarionetteArguments()
    commandline.add_logging_group(parser)

    return parser


def get_fx_executable_name(version):
    if mozinfo.os == "mac":
        executable_platform = "mac"
        executable_name = f"Firefox {version}.dmg"

    if mozinfo.os == "linux":
        executable_platform = "linux-x86_64"
        if int(version.split(".")[0]) < 135:
            executable_name = f"firefox-{version}.tar.bz2"
        else:
            executable_name = f"firefox-{version}.tar.xz"

    if mozinfo.os == "win":
        if mozinfo.arch == "aarch64":
            executable_platform = "win64-aarch64"
        elif mozinfo.bits == "64":
            executable_platform = "win64"
        else:
            executable_platform = "win32"
        executable_name = f"Firefox Setup {version}.exe"

    return executable_platform, executable_name.replace(" ", "%20")


def fx_version_exists(version, locale):
    platform, _ = get_fx_executable_name(version)
    executable_url = rf"{FX_DOWNLOAD_DIR_URL}{version}/{platform}/{locale}/"
    response = requests.get(executable_url)
    try:
        response.raise_for_status()
    except requests.exceptions.HTTPError:
        return False
    return True


def get_valid_source_versions(current_version, locale):
    earliest_version = int(current_version.split(".")[0]) - 3

    valid_versions = []
    for major in range(earliest_version, earliest_version + 3):
        minor_versions = [0]
        for minor in range(1, 11):
            if fx_version_exists(f"{major}.{minor}", locale):
                minor_versions.append(minor)
                valid_versions.append(f"{major}.{minor}")
            else:
                break

        for minor in minor_versions:
            for dot in range(1, 15):
                if fx_version_exists(f"{major}.{minor}.{dot}", locale):
                    valid_versions.append(f"{major}.{minor}.{dot}")

    valid_versions.sort()
    return valid_versions


def get_binary_path(tempdir, **kwargs) -> str:
    # Install correct Fx and return executable location
    source_locale = kwargs.get("source_locale") or DEFAULT_LOCALE
    response = requests.get(
        "https://product-details.mozilla.org/1.0/firefox_versions.json"
    )
    response.raise_for_status()
    product_details = response.json()

    source_versions = get_valid_source_versions(
        product_details.get("LATEST_FIREFOX_VERSION"), source_locale
    )
    if kwargs.get("source_versions_back"):
        # NB below: value 0 will get you the oldest acceptable version, not the newest
        source_version = source_versions[-int(kwargs.get("source_versions_back"))]
    else:
        source_version = (
            kwargs.get("source_version")
            or source_versions[DEFAULT_SOURCE_VERSION_POSITION]
        )
    platform, executable_name = get_fx_executable_name(source_version)

    os_edition = f"{mozinfo.os} {mozinfo.os_version}"
    if VERSION_INFO_FILENAME:
        # Only write the file on non-local runs
        print(f"Writing source info to {VERSION_INFO_FILENAME.resolve()}...")
        with VERSION_INFO_FILENAME.open("a") as fh:
            fh.write(f"Test Type: {kwargs.get('test_type')}\n")
            fh.write(f"Region: {source_locale}\n")
            fh.write(f"Source Version: {source_version}\n")
            fh.write(f"Platform: {os_edition}\n")
        with VERSION_INFO_FILENAME.open() as fh:
            print("".join(fh.readlines()))
    else:
        print(
            f"Region: {source_locale}\nSource Version: {source_version}\nPlatform: {os_edition}"
        )

    executable_url = rf"{FX_DOWNLOAD_DIR_URL}{source_version}/{platform}/{source_locale}/{executable_name}"

    installer_filename = Path(tempdir, Path(executable_url).name)
    installed_app_dir = Path(tempdir, APP_DIR_NAME)
    print(f"Downloading Fx from {executable_url}...")
    response = requests.get(executable_url)
    response.raise_for_status()
    print(f"Download successful, status {response.status_code}")
    with installer_filename.open("wb") as fh:
        fh.write(response.content)
    fx_location = mozinstall.install(installer_filename, installed_app_dir)
    print(f"Firefox installed to {fx_location}")
    return fx_location


@Command(
    "update-test",
    category="testing",
    virtualenv_name="update",
    description="Test if the version can be updated to the latest patch successfully,",
    parser=setup_update_argument_parser,
)
@CommandArgument("--binary-path", help="Firefox executable path is needed")
@CommandArgument("--test-type", default="Base", help="Base/Background")
@CommandArgument("--source-version", help="Firefox build version to update from")
@CommandArgument(
    "--source-versions-back",
    help="Update from the version of Fx $N releases before current",
)
@CommandArgument("--source-locale", help="Firefox build locale to update from")
def build(command_context, binary_path, **kwargs):
    tempdir = tempfile.TemporaryDirectory()
    # If we have a symlink to the tmp directory, resolve it
    tempdir_name = str(Path(tempdir.name).resolve())
    test_type = kwargs.get("test_type")

    # Run the specified test in the suite
    with open(MANIFEST_LOC) as f:
        old_content = f.read()

    with open(MANIFEST_LOC, "w") as f:
        f.write("[DEFAULT]\n")
        if test_type.lower() == "base":
            f.write('["test_apply_update.py"]')
        elif test_type.lower() == "background":
            f.write('["test_background_update.py"]')
        else:
            logging.ERROR("Invalid test type")
            sys.exit(1)

    try:
        kwargs["binary"] = set_up(
            binary_path or get_binary_path(tempdir_name, **kwargs), tempdir=tempdir_name
        )
        return run_tests(
            topsrcdir=command_context.topsrcdir, tempdir=tempdir_name, **kwargs
        )
    except BinaryNotFoundException as e:
        command_context.log(
            logging.ERROR,
            "update-test",
            {"error": str(e)},
            "ERROR: {error}",
        )
        command_context.log(logging.INFO, "update-test", {"help": e.help()}, "{help}")
        return 1
    finally:
        with open(MANIFEST_LOC, "w") as f:
            f.write(old_content)
        tempdir.cleanup()


def run_tests(binary=None, topsrcdir=None, tempdir=None, **kwargs):
    from argparse import Namespace

    from marionette_harness.runtests import MarionetteHarness, MarionetteTestRunner

    args = Namespace()
    args.binary = binary
    args.logger = kwargs.pop("log", None)
    if not args.logger:
        args.logger = commandline.setup_logging(
            "Update Tests", args, {"mach": sys.stdout}
        )

    for k, v in kwargs.items():
        setattr(args, k, v)

    args.tests = [
        Path(
            topsrcdir,
            MANIFEST_LOC,
        )
    ]
    args.gecko_log = "-"

    parser = setup_update_argument_parser()
    parser.verify_usage(args)

    failed = MarionetteHarness(MarionetteTestRunner, args=vars(args)).run()
    if VERSION_INFO_FILENAME:
        with VERSION_INFO_FILENAME.open("a") as fh:
            fh.write(f"Status: {'failed' if failed else 'passed'}\n\n")
    if failed > 0:
        return 1
    return 0


def copy_macos_channelprefs(tempdir) -> str:
    # Copy ChannelPrefs.framework to the correct location on MacOS,
    # return the location of the Fx executable
    installed_app_dir = Path(tempdir, APP_DIR_NAME)

    bz_channelprefs_link = "https://bugzilla.mozilla.org/attachment.cgi?id=9417387"

    resp = requests.get(bz_channelprefs_link)
    download_target = Path(tempdir, "channelprefs.zip")
    unpack_target = str(download_target).rsplit(".", 1)[0]
    with download_target.open("wb") as fh:
        fh.write(resp.content)

    unpack_archive(download_target, unpack_target)
    print(
        f"Downloaded channelprefs.zip to {download_target} and unpacked to {unpack_target}"
    )

    src = Path(tempdir, "channelprefs", TEST_UPDATE_CHANNEL)
    dst = Path(installed_app_dir, "Contents", "Frameworks")

    Path(installed_app_dir, "Firefox.app").chmod(455)  # rwx for all users

    print(f"Copying ChannelPrefs.framework from {src} to {dst}")
    copytree(
        Path(src, "ChannelPrefs.framework"),
        Path(dst, "ChannelPrefs.framework"),
        dirs_exist_ok=True,
    )

    # test against the binary that was copied to local
    fx_executable = Path(
        installed_app_dir, "Firefox.app", "Contents", "MacOS", "firefox"
    )
    return str(fx_executable)


def set_up(binary_path, tempdir):
    # Set channel prefs for all OS targets
    binary_path_str = mozinstall.get_binary(binary_path, "Firefox")
    print(f"Binary path: {binary_path_str}")
    binary_dir = Path(binary_path_str).absolute().parent

    if mozinfo.os == "mac":
        return copy_macos_channelprefs(tempdir)
    else:
        with Path(binary_dir, "update-settings.ini").open("w") as f:
            f.write("[Settings]\n")
            f.write(f"ACCEPTED_MAR_CHANNEL_IDS={MAR_CHANNEL}")

        with Path(binary_dir, "defaults", "pref", "channel-prefs.js").open("w") as f:
            f.write(f'pref("app.update.channel", "{TEST_UPDATE_CHANNEL}");')

    return binary_path_str
