# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, # You can obtain one at http://mozilla.org/MPL/2.0/.


import logging
import os
import sys

import six
from mach.decorators import Command
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
def build(command_context, **kwargs):
    try:
        if not kwargs["binary"]:
            kwargs["binary"] = command_context.get_binary_path("app")
    except BinaryNotFoundException as e:
        command_context.log(
            logging.ERROR,
            "update-test",
            {"error": str(e)},
            "ERROR: {error}",
        )
        command_context.log(logging.INFO, "update-test", {"help": e.help()}, "{help}")
        return 1
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
