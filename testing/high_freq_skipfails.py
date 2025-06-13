# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import datetime
import logging
import os
import re
import sys
from pathlib import Path
from typing import Literal, Optional, TypedDict

import requests
from mozci.util.taskcluster import queue
from mozinfo.platforminfo import PlatformInfo
from skipfails import Skipfails

ERROR = "error"
USER_AGENT = "mach-manifest-high-freq-skipfails/1.0"


class FailureByBug(TypedDict):
    task_id: str
    bug_id: int
    job_id: int
    tree: str


class BugzillaSummary(TypedDict):
    summary: str
    id: int


class BugSuggestion(TypedDict):
    path_end: Optional[str]


class TestInfoAllTestsItem(TypedDict):
    manifest: list[str]
    test: str


class TestInfoAllTests(TypedDict):
    tests: dict[str, list[TestInfoAllTestsItem]]


class BugzillaFailure(TypedDict):
    bug_id: int
    bug_count: int


class HighFreqSkipfails:
    "mach manifest high-freq-skip-fails implementation: Update manifests to skip failing tests by looking at recent failures"

    def __init__(self, command_context=None, failures: int = 30, days: int = 7) -> None:
        self.command_context = command_context
        if self.command_context is not None:
            self.topsrcdir = self.command_context.topsrcdir
        else:
            self.topsrcdir = Path(__file__).parent.parent
        self.topsrcdir = os.path.normpath(self.topsrcdir)
        self.component = "high-freq-skip-fails"

        self.failures = failures
        self.days = days

        self.start_date = datetime.datetime.now()
        self.start_date = self.start_date - datetime.timedelta(days=self.days)
        self.end_date = datetime.datetime.now()
        self.test_info_all_tests: Optional[TestInfoAllTests] = None

    def error(self, e):
        if self.command_context is not None:
            self.command_context.log(
                logging.ERROR, self.component, {ERROR: str(e)}, "ERROR: {error}"
            )
        else:
            print(f"ERROR: {e}", file=sys.stderr, flush=True)

    def info(self, e):
        if self.command_context is not None:
            self.command_context.log(
                logging.INFO, self.component, {ERROR: str(e)}, "INFO: {error}"
            )
        else:
            print(f"INFO: {e}", file=sys.stderr, flush=True)

    def run(self):
        self.info(
            f"Fetching bugs with failure count above {self.failures} in the last {self.days} days..."
        )
        bug_list = self.get_bugs_to_inspect()
        if len(bug_list) == 0:
            self.info(
                f"Could not find bugs wih at least {self.failures} failures in the last {self.days}"
            )
            return
        self.info(f"Found {len(bug_list)} bugs to inspect")

        self.info("Fetching test_info_all_tests and caching it...")
        self.test_info_all_tests = self.get_test_info_all_tests()

        manifest_errors: set[tuple[int, str]] = set()

        task_data: dict[str, tuple[int, str, str]] = {}
        for bug_id, test_path in bug_list:
            self.info(f"Getting failures for bug '{bug_id}'...")
            failures_by_bug = self.get_failures_by_bug(bug_id)
            self.info(f"Found {len(failures_by_bug)} failures")
            for failure in failures_by_bug:
                manifest = self.get_manifest_from_path(test_path)
                if manifest:
                    self.info(f"Found manifest '{manifest}' for path '{test_path}'")
                    task_data[failure["task_id"]] = (bug_id, test_path, manifest)
                else:
                    manifest_errors.add((bug_id, test_path))
                    self.error(f"Could not find manifest for path '{test_path}'")

        skipfails = Skipfails(self.command_context, "", True, "disable", True)

        task_list = self.get_task_list([task_id for task_id in task_data])
        for task_id, task in task_list:
            test_setting = task.get("extra", {}).get("test-setting", {})
            if not test_setting:
                continue
            platform_info = PlatformInfo(test_setting)
            (bug_id, test_path, raw_manifest) = task_data[task_id]

            kind, manifest = skipfails.get_kind_manifest(raw_manifest)
            if kind is None or manifest is None:
                self.error(f"Could not resolve kind for manifest {raw_manifest}")
                continue
            skipfails.skip_failure(
                manifest,
                kind,
                test_path,
                task_id,
                platform_info,
                str(bug_id),
                high_freq=True,
            )

        if len(manifest_errors) > 0:
            self.info("\nExecution complete\n")
            self.info("Script encountered errors while fetching manifests:")
            for bug_id, test_path in manifest_errors:
                self.info(
                    f"Bug {bug_id}: Could not find manifest for path '{test_path}'"
                )

    def get_manifest_from_path(self, path: Optional[str]) -> Optional[str]:
        manifest: Optional[str] = None
        if path is not None and self.test_info_all_tests is not None:
            for test_list in self.test_info_all_tests["tests"].values():
                for test in test_list:
                    # FIXME
                    # in case of wpt, we have an incoming path that is a subset of the full test["test"], for example, path could be:
                    # /navigation-api/ordering-and-transition/location-href-canceled.html
                    # but full path as found in test_info_all_tests is:
                    # testing/web-platform/tests/navigation-api/ordering-and-transition/location-href-canceled.html
                    # unfortunately in this case manifest ends up being: /navigation-api/ordering-and-transition
                    if test["test"] == path:
                        manifest = test["manifest"][0]
                        break
                if manifest is not None:
                    break
        return manifest

    def get_bugs_to_inspect(self) -> list[tuple[int, str]]:
        """
        Returns the id of bugs with the required amount of failures in the specified time range.
        Only bugs marked as `single tracking bugs` are taken into account.
        """
        bug_list = self.keep_bugs_above_threshold(self.get_bugzilla_failures())
        if len(bug_list) > 0:
            bugs_with_path = self.keep_single_tracking_bugs_with_path(
                self.get_bugzilla_summaries(bug_list)
            )
            return bugs_with_path
        return []

    def keep_bugs_above_threshold(
        self, failure_list: list[BugzillaFailure]
    ) -> list[int]:
        filtered_failure_list: list[int] = []
        if failure_list is not None:
            filtered_failure_list = [
                item["bug_id"]
                for item in failure_list
                if item["bug_count"] >= self.failures
            ]
        return filtered_failure_list

    def keep_single_tracking_bugs_with_path(
        self, summaries: list[BugzillaSummary]
    ) -> list[tuple[int, str]]:
        valid_bug_list: list[tuple[int, str]] = []
        for summary in summaries:
            if "single tracking bug" in summary["summary"]:
                # Find the tests's relative path in the summary
                # If there is not one, ignore the bug
                match = re.findall(
                    r" ([^\s]+\/?\.[a-z0-9-A-Z]+) \|", summary["summary"]
                )
                if len(match) > 0:
                    valid_bug_list.append((summary["id"], match[0]))
        return valid_bug_list

    #################
    #   API Calls   #
    #################

    def get_bugzilla_summaries(self, bug_id_list: list[int]) -> list[BugzillaSummary]:
        url = f"https://bugzilla.mozilla.org/rest/bug?include_fields=summary,id&id={','.join([str(id) for id in bug_id_list])}"
        response = requests.get(url, headers={"User-agent": USER_AGENT})
        json_response: dict[Literal["bugs"], list[BugzillaSummary]] = response.json()
        return json_response["bugs"]

    def get_bugzilla_failures(self, branch="trunk") -> list[BugzillaFailure]:
        url = f"https://treeherder.mozilla.org/api/failures/?startday={self.start_date.date()}&endday={self.end_date.date()}&tree={branch}&failurehash=all"
        response = requests.get(url, headers={"User-agent": USER_AGENT})
        # Some items may be missing the bug_id. Skip those
        return [
            item
            for item in response.json()
            if "bug_id" in item and isinstance(item["bug_id"], int)
        ]

    def get_failures_by_bug(self, bug: int, branch="trunk") -> list[FailureByBug]:
        url = f"https://treeherder.mozilla.org/api/failuresbybug/?startday={self.start_date.date()}&endday={self.end_date.date()}&tree={branch}&bug={bug}"
        response = requests.get(url, headers={"User-agent": USER_AGENT})
        json_data = response.json()
        return json_data

    def get_task_list(
        self, task_id_list: list[str], branch="trunk"
    ) -> list[tuple[str, object]]:
        tasks_response = queue.tasks({"taskIds": task_id_list})
        if tasks_response is not None:
            task_list = tasks_response["tasks"]
            return [
                (task_id_list[index], task["task"])
                for index, task in enumerate(task_list)
            ]
        return []

    def get_test_info_all_tests(self) -> TestInfoAllTests:
        url = "https://firefox-ci-tc.services.mozilla.com/api/index/v1/task/gecko.v2.mozilla-central.latest.source.test-info-all/artifacts/public/test-info-all-tests.json"
        response = requests.get(url, headers={"User-agent": USER_AGENT})
        json_data = response.json()
        return json_data
