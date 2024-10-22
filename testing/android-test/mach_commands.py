# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Integrates android tests with mach

import os

from mach.decorators import Command, CommandArgument


def classname_for_test(test, test_path):
    """Convert path of test file to gradle recognized test suite name"""
    return (
        os.path.normpath(test)
        .split(os.path.normpath(test_path))[-1]
        .removeprefix(os.path.sep)
        .replace(os.path.sep, ".")
        .removesuffix(".kt")
    )


@Command(
    "android-test",
    category="testing",
    description="Run Android tests.",
)
@CommandArgument(
    "--subproject",
    default="fenix",
    choices=["fenix", "focus"],
    help="Android subproject to run tests for.",
)
@CommandArgument(
    "--test",
    default=None,
    help="Test to run",
)
def run_android_test(command_context, subproject, test=None, test_objects=[], **kwargs):
    if subproject == "fenix":
        gradle_subcommand = ":fenix:testDebug"
        test_path = os.path.join(
            "mobile", "android", "fenix", "app", "src", "test", "java"
        )
    elif subproject == "focus":
        gradle_subcommand = ":focus-android:testFocusDebugUnitTest"
        test_path = os.path.join(
            "mobile",
            "android",
            "focus-android",
            "app",
            "src",
            "test",
            "java",
        )
    else:
        return None
    test_classes = []
    for test_object in test_objects:
        test_classes.append("--tests")
        test_classes.append(classname_for_test(test_object["name"], test_path))
    if test:
        test_classes.append("--tests")
        test_classes.append(classname_for_test(test, test_path))
    return command_context._mach_context.commands.dispatch(
        "gradle",
        command_context._mach_context,
        args=[gradle_subcommand, "-q"] + test_classes,
    )
