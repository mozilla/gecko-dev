# -*- Mode: python; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 40 -*-
# vim: set filetype=python:
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import signal
import subprocess

from mozlint import result

PRETTIER_ERROR_MESSAGE = """
An error occurred running prettier. Please check the following error messages:

{}
""".strip()

PRETTIER_FORMATTING_MESSAGE = (
    "This file needs formatting with Prettier (use 'mach lint --fix <path>')."
)


def run_prettier(cmd_args, config, fix):
    shell = False
    if is_windows():
        # The eslint binary needs to be run from a shell with msys
        shell = True
    encoding = "utf-8"

    orig = signal.signal(signal.SIGINT, signal.SIG_IGN)
    proc = subprocess.Popen(
        cmd_args, shell=shell, stdout=subprocess.PIPE, stderr=subprocess.PIPE
    )
    signal.signal(signal.SIGINT, orig)

    try:
        output, errors = proc.communicate()
    except KeyboardInterrupt:
        proc.kill()
        return {"results": [], "fixed": 0}

    results = []

    if errors:
        errors = errors.decode(encoding, "replace").strip().split("\n")
        errors = [
            error
            for error in errors
            # Unknown options are not an issue for Prettier, this avoids
            # errors during tests.
            if not ("Ignored unknown option" in error)
        ]
        if len(errors):
            results.append(
                result.from_config(
                    config,
                    **{
                        "name": "eslint",
                        "path": os.path.abspath("."),
                        "message": PRETTIER_ERROR_MESSAGE.format("\n".join(errors)),
                        "level": "error",
                        "rule": "prettier",
                        "lineno": 0,
                        "column": 0,
                    }
                )
            )

    if not output:
        # If we have errors, but no output, we assume something really bad happened.
        if errors and len(errors):
            return {"results": results, "fixed": 0}

        return {"results": [], "fixed": 0}  # no output means success

    output = output.decode(encoding, "replace").splitlines()

    fixed = 0

    if fix:
        # When Prettier is running in fix mode, it outputs the list of files
        # that have been fixed, so sum them up here.
        # If it can't fix files, it will throw an error, which will be handled
        # above.
        fixed = len(output)
    else:
        # When in "check" mode, Prettier will output the list of files that
        # need changing, so we'll wrap them in our result structure here.
        for file in output:
            if not file:
                continue

            file = os.path.abspath(file)
            results.append(
                result.from_config(
                    config,
                    **{
                        "name": "eslint",
                        "path": file,
                        "message": PRETTIER_FORMATTING_MESSAGE,
                        "level": "error",
                        "rule": "prettier",
                        "lineno": 0,
                        "column": 0,
                    }
                )
            )

    return {"results": results, "fixed": fixed}


def is_windows():
    return (
        os.environ.get("MSYSTEM") in ("MINGW32", "MINGW64")
        or "MOZILLABUILD" in os.environ
    )
