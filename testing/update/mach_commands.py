# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, # You can obtain one at http://mozilla.org/MPL/2.0/.

import logging
import sys
import tempfile
from pathlib import Path
from platform import uname
from shutil import copytree, unpack_archive

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
TEST_REGION = "en-US"
TEST_SOURCE_VERSION = "135.0.1"
FX_DOWNLOAD_DIR_URL = "https://archive.mozilla.org/pub/firefox/releases/"
APP_DIR_NAME = "fx_test"
MANIFEST_LOC = "testing/update/manifest.toml"


def setup_update_argument_parser():
    from marionette_harness.runtests import MarionetteArguments
    from mozlog.structured import commandline

    parser = MarionetteArguments()
    commandline.add_logging_group(parser)

    return parser


def get_fx_executable_name(version):
    u = uname()

    if u.system == "Darwin":
        platform = "mac"
        executable_name = f"Firefox {version}.dmg"

    if u.system == "Linux":
        if "64" in u.machine:
            platform = "linux-x86_64"
        else:
            platform = "linux-x86_64"
        if int(version.split(".")[0]) < 135:
            executable_name = f"firefox-{version}.tar.bz2"
        else:
            executable_name = f"firefox-{version}.tar.xz"

    if u.system == "Windows":
        if u.machine == "ARM64":
            platform = "win64-aarch64"
        elif "64" in u.machine:
            platform = "win64"
        else:
            platform = "win32"
        executable_name = f"Firefox Setup {version}.exe"

    return platform, executable_name.replace(" ", "%20")


def get_binary_path(tempdir, **kwargs) -> str:
    # Install correct Fx and return executable location
    platform, executable_name = get_fx_executable_name(TEST_SOURCE_VERSION)

    executable_url = rf"{FX_DOWNLOAD_DIR_URL}{TEST_SOURCE_VERSION}/{platform}/{TEST_REGION}/{executable_name}"

    installer_filename = Path(tempdir, Path(executable_url).name)
    installed_app_dir = Path(tempdir, APP_DIR_NAME)
    print(f"Downloading Fx from {executable_url}...")
    response = requests.get(executable_url)
    if 199 < response.status_code < 300:
        print(f"Download successful, status {response.status_code}")
    with open(installer_filename, "wb") as fh:
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
@CommandArgument("--binary_path", help="Firefox executable path is needed")
@CommandArgument("--test_type", help="Base/Background")
def build(command_context, binary_path, test_type, **kwargs):
    tempdir = tempfile.TemporaryDirectory()
    # If we have a symlink to the tmp directory, resolve it
    tempdir_name = str(Path(tempdir.name).resolve())

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
    with open(download_target, "wb") as fh:
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

    if uname().system == "Darwin":
        return copy_macos_channelprefs(tempdir)
    else:
        with Path(binary_dir, "update-settings.ini").open("w") as f:
            f.write("[Settings]\n")
            f.write(f"ACCEPTED_MAR_CHANNEL_IDS={MAR_CHANNEL}")

        with Path(binary_dir, "defaults", "pref", "channel-prefs.js").open("w") as f:
            f.write(f'pref("app.update.channel", "{TEST_UPDATE_CHANNEL}");')

    return binary_path_str
