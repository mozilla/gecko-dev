# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, # You can obtain one at http://mozilla.org/MPL/2.0/.

import logging
import os
import sys
from pathlib import Path
from platform import uname

import six
from mach.decorators import Command, CommandArgument
from marionette_harness.runtests import MarionetteHarness, MarionetteTestRunner
from mozbuild.base import BinaryNotFoundException
from mozlog.structured import commandline


def setup_update_argument_parser():
    from marionette_harness.runtests import MarionetteArguments
    from mozlog.structured import commandline

    parser = MarionetteArguments()
    commandline.add_logging_group(parser)

    return parser


@Command(
    "update-test",
    category="testing",
    description="Test if the version can be updated to the latest patch successfully,",
    parser=setup_update_argument_parser,
)
@CommandArgument("--binary_path", help="Firefox executable path is needed")
def build(command_context, binary_path, **kwargs):
    try:
        if not binary_path:
            kwargs["binary"] = command_context.get_binary_path("app")
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

    set_up(kwargs["binary"])
    return run_tests(topsrcdir=command_context.topsrcdir, **kwargs)


def run_tests(binary=None, topsrcdir=None, **kwargs):
    from argparse import Namespace

    args = Namespace()
    args.binary = binary
    args.logger = kwargs.pop("log", None)
    if not args.logger:
        args.logger = commandline.setup_logging(
            "Update Tests", args, {"mach": sys.stdout}
        )

    for k, v in six.iteritems(kwargs):
        setattr(args, k, v)

    args.tests = [
        os.path.join(
            topsrcdir,
            "testing/update/manifest.toml",
        )
    ]

    parser = setup_update_argument_parser()
    parser.verify_usage(args)

    failed = MarionetteHarness(MarionetteTestRunner, args=vars(args)).run()
    if failed > 0:
        return 1
    return 0


def set_up(binary_path):
    executable_dir = Path(binary_path).parent.absolute()

    if uname() == "Darwin":
        pass

    else:

        with open(os.path.join(executable_dir, "update-settings.ini"), "w") as f:
            f.write("[Settings]\n")
            f.write("ACCEPTED_MAR_CHANNEL_IDS=firefox-mozilla-central")

        with open(
            os.path.join(executable_dir, "defaults", "pref", "channel-prefs.js"), "w"
        ) as f:
            f.write('pref("app.update.channel", "nightlytest");')
