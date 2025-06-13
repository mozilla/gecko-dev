# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import json
import logging
from typing import Optional

import requests

ERROR = "error"
USER_AGENT = "mach-runner-diff/1.0"


class PlatformDiff:

    def __init__(
        self,
        command_context,
        task_id: str,
        replace: Optional[str],
    ) -> None:
        self.command_context = command_context
        self.component = "platform-diff"
        self.task_id = task_id
        try:
            self.replace: Optional[list[str]] = json.loads(replace) if replace else None
        except json.JSONDecodeError:
            self.error(
                f"Invalid JSON supplied to 'replace': '{replace}'. Ignoring parameter."
            )
            self.replace = None

    def error(self, e):
        self.command_context.log(
            logging.ERROR, self.component, {ERROR: str(e)}, "ERROR: {error}"
        )

    def info(self, e):
        self.command_context.log(
            logging.INFO, self.component, {ERROR: str(e)}, "INFO: {error}"
        )

    def run(self):
        tgdiff = self.get_tgdiff()
        total_added, total_removed, new_platforms, missing_platforms = (
            self.get_diff_lines(tgdiff)
        )
        self.info("")
        self.info(f"Total new platforms added: \033[34m{len(total_added)}\033[0m")
        if len(new_platforms) > 0:
            self.info("")
            self.info(
                f"New platforms not replacing old one (\033[34m{len(new_platforms)}\033[0m):"
            )
            self.info("")
            for platform in new_platforms:
                self.info(f"\033[93m{platform}\033[0m")

        self.info("")
        self.info(f"Total old platforms removed: \033[34m{len(total_removed)}\033[0m")
        if len(missing_platforms) > 0:
            self.info("")
            self.info(
                f"Old platforms missing and their suggested new platform equivalent (\033[34m{len(missing_platforms)}\033[0m):"
            )
            self.info("")
            for platform in missing_platforms:
                original, suggestion = next(
                    x for x in total_removed if x[1] == platform
                )
                self.info(f"\033[93m{original}\033[0m -> \033[34m{suggestion}\033[0m")

    def transform_old_platform(self, old_platform: str):
        new_platform = old_platform
        if self.replace is not None:
            for r in self.replace:
                split = r.split("=", 1)
                if len(split) == 1:
                    split.append("")
                new_platform = new_platform.replace(split[0], split[1])

        return new_platform

    def get_diff_lines(
        self, tgdiff: str
    ) -> tuple[set[str], set[tuple[str, str]], set[str], set[str]]:
        total_added: set[str] = set()
        total_removed: set[tuple[str, str]] = set()
        for line in tgdiff.split("\n"):
            if line.startswith("+++") or line.startswith("---"):
                continue
            if line.startswith("+"):
                total_added.add(line.strip("+"))
            elif line.startswith("-"):
                total_removed.add(
                    (line.strip("-"), self.transform_old_platform(line.strip("-")))
                )

        total_removed_suggestion = set([r[1] for r in total_removed])
        new_platforms = total_added - total_removed_suggestion
        missing_platforms = total_removed_suggestion - total_added
        return ((total_added), (total_removed), (new_platforms), (missing_platforms))

    def get_tgdiff(self) -> str:
        url = f"https://firefoxci.taskcluster-artifacts.net/{self.task_id}/0/public/taskgraph/diffs/diff_mc-onpush.txt"
        self.info(f"Fetching diff from {url}")
        response = requests.get(url, headers={"User-agent": USER_AGENT})
        return response.text
