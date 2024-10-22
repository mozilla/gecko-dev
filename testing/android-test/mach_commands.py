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


def project_for_ac(test, test_path):
    """Get project name for android-component subprojects from path of test file"""
    dir = os.path.normpath("mobile/android/android-components/components")
    return (
        os.path.normpath(test)
        .split(os.path.normpath(dir))[-1]
        .split(os.path.normpath(test_path))[0]
        .removeprefix(os.path.sep)
        .removesuffix(os.path.sep)
        .replace(os.path.sep, "-")
    )


@Command(
    "android-test",
    category="testing",
    description="Run Android tests.",
)
@CommandArgument(
    "--subproject",
    default="fenix",
    choices=["fenix", "focus", "android-components", "ac"],
    help="Android subproject to run tests for.",
)
@CommandArgument(
    "--test",
    default=None,
    help="Test to run",
)
def run_android_test(command_context, subproject, test=None, test_objects=[], **kwargs):
    gradle_command = []
    AC = ("android-components", "ac")
    if subproject == "fenix":
        gradle_command = [":fenix:testDebug"]
        test_path = os.path.join(
            "mobile", "android", "fenix", "app", "src", "test", "java"
        )
    elif subproject == "focus":
        gradle_command = [":focus-android:testFocusDebugUnitTest"]
        test_path = os.path.join(
            "mobile",
            "android",
            "focus-android",
            "app",
            "src",
            "test",
            "java",
        )
    elif subproject in AC:
        if not test_objects or test:
            return command_context._mach_context.commands.dispatch(
                "gradle",
                command_context._mach_context,
                args=["-q", "test"],
                topsrcdir=os.path.join(
                    command_context.topsrcdir, "mobile", "android", "android-components"
                ),
            )
        test_path = os.path.join("src", "test", "java")
    else:
        return None
    for test_object in test_objects:
        if subproject in AC:
            gradle_command.append(
                ":"
                + project_for_ac(test_object["name"], test_path)
                + ":testDebugUnitTest"
            )
        gradle_command.append("--tests")
        gradle_command.append(classname_for_test(test_object["name"], test_path))
    if test:
        if subproject in AC:
            gradle_command.append(
                ":" + project_for_ac(test, test_path) + ":testDebugUnitTest"
            )
        gradle_command.append("--tests")
        gradle_command.append(classname_for_test(test, test_path))
    return command_context._mach_context.commands.dispatch(
        "gradle",
        command_context._mach_context,
        args=["-q"] + gradle_command,
    )
