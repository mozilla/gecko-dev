# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import os
import sys
from functools import partial

from mach.decorators import Command, CommandArgument, SubCommand
from mozbuild.base import MachCommandConditions as conditions

_TRY_PLATFORMS = {
    "linux-xpcshell": "perftest-linux-try-xpcshell",
    "mac-xpcshell": "perftest-macosx-try-xpcshell",
    "linux-browsertime": "perftest-linux-try-browsertime",
    "mac-browsertime": "perftest-macosx-try-browsertime",
    "win-browsertimee": "perftest-windows-try-browsertime",
}


HERE = os.path.dirname(__file__)


def get_perftest_parser():
    from mozperftest import PerftestArgumentParser

    return PerftestArgumentParser


def get_perftest_tools_parser(tool):
    def tools_parser_func():
        from mozperftest import PerftestToolsArgumentParser

        PerftestToolsArgumentParser.tool = tool
        return PerftestToolsArgumentParser

    return tools_parser_func


def get_parser():
    return run_perftest._mach_command._parser


@Command(
    "perftest",
    category="testing",
    conditions=[partial(conditions.is_buildapp_in, apps=["firefox", "android"])],
    description="Run any flavor of perftest",
    parser=get_perftest_parser,
)
def run_perftest(command_context, **kwargs):
    # original parser that brought us there
    original_parser = get_parser()

    from mozperftest.script import ParseError, ScriptInfo, ScriptType

    # Refer people to the --help command if they are lost
    if not kwargs["tests"] or kwargs["tests"] == ["help"]:
        print("No test selected!\n")
        print("See `./mach perftest --help` for more info\n")
        return

    if len(kwargs["tests"]) > 1:
        print("\nSorry no support yet for multiple local perftest")
        return

    # if the script is xpcshell, we can force the flavor here
    # XXX on multi-selection,  what happens if we have several flavors?
    try:
        script_info = ScriptInfo(kwargs["tests"][0])
    except ParseError as e:
        if e.exception is IsADirectoryError:
            script_info = None
        else:
            raise
    else:
        if script_info.script_type == ScriptType.xpcshell:
            kwargs["flavor"] = script_info.script_type.name
        elif script_info.script_type == ScriptType.alert:
            kwargs["flavor"] = script_info.script_type.name
        elif "flavor" not in kwargs:
            # we set the value only if not provided (so "mobile-browser"
            # can be picked)
            kwargs["flavor"] = "desktop-browser"

    from mozperftest.runner import run_tests

    run_tests(command_context, kwargs, original_parser.get_user_args(kwargs))

    print("\nFirefox. Fast For Good.\n")


@Command(
    "perftest-test",
    category="testing",
    description="Run perftest tests",
    virtualenv_name="perftest-test",
)
@CommandArgument(
    "tests", default=None, nargs="*", help="Tests to run. By default will run all"
)
@CommandArgument(
    "-s",
    "--skip-linters",
    action="store_true",
    default=False,
    help="Skip flake8 and black",
)
@CommandArgument(
    "-v", "--verbose", action="store_true", default=False, help="Verbose mode"
)
@CommandArgument(
    "-r",
    "--raptor",
    action="store_true",
    default=False,
    help="Run raptor tests",
)
def run_tests(command_context, **kwargs):
    from pathlib import Path

    from mozperftest.utils import temporary_env

    COVERAGE_RCFILE = str(Path(HERE, ".mpt-coveragerc"))
    if kwargs.get("raptor", False):
        print("Running raptor unit tests through mozperftest")
        COVERAGE_RCFILE = str(Path(HERE, ".raptor-coveragerc"))

    with temporary_env(COVERAGE_RCFILE=COVERAGE_RCFILE, RUNNING_TESTS="YES"):
        _run_tests(command_context, **kwargs)


def _run_tests(command_context, **kwargs):
    from pathlib import Path

    from mozperftest.utils import ON_TRY, checkout_python_script, checkout_script

    venv = command_context.virtualenv_manager
    skip_linters = kwargs.get("skip_linters", False)
    verbose = kwargs.get("verbose", False)

    if not ON_TRY and not skip_linters and not kwargs.get("raptor"):
        cmd = "./mach lint "
        if verbose:
            cmd += " -v"
        cmd += " " + str(HERE)
        if not checkout_script(cmd, label="linters", display=verbose, verbose=verbose):
            raise AssertionError("Please fix your code.")

    # running pytest with coverage
    # coverage is done in three steps:
    # 1/ coverage erase => erase any previous coverage data
    # 2/ coverage run pytest ... => run the tests and collect info
    # 3/ coverage report => generate the report
    tests_dir = Path(HERE, "tests").resolve()

    tests = kwargs.get("tests", [])
    if tests == []:
        tests = str(tests_dir)
        run_coverage_check = not skip_linters
    else:
        run_coverage_check = False

        def _get_test(test):
            if Path(test).exists():
                return str(test)
            return str(tests_dir / test)

        tests = " ".join([_get_test(test) for test in tests])

    # on macOS + try we skip the coverage
    # because macOS workers prevent us from installing
    # packages from PyPI
    if sys.platform == "darwin" and ON_TRY:
        run_coverage_check = False

    options = "-xs"
    if kwargs.get("verbose"):
        options += "v"

    # If we run mozperftest with the --raptor argument,
    # then only run the raptor unit tests
    if kwargs.get("raptor"):
        run_coverage_check = True
        tests = str(Path(command_context.topsrcdir, "testing", "raptor", "test"))

    if run_coverage_check:
        assert checkout_python_script(
            venv, "coverage", ["erase"], label="remove old coverage data"
        )

    args = ["run", "-m", "pytest", options, "--durations", "10", tests]

    assert checkout_python_script(
        venv, "coverage", args, label="running tests", verbose=verbose
    )
    if run_coverage_check and not checkout_python_script(
        venv, "coverage", ["report"], display=True
    ):
        raise ValueError("Coverage is too low!")


@Command(
    "perftest-tools",
    category="testing",
    description="Run perftest tools",
)
def run_tools(command_context, **kwargs):
    """
    Runs various perftest tools such as the side-by-side generator.
    """
    print("Runs various perftest tools such as the side-by-side generator.")


@SubCommand(
    "perftest-tools",
    "side-by-side",
    description="This tool can be used to generate a side-by-side visualization of two videos. "
    "When using this tool, make sure that the `--test-name` is an exact match, i.e. if you are "
    "comparing  the task `test-linux64-shippable-qr/opt-browsertime-tp6-firefox-linkedin-e10s` "
    "between two revisions, then use `browsertime-tp6-firefox-linkedin-e10s` as the suite name "
    "and `test-linux64-shippable-qr/opt` as the platform.",
    parser=get_perftest_tools_parser("side-by-side"),
)
def run_side_by_side(command_context, **kwargs):
    from mozperftest.runner import run_tools

    kwargs["tool"] = "side-by-side"
    run_tools(command_context, kwargs)


@SubCommand(
    "perftest-tools",
    "change-detector",
    description="This tool can be used to determine if there are differences between two "
    "revisions. It can do either direct comparisons, or searching for regressions in between "
    "two revisions (with a maximum or autocomputed depth).",
    parser=get_perftest_tools_parser("change-detector"),
)
def run_change_detector(command_context, **kwargs):
    from mozperftest.runner import run_tools

    kwargs["tool"] = "change-detector"
    run_tools(command_context, kwargs)
