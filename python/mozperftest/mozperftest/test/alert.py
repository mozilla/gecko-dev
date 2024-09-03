# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import json
import sys

import mozprocess
import requests

from mozperftest.layers import Layer

TREEHERDER_ALERT_TASKS_URL = (
    "https://treeherder.mozilla.org/api/performance/alertsummary/?id={}"
)

TASKCLUSTER_TASK_INFO = (
    "https://firefox-ci-tc.services.mozilla.com/api/queue/v1/task/{}"
)


class MachTestData:
    def open_data(self, data):
        return {
            "name": "xpcshell",
            "subtest": data["name"],
            "data": [
                {"file": "xpcshell", "value": value, "xaxis": xaxis}
                for xaxis, value in enumerate(data["values"])
            ],
        }

    def transform(self, data):
        return data

    merge = transform


class MachTestCommand:
    mach_command = ""

    def get_test_name(series_signature):
        raise NotImplementedError("Unknown framework, test setup not implemented")

    def setup_test(series_signature):
        raise NotImplementedError("Unknown framework, test setup not implemented")

    def get_exact_options(command, series_signature):
        raise NotImplementedError("Unknown framework, test setup not implemented")


class RaptorTestCommand:
    mach_command = "raptor"

    def get_test_name(series_signature):
        return series_signature["suite"]

    def setup_test(series_signature):
        return [
            "./mach",
            RaptorTestCommand.mach_command,
            "-t",
            series_signature["suite"],
        ]

    def get_exact_options(command, series_signature):
        from raptor.cmdline import create_parser

        ind = -1
        while (ind := ind + 1) < len(command):
            if "raptor" in command[ind]:
                break

        raptor_parser = create_parser()
        args, _ = raptor_parser.parse_known_args(command[ind:])

        args_list = []
        for action in raptor_parser._actions:
            arg = action.dest
            if not hasattr(args, arg):
                continue

            value = getattr(args, arg)
            default_value = action.default

            if "MOZ_FETCHES_DIR" in str(value):
                # Skip CI-specific options
                continue
            if arg == "t" or arg == "test":
                # Don't add the test argument, it's added
                # by setup_test
                continue
            if value == default_value:
                # Skip any options that are the same
                # as the default
                continue

            if isinstance(value, bool):
                args_list.append(action.option_strings[0])
            elif isinstance(value, list):
                for setting in value:
                    args_list.extend([action.option_strings[0], str(setting)])
            else:
                args_list.extend([action.option_strings[0], str(value)])

        return RaptorTestCommand.setup_test(series_signature) + args_list


class TalosTestCommand:
    mach_command = "talos-test"

    def get_test_name(series_signature):
        if "pdfpaint" in series_signature["suite"]:
            return "-".join([series_signature["suite"], series_signature["test"]])
        return series_signature["suite"]

    def setup_test(series_signature):
        test = [
            "./mach",
            TalosTestCommand.mach_command,
            "-a",
            series_signature["suite"],
        ]
        if series_signature["suite"] == "pdfpaint":
            test.extend(["--pdfPaintName", series_signature["test"]])
        return test

    def get_exact_options(command, series_signature):
        from talos.cmdline import create_parser

        ind = -1
        while (ind := ind + 1) < len(command):
            if "talos_script" in command[ind]:
                break

        talos_parser = create_parser()
        for action in talos_parser._actions:
            action.required = False

        args, _ = talos_parser.parse_known_args(command[ind:])

        args_list = []
        for action in talos_parser._actions:
            arg = action.dest
            if not hasattr(args, arg):
                continue

            value = getattr(args, arg)
            default_value = action.default

            if "MOZ_FETCHES_DIR" in str(value):
                # Skip CI-specific options
                continue
            if arg == "suite":
                # Don't add the test argument, it's added
                # by setup_test
                continue
            if value == default_value:
                # Skip any options that are the same
                # as the default
                continue

            if isinstance(value, bool):
                args_list.append(action.option_strings[0])
            elif isinstance(value, list):
                for setting in value:
                    args_list.extend([action.option_strings[0], str(setting)])
            else:
                args_list.extend([action.option_strings[0], str(value)])

        return TalosTestCommand.setup_test(series_signature) + args_list


