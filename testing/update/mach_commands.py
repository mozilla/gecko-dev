# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, # You can obtain one at http://mozilla.org/MPL/2.0/.

import logging
import subprocess
import sys
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
INSTALLED_APP_DIR = "fx_test"


def setup_update_argument_parser():
    from marionette_harness.runtests import MarionetteArguments
    from mozlog.structured import commandline

    parser = MarionetteArguments()
    commandline.add_logging_group(parser)

    return parser


def get_binary_path(**kwargs):
    executable_url = subprocess.check_output(
        [
            "python3",
            "testing/update/collect_executables.py",
            TEST_REGION,
            TEST_SOURCE_VERSION,
        ],
        text=True,
    ).strip()
    installer_filename = Path(executable_url).name
    print(f"Downloading Fx from {executable_url}...")
    response = requests.get(executable_url)
    if 199 < response.status_code < 300:
        print(f"Download successful, status {response.status_code}")
    with open(installer_filename, "wb") as fh:
        fh.write(response.content)
    fx_location = mozinstall.install(installer_filename, INSTALLED_APP_DIR)
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
def build(command_context, binary_path, **kwargs):
    try:
        if not binary_path:
            kwargs["binary"] = str(get_binary_path(**kwargs))
        else:
            kwargs["binary"] = binary_path
    except BinaryNotFoundException as e:
        command_context.log(
            logging.ERROR,
            "update-test",
            {"error": str(e)},
            "ERROR: {error}",
        )
        command_context.log(logging.INFO, "update-test", {"help": e.help()}, "{help}")
        return 1

    kwargs["binary"] = str(set_up(kwargs["binary"]))
    return run_tests(topsrcdir=command_context.topsrcdir, **kwargs)


def run_tests(binary=None, topsrcdir=None, **kwargs):
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
            "testing/update/manifest.toml",
        )
    ]
    args.gecko_log = "-"

    parser = setup_update_argument_parser()
    parser.verify_usage(args)

    failed = MarionetteHarness(MarionetteTestRunner, args=vars(args)).run()
    if failed > 0:
        return 1
    return 0


def install_macos_binary(binary_path):
    executable_path = Path(binary_path)

    bz_channelprefs_link = "https://bugzilla.mozilla.org/attachment.cgi?id=9417387"

    resp = requests.get(bz_channelprefs_link)
    with open("channelprefs.zip", "wb") as fh:
        fh.write(resp.content)

    unpack_archive("channelprefs.zip", "channelprefs")
    src = Path("channelprefs", TEST_UPDATE_CHANNEL)
    dst = Path(INSTALLED_APP_DIR, "Contents", "Frameworks")

    # Cannot write on /Volumes, copy to a local dir
    copytree(
        Path(executable_path),
        "Firefox.app",
        dirs_exist_ok=True,
    )

    Path("Firefox.app").chmod(455)  # rwx for all users

    copytree(
        Path(src, "ChannelPrefs.framework"),
        Path(dst, "ChannelPrefs.framework"),
        dirs_exist_ok=True,
    )

    # test against the binary that was copied to local
    fx_executable = Path("Firefox.app", "Contents", "MacOS", "firefox")
    return fx_executable


def set_up(binary_path):
    # Set channel prefs for all OS targets
    binary_path_str = mozinstall.get_binary(binary_path, "Firefox")
    print(f"Binary path: {binary_path_str}")
    binary_dir = Path(binary_path_str).absolute().parent

    if uname().system == "Darwin":
        return install_macos_binary(binary_path)
    else:
        with Path(binary_dir, "update-settings.ini").open("w") as f:
            f.write("[Settings]\n")
            f.write(f"ACCEPTED_MAR_CHANNEL_IDS={MAR_CHANNEL}")

        with Path(binary_dir, "defaults", "pref", "channel-prefs.js").open("w") as f:
            f.write(f'pref("app.update.channel", "{TEST_UPDATE_CHANNEL}");')

    return binary_path_str
