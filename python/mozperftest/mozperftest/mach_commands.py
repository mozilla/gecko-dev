# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import json
import os
import pathlib
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

    from pathlib import Path

    from mozperftest.script import ParseError, ScriptInfo, ScriptType

    # user selection with fuzzy UI
    from mozperftest.utils import ON_TRY

    if not ON_TRY and kwargs.get("tests", []) == []:
        from moztest.resolve import TestResolver

        from mozperftest.fzf.fzf import select

        resolver = command_context._spawn(TestResolver)
        test_objects = list(resolver.resolve_tests(paths=None, flavor="perftest"))
        selected = select(test_objects)

        def full_path(selection):
            __, script_name, __, location = selection.split(" ")
            return str(
                Path(
                    command_context.topsrcdir.rstrip(os.sep),
                    location.strip(os.sep),
                    script_name,
                )
            )

        kwargs["tests"] = [full_path(s) for s in selected]

        if kwargs["tests"] == []:
            print("\nNo selection. Bye!")
            return

    if len(kwargs["tests"]) > 1:
        print("\nSorry no support yet for multiple local perftest")
        return

    # Make sure the default artifacts directory exists
    default_artifact_location = pathlib.Path(command_context.topsrcdir, "artifacts")
    default_artifact_location.mkdir(parents=True, exist_ok=True)

    sel = "\n".join(kwargs["tests"])
    print("\nGood job! Best selection.\n%s" % sel)
    # if the script is xpcshell, we can force the flavor here
    # XXX on multi-selection,  what happens if we have seeveral flavors?
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

    push_to_try = kwargs.pop("push_to_try", False)
    if push_to_try:
        sys.path.append(str(Path(command_context.topsrcdir, "tools", "tryselect")))

        from tryselect.push import push_to_try

        perftest_parameters = {}
        args = script_info.update_args(**original_parser.get_user_args(kwargs))
        platform = args.pop("try_platform", "linux")
        if isinstance(platform, str):
            platform = [platform]

        platform = ["%s-%s" % (plat, script_info.script_type.name) for plat in platform]

        for plat in platform:
            if plat not in _TRY_PLATFORMS:
                # we can extend platform support here: linux, win, macOs
                # by adding more jobs in taskcluster/kinds/perftest/kind.yml
                # then picking up the right one here
                raise NotImplementedError(
                    "%r doesn't exist or is not yet supported" % plat
                )

        def relative(path):
            if path.startswith(command_context.topsrcdir):
                return path[len(command_context.topsrcdir) :].lstrip(os.sep)
            return path

        for name, value in args.items():
            # ignore values that are set to default
            new_val = value
            if original_parser.get_default(name) == value:
                continue
            if name == "tests":
                new_val = [relative(path) for path in value]
            perftest_parameters[name] = new_val

        parameters = {
            "try_task_config": {
                "tasks": [_TRY_PLATFORMS[plat] for plat in platform],
                "perftest-options": perftest_parameters,
            },
            "try_mode": "try_task_config",
        }

        task_config = {"parameters": parameters, "version": 2}
        if args.get("verbose"):
            print("Pushing run to try...")
            print(json.dumps(task_config, indent=4, sort_keys=True))

        push_to_try("perftest", "perftest", try_task_config=task_config)
        return

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