class AwsyTestCommand:
    mach_command = "awsy-test"

    def get_test_name(series_signature):
        if "Base " in series_signature["suite"]:
            return "awsy-base"
        return "awsy-tp6"

    def setup_test(series_signature):
        test = ["./mach", AwsyTestCommand.mach_command]
        if "Base " in series_signature["suite"]:
            return test + ["--base"]
        else:
            return test + ["--tp6"]

    def get_exact_options(command, series_signature):
        ind = -1
        while (ind := ind + 1) < len(command):
            if "--cfg" in command[ind]:
                break

        parsed_command = []
        option_ind = ind + 1
        while (option_ind := option_ind + 1) < len(command):
            option = command[option_ind]
            if option in ("--requires-gpu", "--base", "--tp6"):
                continue
            if option in ("--download-symbols"):
                option_ind += 1
                continue
            parsed_command.append(option)

        return AwsyTestCommand.setup_test(series_signature) + parsed_command


class MozPerftestTestCommand:
    mach_command = "perftest"
    exact_options_used = False

    def get_test_name(series_signature):
        return series_signature["suite"]

    def setup_test(series_signature):
        if not MozPerftestTestCommand.exact_options_used:
            raise Exception(
                "Mozperftest tests can only be run with exact options. Rerun "
                "the same command with the `--alert-exact` option to fix this."
            )
        return [MozPerftestTestCommand.mach_command]

    def get_exact_options(command, series_signature):
        from mozperftest.argparser import PerftestArgumentParser

        MozPerftestTestCommand.exact_options_used = True

        command_str = command
        if isinstance(command, list):
            command_str = " ".join(command)

        # Get all the runner calls (may be multiple), first entry
        # is ignored since it's pre-amble for taskcluster runs
        runner_calls = []
        cmds = command_str.split("&&")[1:]
        for cmd in cmds:
            if not "runner.py" in cmd:
                continue
            runner_calls.append(cmd.split("runner.py")[-1])

        tests_to_run = []
        perftest_parser = PerftestArgumentParser()
        for runner_call in runner_calls:
            runner_opts = runner_call.strip().split()
            args, _ = perftest_parser.parse_known_args(runner_opts)

            args_list = []
            for action in perftest_parser._actions:
                arg = action.dest
                if not hasattr(args, arg):
                    continue

                value = getattr(args, arg)
                default_value = action.default

                if "MOZ_FETCHES_DIR" in str(value):
                    # Skip CI-specific options
                    continue
                if value == default_value:
                    # Skip any options that are the same
                    # as the default
                    continue

                if isinstance(value, bool):
                    args_list.append(action.option_strings[0])
                elif isinstance(value, list):
                    for setting in value:
                        if len(action.option_strings) == 0:
                            # Positional arguments that can be set multiple times
                            # such as the test are added here
                            args_list.append(setting)
                        else:
                            args_list.extend([action.option_strings[0], str(setting)])
                else:
                    args_list.extend([action.option_strings[0], str(value)])

            tests_to_run.append(
                ["./mach", MozPerftestTestCommand.mach_command] + args_list
            )

        return tests_to_run


