# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
Shared module for fetching intermittent test failure data from Treeherder and Bugzilla.
"""

import datetime
import re
from typing import Literal, Optional, TypedDict

import requests

USER_AGENT = "mach-intermittent-failures/1.0"


class BugzillaFailure(TypedDict):
    bug_id: int
    bug_count: int


class BugzillaSummary(TypedDict):
    summary: str
    id: int
    status: Optional[str]
    resolution: Optional[str]
    creation_time: Optional[str]
    last_change_time: Optional[str]
    comment_count: Optional[int]


class IntermittentFailure(TypedDict):
    bug_id: int
    failure_count: int
    summary: str
    status: str
    resolution: str
    test_path: Optional[str]
    creation_time: Optional[str]
    last_change_time: Optional[str]
    comment_count: Optional[int]


class IntermittentFailuresFetcher:
    """Fetches intermittent test failure data from Treeherder and Bugzilla APIs."""

    def __init__(self, days: int = 7, threshold: int = 30, verbose: bool = False):
        self.days = days
        self.threshold = threshold
        self.verbose = verbose
        self.end_date = datetime.datetime.now()
        self.start_date = self.end_date - datetime.timedelta(days=self.days)

    def get_failures(self, branch: str = "trunk") -> list[IntermittentFailure]:
        """
        Fetch intermittent failures that meet the threshold.

        Returns a list of intermittent failures with bug information.
        """
        bugzilla_failures = self._get_bugzilla_failures(branch)
        bug_list = self._keep_bugs_above_threshold(bugzilla_failures)

        if not bug_list:
            return []

        bug_summaries = self._get_bugzilla_summaries(bug_list)

        results = []
        for bug in bug_summaries:
            bug_id = bug["id"]
            if bug_id in bug_list:
                result: IntermittentFailure = {
                    "bug_id": bug_id,
                    "failure_count": self._get_failure_count(bugzilla_failures, bug_id),
                    "summary": bug["summary"],
                    "status": bug.get("status", "UNKNOWN"),
                    "resolution": bug.get("resolution", ""),
                    "test_path": None,
                    "creation_time": bug.get("creation_time"),
                    "last_change_time": bug.get("last_change_time"),
                    "comment_count": bug.get("comment_count"),
                }

                if "single tracking bug" in bug["summary"]:
                    match = re.findall(
                        r" ([^\s]+\/?\.[a-z0-9-A-Z]+) \|", bug["summary"]
                    )
                    if match:
                        result["test_path"] = match[0]

                results.append(result)

        return results

    def get_single_tracking_bugs_with_paths(
        self, branch: str = "trunk"
    ) -> list[tuple[int, str]]:
        """
        Get only single tracking bugs that have test paths.
        This is what high_freq_skipfails uses.

        Returns a list of (bug_id, test_path) tuples.
        """
        failures = self.get_failures(branch)

        results = []
        for failure in failures:
            if failure["test_path"] and "single tracking bug" in failure["summary"]:
                results.append((failure["bug_id"], failure["test_path"]))

        return results

    def _get_bugzilla_failures(self, branch: str = "trunk") -> list[BugzillaFailure]:
        """Fetch failure data from Treeherder API."""
        url = (
            f"https://treeherder.mozilla.org/api/failures/"
            f"?startday={self.start_date.date()}&endday={self.end_date.date()}"
            f"&tree={branch}&failurehash=all"
        )
        if self.verbose:
            print(f"[DEBUG] Fetching failures from Treeherder: {url}")
        response = requests.get(url, headers={"User-agent": USER_AGENT})
        response.raise_for_status()

        return [
            item
            for item in response.json()
            if "bug_id" in item and isinstance(item["bug_id"], int)
        ]

    def _keep_bugs_above_threshold(
        self, failure_list: list[BugzillaFailure]
    ) -> list[int]:
        """Filter bugs that have failure counts above the threshold."""
        if not failure_list:
            return []

        bug_counts = {}
        for failure in failure_list:
            bug_id = failure["bug_id"]
            bug_counts[bug_id] = bug_counts.get(bug_id, 0) + failure.get("bug_count", 1)

        return [
            bug_id for bug_id, count in bug_counts.items() if count >= self.threshold
        ]

    def _get_failure_count(
        self, failure_list: list[BugzillaFailure], bug_id: int
    ) -> int:
        """Get the total failure count for a specific bug."""
        total = 0
        for failure in failure_list:
            if failure["bug_id"] == bug_id:
                total += failure.get("bug_count", 1)
        return total

    def _get_bugzilla_summaries(self, bug_id_list: list[int]) -> list[BugzillaSummary]:
        """Fetch bug summaries from Bugzilla REST API."""
        if not bug_id_list:
            return []

        url = (
            f"https://bugzilla.mozilla.org/rest/bug"
            f"?include_fields=summary,id,status,resolution,creation_time,last_change_time,comment_count"
            f"&id={','.join(str(id) for id in bug_id_list)}"
        )
        if self.verbose:
            print(f"[DEBUG] Fetching bug details from Bugzilla: {url}")
        response = requests.get(url, headers={"User-agent": USER_AGENT})
        response.raise_for_status()

        json_response: dict[Literal["bugs"], list[BugzillaSummary]] = response.json()
        return json_response.get("bugs", [])