class AlertTestRunner(Layer):
    """Runs an xpcshell test."""

    name = "alert"
    activated = True

    arguments = {
        "exact": {
            "action": "store_true",
            "default": False,
            "help": (
                "Use the exact same command as the one that was used to "
                "run the test in CI. The options for the command are retrieved from "
                "the task which triggered the alert. "
            ),
        },
    }

    def __init__(self, env, mach_cmd):
        super(AlertTestRunner, self).__init__(env, mach_cmd)
        self.perfherder_data = {}

    def _get_task_info(self, task_id):
        task_info_req = requests.get(
            TASKCLUSTER_TASK_INFO.format(task_id),
            headers={"User-Agent": "mozilla-central"},
        )

        if task_info_req.status_code != 200:
            print(
                "\nFailed to obtain task info due to:\n"
                f"Task ID: {task_id}\n"
                f"Status Code: {task_info_req.status_code}\n"
                f"Response Message: {task_info_req.json()}\n"
            )
            task_info_req.raise_for_status()

        return task_info_req.json()

    def _get_exact_options(self, task_id, series_signature, test_command_klass):
        task_info = self._get_task_info(task_id)

        commands = []
        payload_commands = task_info["payload"]["command"]
        if len(payload_commands) == 1 and isinstance(payload_commands[0], str):
            commands = payload_commands[0].split()
        else:
            for cmd in payload_commands:
                commands.extend(cmd)

        return test_command_klass.get_exact_options(commands, series_signature)

    def _get_framework_klass(self, framework_number):
        return {
            "1": TalosTestCommand,
            "4": AwsyTestCommand,
            "13": RaptorTestCommand,
            "15": MozPerftestTestCommand,
        }.get(str(framework_number), MachTestCommand)

    def _get_framework_commands(self, alert_info):
        framework_commands = {}

        all_alerts = []
        for results in alert_info["results"]:
            all_alerts.extend(results["alerts"] + results["related_alerts"])

        for alert in all_alerts:
            framework_command = []
            series_signature = alert["series_signature"]

            test_command_klass = self._get_framework_klass(
                series_signature["framework_id"]
            )
            test = test_command_klass.get_test_name(series_signature)
            machine_platform = series_signature["machine_platform"]

            platform = "desktop"
            if "android" in machine_platform:
                platform = "android"
            if (test, platform) in framework_commands:
                continue

            if platform != "android" and (
                ("mac" in machine_platform and "darwin" not in sys.platform)
                or ("win" in machine_platform and "win" not in sys.platform)
                or ("linux" in machine_platform and "linux" not in sys.platform)
            ):
                self.warning(
                    f"Local platform doesn't match the alerting platform. "
                    f"Regression for {test} may not be reproducible on your machine."
                )

            if self.get_arg("alert_exact"):
                exact_options = self._get_exact_options(
                    alert["taskcluster_metadata"]["task_id"],
                    series_signature,
                    test_command_klass,
                )
                self.info(f"Found the following options for {test}: {exact_options}")

                if isinstance(exact_options[0], list):
                    framework_command.extend(exact_options)
                else:
                    framework_command.append(exact_options)
            else:
                framework_command.append(
                    test_command_klass.setup_test(alert["series_signature"])
                )

            framework_commands[(test, platform)] = framework_command

        return framework_commands

    def _get_alert(self, alert_summary_id):
        alert_info_req = requests.get(
            TREEHERDER_ALERT_TASKS_URL.format(alert_summary_id),
            headers={"User-Agent": "mozilla-central"},
        )
        if alert_info_req.status_code != 200:
            print(
                "\nFailed to obtain tasks from alert due to:\n"
                f"Alert ID: {alert_summary_id}\n"
                f"Status Code: {alert_info_req.status_code}\n"
                f"Response Message: {alert_info_req.json()}\n"
            )
            alert_info_req.raise_for_status()

        return alert_info_req.json()

    def create_line_handler(self, test):
        def _line_handler(proc, line):
            """This function acts as a bridge between the test harnesses,
            and mozperftest. It's used to capture all the PERFHERDER_DATA
            output from them."""

            # NOTE: this hack is to workaround encoding issues on windows
            line = line.replace(b"\xcf\x83", b"")

            line = line.decode("utf-8")
            if not line.strip():
                return

            self.info(line.strip().replace("{", "{{").replace("}", "}}"))
            if "PERFHERDER_DATA: " in line:
                self.perfherder_data.setdefault(test, []).append(
                    json.loads(line.strip().split("PERFHERDER_DATA: ")[-1])
                )

        return _line_handler

    def run(self, metadata):
        alert_summary_id = metadata.script.script
        alert_info = self._get_alert(alert_summary_id)
        framework_commands = self._get_framework_commands(alert_info)

        # Run the tests
        failed_commands = []
        for (test, platform), cmds in framework_commands.items():
            if "android" in platform:
                self.info("WARNING: The next test runs on android.")

            for cmd in cmds:
                self.info(f"Running command for {test} on {platform} platform: {cmd}")

                mozprocess.run_and_wait(
                    cmd,
                    output_line_handler=self.create_line_handler(test),
                    text=False,
                )
                if test not in self.perfherder_data:
                    failed_commands.append(
                        {
                            "cmd": cmd,
                            "test": test,
                        }
                    )

        # Output results in a more readable manner
        for test, perfherder_data in self.perfherder_data.items():
            self.info(f"\nResults for {test}:")

            for subtest in perfherder_data:
                self.info(str(subtest).replace("{", "{{").replace("}", "}}"))

            self.info("")

        # Output a message stating that a command failed, and provide
        # the command so users can re-run outside of this tool
        for failed_command in failed_commands:
            self.warning(
                f"Failed to run command for {failed_command['test']}: "
                f"{' '.join(failed_command['cmd'])}"
            )
            self.warning("No PERFHERDER_DATA output was detected for it.\n")

        return metadata
