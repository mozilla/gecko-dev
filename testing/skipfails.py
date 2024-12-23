# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import gzip
import io
import json
import logging
import os
import os.path
import pprint
import re
import sys
import tempfile
import urllib.parse
from copy import deepcopy
from pathlib import Path
from statistics import median
from typing import Any, Dict, Literal
from xmlrpc.client import Fault

from failedplatform import FailedPlatform
from yaml import load

try:
    from yaml import CLoader as Loader
except ImportError:
    from yaml import Loader

import bugzilla
import mozci.push
import requests
from manifestparser import ManifestParser
from manifestparser.toml import add_skip_if, alphabetize_toml_str, sort_paths
from mozci.task import Optional, TestTask
from mozci.util.taskcluster import get_task

from taskcluster.exceptions import TaskclusterRestFailure

TASK_LOG = "live_backing.log"
TASK_ARTIFACT = "public/logs/" + TASK_LOG
ATTACHMENT_DESCRIPTION = "Compressed " + TASK_ARTIFACT + " for task "
ATTACHMENT_REGEX = (
    r".*Created attachment ([0-9]+)\n.*"
    + ATTACHMENT_DESCRIPTION
    + "([A-Za-z0-9_-]+)\n.*"
)

BUGZILLA_AUTHENTICATION_HELP = "Must create a Bugzilla API key per https://github.com/mozilla/mozci-tools/blob/main/citools/test_triage_bug_filer.py"

MS_PER_MINUTE = 60 * 1000  # ms per minute
DEBUG_THRESHOLD = 40 * MS_PER_MINUTE  # 40 minutes in ms
OPT_THRESHOLD = 20 * MS_PER_MINUTE  # 20 minutes in ms

ANYJS = "anyjs"
CC = "classification"
DEF = "DEFAULT"
DIFFERENCE = "difference"
DURATIONS = "durations"
EQEQ = "=="
ERROR = "error"
FAIL = "FAIL"
FAILED_RUNS = "runs_failed"
FAILURE_RATIO = 0.4  # more than this fraction of failures will disable
INTERMITTENT_RATIO_REFTEST = 0.4  # reftest low frequency intermittent
FAILURE_RATIO_REFTEST = 0.8  # disable ratio for reftest (high freq intermittent)
GROUP = "group"
KIND = "kind"
LINENO = "lineno"
LL = "label"
MEDIAN_DURATION = "duration_median"
MINIMUM_RUNS = 3  # mininum number of runs to consider success/failure
MOCK_BUG_DEFAULTS = {"blocks": [], "comments": []}
MOCK_TASK_DEFAULTS = {"extra": {}, "failure_types": {}, "results": []}
MOCK_TASK_INITS = ["results"]
MODIFIERS = "modifiers"
NOTEQ = "!="
OPT = "opt"
PASS = "PASS"
PIXELS = "pixels"
PP = "path"
QUERY = "query"
RR = "result"
RUNS = "runs"
STATUS = "status"
SUBTEST = "subtest"
SUBTEST_REGEX = (
    r"image comparison, max difference: ([0-9]+), number of differing pixels: ([0-9]+)"
)
SUM_BY_LABEL = "sum_by_label"
TEST = "test"
TEST_TYPES = [EQEQ, NOTEQ]
TOTAL_DURATION = "duration_total"
TOTAL_RUNS = "runs_total"
WP = "testing/web-platform/"
WPT0 = WP + "tests/infrastructure"
WPT_META0 = WP + "tests/infrastructure/metadata"
WPT_META0_CLASSIC = WP + "meta/infrastructure"
WPT1 = WP + "tests"
WPT_META1 = WPT1.replace("tests", "meta")
WPT2 = WP + "mozilla/tests"
WPT_META2 = WPT2.replace("tests", "meta")
WPT_MOZILLA = "/_mozilla"


class Mock(object):
    def __init__(self, data, defaults={}, inits=[]):
        self._data = data
        self._defaults = defaults
        for name in inits:
            values = self._data.get(name, [])  # assume type is an array
            values = [Mock(value, defaults, inits) for value in values]
            self._data[name] = values

    def __getattr__(self, name):
        if name in self._data:
            return self._data[name]
        if name in self._defaults:
            return self._defaults[name]
        return ""


class Classification(object):
    "Classification of the failure (not the task result)"

    DISABLE_INTERMITTENT = "disable_intermittent"  # reftest [40%, 80%)
    DISABLE_FAILURE = "disable_failure"  # reftest (80%,100%] failure
    DISABLE_MANIFEST = "disable_manifest"  # crash found
    DISABLE_RECOMMENDED = "disable_recommended"  # disable first failing path
    DISABLE_TOO_LONG = "disable_too_long"  # runtime threshold exceeded
    INTERMITTENT = "intermittent"
    SECONDARY = "secondary"  # secondary failing path
    SUCCESS = "success"  # path always succeeds
    UNKNOWN = "unknown"


class Kind(object):
    "Kind of manifest"

    LIST = "list"
    TOML = "toml"
    UNKNOWN = "unknown"
    WPT = "wpt"


class Skipfails(object):
    "mach manifest skip-fails implementation: Update manifests to skip failing tests"

    REPO = "repo"
    REVISION = "revision"
    TREEHERDER = "treeherder.mozilla.org"
    BUGZILLA_SERVER_DEFAULT = "bugzilla.allizom.org"

    def __init__(
        self,
        command_context=None,
        try_url="",
        verbose=True,
        bugzilla=None,
        dry_run=False,
        turbo=False,
        implicit_vars=False,
        new_version=None,
    ):
        self.command_context = command_context
        if self.command_context is not None:
            self.topsrcdir = self.command_context.topsrcdir
        else:
            self.topsrcdir = Path(__file__).parent.parent
        self.topsrcdir = os.path.normpath(self.topsrcdir)
        if isinstance(try_url, list) and len(try_url) == 1:
            self.try_url = try_url[0]
        else:
            self.try_url = try_url
        self.dry_run = dry_run
        self.implicit_vars = implicit_vars
        self.new_version = new_version
        self.verbose = verbose
        self.turbo = turbo
        if bugzilla is not None:
            self.bugzilla = bugzilla
        elif "BUGZILLA" in os.environ:
            self.bugzilla = os.environ["BUGZILLA"]
        else:
            self.bugzilla = Skipfails.BUGZILLA_SERVER_DEFAULT
        if self.bugzilla == "disable":
            self.bugzilla = None  # Bug filing disabled
        self.component = "skip-fails"
        self._bzapi = None
        self._attach_rx = None
        self.variants = {}
        self.tasks = {}
        self.pp = None
        self.headers = {}  # for Treeherder requests
        self.headers["Accept"] = "application/json"
        self.headers["User-Agent"] = "treeherder-pyclient"
        self.jobs_url = "https://treeherder.mozilla.org/api/jobs/"
        self.push_ids = {}
        self.job_ids = {}
        self.extras = {}
        self.bugs = []  # preloaded bugs, currently not an updated cache
        self.error_summary = {}
        self._subtest_rx = None
        self.lmp = None
        self.failure_types = None
        self.failed_platforms: Dict[str, FailedPlatform] = {}
        self.platform_permutations: Dict[
            str,  # Manifest
            Dict[
                str,  # OS
                Dict[
                    str,  # OS Version
                    Dict[
                        str,  # Processor
                        # Test variants for each build type
                        Dict[str, list[str]],
                    ],
                ],
            ],
        ] = {}

    def _initialize_bzapi(self):
        """Lazily initializes the Bugzilla API (returns True on success)"""
        if self._bzapi is None and self.bugzilla is not None:
            self._bzapi = bugzilla.Bugzilla(self.bugzilla)
            self._attach_rx = re.compile(ATTACHMENT_REGEX, flags=re.M)
        return self._bzapi is not None

    def pprint(self, obj):
        if self.pp is None:
            self.pp = pprint.PrettyPrinter(indent=4, stream=sys.stderr)
        self.pp.pprint(obj)
        sys.stderr.flush()

    def error(self, e):
        if self.command_context is not None:
            self.command_context.log(
                logging.ERROR, self.component, {ERROR: str(e)}, "ERROR: {error}"
            )
        else:
            print(f"ERROR: {e}", file=sys.stderr, flush=True)

    def warning(self, e):
        if self.command_context is not None:
            self.command_context.log(
                logging.WARNING, self.component, {ERROR: str(e)}, "WARNING: {error}"
            )
        else:
            print(f"WARNING: {e}", file=sys.stderr, flush=True)

    def info(self, e):
        if self.command_context is not None:
            self.command_context.log(
                logging.INFO, self.component, {ERROR: str(e)}, "INFO: {error}"
            )
        else:
            print(f"INFO: {e}", file=sys.stderr, flush=True)

    def vinfo(self, e):
        if self.verbose:
            self.info(e)

    def full_path(self, filename):
        """Returns full path for the relative filename"""

        return os.path.join(self.topsrcdir, os.path.normpath(filename.split(":")[-1]))

    def isdir(self, filename):
        """Returns True if filename is a directory"""

        return os.path.isdir(self.full_path(filename))

    def exists(self, filename):
        """Returns True if filename exists"""

        return os.path.exists(self.full_path(filename))

    def run(
        self,
        meta_bug_id=None,
        save_tasks=None,
        use_tasks=None,
        save_failures=None,
        use_failures=None,
        max_failures=-1,
    ):
        "Run skip-fails on try_url, return True on success"

        try_url = self.try_url
        revision, repo = self.get_revision(try_url)
        if use_tasks is not None:
            tasks = self.read_tasks(use_tasks)
            self.vinfo(f"use tasks: {use_tasks}")
            self.failure_types = None  # do NOT cache failure_types
        else:
            tasks = self.get_tasks(revision, repo)
            self.failure_types = {}  # cache failure_types
        if use_failures is not None:
            failures = self.read_failures(use_failures)
            self.vinfo(f"use failures: {use_failures}")
        else:
            failures = self.get_failures(tasks)
            if save_failures is not None:
                self.write_json(save_failures, failures)
                self.vinfo(f"save failures: {save_failures}")
        if save_tasks is not None:
            self.write_tasks(save_tasks, tasks)
            self.vinfo(f"save tasks: {save_tasks}")
        num_failures = 0
        self.vinfo(
            f"skip-fails assumes implicit-vars for reftest: {self.implicit_vars}"
        )
        for manifest in failures:
            kind = failures[manifest][KIND]
            for label in failures[manifest][LL]:
                for path in failures[manifest][LL][label][PP]:
                    classification = failures[manifest][LL][label][PP][path][CC]
                    if classification.startswith("disable_") or (
                        self.turbo and classification == Classification.SECONDARY
                    ):
                        anyjs = {}  # anyjs alternate basename = False
                        differences = []
                        pixels = []
                        status = FAIL
                        lineno = failures[manifest][LL][label][PP][path].get(LINENO, 0)
                        runs: Dict[str, Dict[str, Any]] = failures[manifest][LL][label][
                            PP
                        ][path][RUNS]
                        # skip_failure only needs to run against one task for each path
                        first_task_id = next(iter(runs))
                        for task_id in runs:
                            if kind == Kind.TOML:
                                break
                            elif kind == Kind.LIST:
                                difference = runs[task_id].get(DIFFERENCE, 0)
                                if difference > 0:
                                    differences.append(difference)
                                pixel = runs[task_id].get(PIXELS, 0)
                                if pixel > 0:
                                    pixels.append(pixel)
                                status = runs[task_id].get(STATUS, FAIL)
                            elif kind == Kind.WPT:
                                filename = os.path.basename(path)
                                anyjs[filename] = False
                                if QUERY in runs[task_id]:
                                    query = runs[task_id][QUERY]
                                    anyjs[filename + query] = False
                                else:
                                    query = None
                                if ANYJS in runs[task_id]:
                                    any_filename = os.path.basename(
                                        runs[task_id][ANYJS]
                                    )
                                    anyjs[any_filename] = False
                                    if query is not None:
                                        anyjs[any_filename + query] = False

                        self.skip_failure(
                            manifest,
                            kind,
                            path,
                            anyjs,
                            differences,
                            pixels,
                            lineno,
                            status,
                            label,
                            classification,
                            first_task_id,
                            try_url,
                            revision,
                            repo,
                            meta_bug_id,
                        )
                        num_failures += 1
                        if max_failures >= 0 and num_failures >= max_failures:
                            self.warning(
                                f"max_failures={max_failures} threshold reached: stopping."
                            )
                            return True
        return True

    def get_revision(self, url):
        parsed = urllib.parse.urlparse(url)
        if parsed.scheme != "https":
            raise ValueError("try_url scheme not https")
        if parsed.netloc != Skipfails.TREEHERDER:
            raise ValueError(f"try_url server not {Skipfails.TREEHERDER}")
        if len(parsed.query) == 0:
            raise ValueError("try_url query missing")
        query = urllib.parse.parse_qs(parsed.query)
        if Skipfails.REVISION not in query:
            raise ValueError("try_url query missing revision")
        revision = query[Skipfails.REVISION][0]
        if Skipfails.REPO in query:
            repo = query[Skipfails.REPO][0]
        else:
            repo = "try"
        self.vinfo(f"considering {repo} revision={revision}")
        return revision, repo

    def get_tasks(self, revision, repo):
        push = mozci.push.Push(revision, repo)
        return push.tasks

    def get_failures(self, tasks: list[TestTask]):
        """
        find failures and create structure comprised of runs by path:
           result:
            * False (failed)
            * True (passed)
           classification: Classification
            * unknown (default) < 3 runs
            * intermittent (not enough failures)
            * disable_recommended (enough repeated failures) >3 runs >= 4
            * disable_manifest (disable DEFAULT if no other failures)
            * secondary (not first failure in group)
            * success
        """

        ff = {}
        manifest_paths = {}
        manifest_ = {
            KIND: Kind.UNKNOWN,
            LL: {},
        }
        label_ = {
            DURATIONS: {},
            MEDIAN_DURATION: 0,
            OPT: False,
            PP: {},
            SUM_BY_LABEL: {},  # All sums implicitly zero
            TOTAL_DURATION: 0,
        }
        path_ = {
            CC: Classification.UNKNOWN,
            FAILED_RUNS: 0,
            RUNS: {},
            TOTAL_RUNS: 0,
        }
        run_ = {
            RR: False,
        }

        for task in tasks:  # add explicit failures
            # Task failed but it was not unexpected, skip this
            if task.result == "passed":
                continue

            # strip chunk number - this finds failures across different chunks
            try:
                parts = task.label.split("-")
                int(parts[-1])
                config = "-".join(parts[:-1])
            except ValueError:
                config = task.label

            try:
                if len(task.results) == 0:
                    continue  # ignore aborted tasks
                failure_types = task.failure_types  # call magic property once
                if self.failure_types is None:
                    self.failure_types = {}
                self.failure_types[task.id] = failure_types
                self.vinfo(f"Getting failure_types from task: {task.id}")
                for manifest in failure_types:
                    mm = manifest
                    ll = task.label
                    kind = Kind.UNKNOWN
                    if mm.endswith(".ini"):
                        self.warning(
                            f"cannot analyze skip-fails on INI manifests: {mm}"
                        )
                        continue
                    elif mm.endswith(".list"):
                        kind = Kind.LIST
                    elif mm.endswith(".toml"):
                        kind = Kind.TOML
                    else:
                        kind = Kind.WPT
                        path, mm, _query, _anyjs = self.wpt_paths(mm)
                        if path is None:  # not WPT
                            self.warning(
                                f"cannot analyze skip-fails on unknown manifest type: {manifest}"
                            )
                            continue
                    if kind != Kind.WPT:
                        if mm not in ff:
                            ff[mm] = deepcopy(manifest_)
                            ff[mm][KIND] = kind
                        if ll not in ff[mm][LL]:
                            ff[mm][LL][ll] = deepcopy(label_)

                    if mm not in manifest_paths:
                        manifest_paths[mm] = {}
                    if config not in manifest_paths[mm]:
                        manifest_paths[mm][config] = []

                    for path_type in failure_types[manifest]:
                        path, _type = path_type
                        query = None
                        anyjs = None
                        allpaths = []
                        if kind == Kind.WPT:
                            path, mmpath, query, anyjs = self.wpt_paths(path)
                            if path is None:
                                self.warning(
                                    f"non existant failure path: {path_type[0]}"
                                )
                                break
                            allpaths = [path]
                            mm = os.path.dirname(mmpath)
                            if mm not in manifest_paths:
                                manifest_paths[mm] = []
                            if mm not in ff:
                                ff[mm] = deepcopy(manifest_)
                                ff[mm][KIND] = kind
                            if ll not in ff[mm][LL]:
                                ff[mm][LL][ll] = deepcopy(label_)
                        elif kind == Kind.LIST:
                            words = path.split()
                            if len(words) != 3 or words[1] not in TEST_TYPES:
                                self.warning(f"reftest type not supported: {path}")
                                continue
                            allpaths = self.get_allpaths(task.id, mm, path)
                        elif kind == Kind.TOML:
                            if path == mm:
                                path = DEF  # refers to the manifest itself
                            allpaths = [path]
                        for path in allpaths:
                            if path not in manifest_paths[mm].get(config, []):
                                manifest_paths[mm][config].append(path)
                            self.vinfo(
                                f"Getting failure info in manifest: {mm}, path: {path}"
                            )
                            if path not in ff[mm][LL][ll][PP]:
                                ff[mm][LL][ll][PP][path] = deepcopy(path_)
                            if task.id not in ff[mm][LL][ll][PP][path][RUNS]:
                                ff[mm][LL][ll][PP][path][RUNS][task.id] = deepcopy(run_)
                            else:
                                continue
                            ff[mm][LL][ll][PP][path][RUNS][task.id][RR] = False
                            if query is not None:
                                ff[mm][LL][ll][PP][path][RUNS][task.id][QUERY] = query
                            if anyjs is not None:
                                ff[mm][LL][ll][PP][path][RUNS][task.id][ANYJS] = anyjs
                            ff[mm][LL][ll][PP][path][TOTAL_RUNS] += 1
                            ff[mm][LL][ll][PP][path][FAILED_RUNS] += 1
                            if kind == Kind.LIST:
                                (
                                    lineno,
                                    difference,
                                    pixels,
                                    status,
                                ) = self.get_lineno_difference_pixels_status(
                                    task.id, mm, path
                                )
                                if lineno > 0:
                                    ff[mm][LL][ll][PP][path][LINENO] = lineno
                                else:
                                    self.vinfo(f"ERROR no lineno for {path}")
                                if status != FAIL:
                                    ff[mm][LL][ll][PP][path][RUNS][task.id][
                                        STATUS
                                    ] = status
                                if status == FAIL and difference == 0 and pixels == 0:
                                    # intermittent, not error
                                    ff[mm][LL][ll][PP][path][RUNS][task.id][RR] = True
                                    ff[mm][LL][ll][PP][path][FAILED_RUNS] -= 1
                                elif difference > 0:
                                    ff[mm][LL][ll][PP][path][RUNS][task.id][
                                        DIFFERENCE
                                    ] = difference
                                if pixels > 0:
                                    ff[mm][LL][ll][PP][path][RUNS][task.id][
                                        PIXELS
                                    ] = pixels
            except AttributeError:
                pass  # self.warning(f"unknown attribute in task (#1): {ae}")

        for task in tasks:  # add results
            # Task failed but it was not unexpected, skip this
            if task.result == "passed":
                continue
            try:
                parts = task.label.split("-")
                int(parts[-1])
                config = "-".join(parts[:-1])
            except ValueError:
                config = task.label

            try:
                if len(task.results) == 0:
                    continue  # ignore aborted tasks
                self.vinfo(f"Getting results from task: {task.id}")
                for result in task.results:
                    mm = result.group
                    ll = task.label
                    kind = Kind.UNKNOWN
                    if mm.endswith(".ini"):
                        self.warning(
                            f"cannot analyze skip-fails on INI manifests: {mm}"
                        )
                        continue
                    elif mm.endswith(".list"):
                        kind = Kind.LIST
                    elif mm.endswith(".toml"):
                        kind = Kind.TOML
                    else:
                        kind = Kind.WPT
                        path, mm, _query, _anyjs = self.wpt_paths(mm)
                        if path is None:  # not WPT
                            self.warning(
                                f"cannot analyze skip-fails on unknown manifest type: {result.group}"
                            )
                            continue
                    if mm not in manifest_paths:
                        continue
                    if config not in manifest_paths[mm]:
                        continue
                    if mm not in ff:
                        ff[mm] = deepcopy(manifest_)
                    if ll not in ff[mm][LL]:
                        ff[mm][LL][ll] = deepcopy(label_)
                    if task.id not in ff[mm][LL][ll][DURATIONS]:
                        # duration may be None !!!
                        ff[mm][LL][ll][DURATIONS][task.id] = result.duration or 0
                        if ff[mm][LL][ll][OPT] is None:
                            ff[mm][LL][ll][OPT] = self.get_opt_for_task(task.id)
                    for path in manifest_paths[mm][config]:  # all known paths
                        # path can be one of any paths that have failed for the manifest/config
                        # ensure the path is in the specific task failure data
                        if path not in [
                            path
                            for path, type in self.failure_types.get(task.id, {}).get(
                                mm, [("", "")]
                            )
                        ]:
                            result.ok = True

                        if path not in ff[mm][LL][ll][PP]:
                            ff[mm][LL][ll][PP][path] = deepcopy(path_)
                        if task.id not in ff[mm][LL][ll][PP][path][RUNS]:
                            ff[mm][LL][ll][PP][path][RUNS][task.id] = deepcopy(run_)
                            ff[mm][LL][ll][PP][path][RUNS][task.id][RR] = result.ok
                            ff[mm][LL][ll][PP][path][TOTAL_RUNS] += 1
                            if not result.ok:
                                ff[mm][LL][ll][PP][path][FAILED_RUNS] += 1
                            if kind == Kind.LIST:
                                (
                                    lineno,
                                    difference,
                                    pixels,
                                    status,
                                ) = self.get_lineno_difference_pixels_status(
                                    task.id, mm, path
                                )
                                if lineno > 0:
                                    ff[mm][LL][ll][PP][path][LINENO] = lineno
                                else:
                                    self.vinfo(f"ERROR no lineno for {path}")
                                if status != FAIL:
                                    ff[mm][LL][ll][PP][path][RUNS][task.id][
                                        STATUS
                                    ] = status
                                if (
                                    status == FAIL
                                    and difference == 0
                                    and pixels == 0
                                    and not result.ok
                                ):
                                    # intermittent, not error
                                    ff[mm][LL][ll][PP][path][RUNS][task.id][RR] = True
                                    ff[mm][LL][ll][PP][path][FAILED_RUNS] -= 1
                                if difference > 0:
                                    ff[mm][LL][ll][PP][path][RUNS][task.id][
                                        DIFFERENCE
                                    ] = difference
                                if pixels > 0:
                                    ff[mm][LL][ll][PP][path][RUNS][task.id][
                                        PIXELS
                                    ] = pixels
            except AttributeError:
                pass  # self.warning(f"unknown attribute in task (#2): {ae}")

        for mm in ff:  # determine classifications
            kind = ff[mm][KIND]
            for label in ff[mm][LL]:
                ll = label
                opt = ff[mm][LL][ll][OPT]
                durations = []  # summarize durations
                for task_id in ff[mm][LL][ll][DURATIONS]:
                    duration = ff[mm][LL][ll][DURATIONS][task_id]
                    durations.append(duration)
                if len(durations) > 0:
                    total_duration = sum(durations)
                    median_duration = median(durations)
                    ff[mm][LL][ll][TOTAL_DURATION] = total_duration
                    ff[mm][LL][ll][MEDIAN_DURATION] = median_duration
                    if (opt and median_duration > OPT_THRESHOLD) or (
                        (not opt) and median_duration > DEBUG_THRESHOLD
                    ):
                        if kind == Kind.TOML:
                            paths = [DEF]
                        else:
                            paths = ff[mm][LL][ll][PP].keys()
                        for path in paths:
                            if path not in ff[mm][LL][ll][PP]:
                                ff[mm][LL][ll][PP][path] = deepcopy(path_)
                            if task_id not in ff[mm][LL][ll][PP][path][RUNS]:
                                ff[mm][LL][ll][PP][path][RUNS][task.id] = deepcopy(run_)
                                ff[mm][LL][ll][PP][path][RUNS][task.id][RR] = False
                                ff[mm][LL][ll][PP][path][TOTAL_RUNS] += 1
                                ff[mm][LL][ll][PP][path][FAILED_RUNS] += 1
                            ff[mm][LL][ll][PP][path][
                                CC
                            ] = Classification.DISABLE_TOO_LONG
                primary = True  # we have not seen the first failure
                for path in sort_paths(ff[mm][LL][ll][PP]):
                    classification = ff[mm][LL][ll][PP][path][CC]
                    if classification == Classification.UNKNOWN:
                        failed_runs = ff[mm][LL][ll][PP][path][FAILED_RUNS]
                        total_runs = ff[mm][LL][ll][PP][path][TOTAL_RUNS]
                        status = FAIL  # default status, only one run could be PASS
                        for task_id in ff[mm][LL][ll][PP][path][RUNS]:
                            status = ff[mm][LL][ll][PP][path][RUNS][task_id].get(
                                STATUS, status
                            )
                        if kind == Kind.LIST:
                            failure_ratio = INTERMITTENT_RATIO_REFTEST
                        else:
                            failure_ratio = FAILURE_RATIO
                        if total_runs >= MINIMUM_RUNS:
                            if failed_runs / total_runs < failure_ratio:
                                if failed_runs == 0:
                                    classification = Classification.SUCCESS
                                else:
                                    classification = Classification.INTERMITTENT
                            elif kind == Kind.LIST:
                                if failed_runs / total_runs < FAILURE_RATIO_REFTEST:
                                    classification = Classification.DISABLE_INTERMITTENT
                                else:
                                    classification = Classification.DISABLE_FAILURE
                            elif primary:
                                if path == DEF:
                                    classification = Classification.DISABLE_MANIFEST
                                else:
                                    classification = Classification.DISABLE_RECOMMENDED
                                primary = False
                            else:
                                classification = Classification.SECONDARY
                        ff[mm][LL][ll][PP][path][CC] = classification
                    if classification not in ff[mm][LL][ll][SUM_BY_LABEL]:
                        ff[mm][LL][ll][SUM_BY_LABEL][classification] = 0
                    ff[mm][LL][ll][SUM_BY_LABEL][classification] += 1
        return ff

    def _get_os_version(self, os, platform):
        """Return the os_version given the label platform string"""
        i = platform.find(os)
        j = i + len(os)
        yy = platform[j : j + 2]
        mm = platform[j + 2 : j + 4]
        return yy + "." + mm

    def get_bug_by_id(self, id):
        """Get bug by bug id"""

        bug = None
        for b in self.bugs:
            if b.id == id:
                bug = b
                break
        if bug is None and self._initialize_bzapi():
            bug = self._bzapi.getbug(id)
        return bug

    def get_bugs_by_summary(self, summary):
        """Get bug by bug summary"""

        bugs = []
        for b in self.bugs:
            if b.summary == summary:
                bugs.append(b)
        if len(bugs) > 0:
            return bugs
        if self._initialize_bzapi():
            query = self._bzapi.build_query(short_desc=summary)
            query["include_fields"] = [
                "id",
                "product",
                "component",
                "status",
                "resolution",
                "summary",
                "blocks",
            ]
            bugs = self._bzapi.query(query)
        return bugs

    def create_bug(
        self,
        summary="Bug short description",
        description="Bug description",
        product="Testing",
        component="General",
        version="unspecified",
        bugtype="task",
    ):
        """Create a bug"""

        bug = None
        if self._initialize_bzapi():
            if not self._bzapi.logged_in:
                self.error(
                    "Must create a Bugzilla API key per https://github.com/mozilla/mozci-tools/blob/main/citools/test_triage_bug_filer.py"
                )
                raise PermissionError(f"Not authenticated for Bugzilla {self.bugzilla}")
            createinfo = self._bzapi.build_createbug(
                product=product,
                component=component,
                summary=summary,
                version=version,
                description=description,
            )
            createinfo["type"] = bugtype
            bug = self._bzapi.createbug(createinfo)
        return bug

    def add_bug_comment(self, id, comment, meta_bug_id=None):
        """Add a comment to an existing bug"""

        if self._initialize_bzapi():
            if not self._bzapi.logged_in:
                self.error(BUGZILLA_AUTHENTICATION_HELP)
                raise PermissionError("Not authenticated for Bugzilla")
            if meta_bug_id is not None:
                blocks_add = [meta_bug_id]
            else:
                blocks_add = None
            updateinfo = self._bzapi.build_update(
                comment=comment, blocks_add=blocks_add
            )
            self._bzapi.update_bugs([id], updateinfo)

    def generate_bugzilla_comment(
        self,
        manifest: str,
        kind: str,
        path: str,
        anyjs: Optional[Dict[str, bool]],
        lineno: int,
        label: str,
        classification: str,
        task_id: Optional[str],
        try_url: str,
        revision: str,
        repo: str,
        skip_if: str,
        filename: str,
        meta_bug_id: Optional[str] = None,
    ):
        bug_reference = ""
        if classification == Classification.DISABLE_MANIFEST:
            comment = "Disabled entire manifest due to crash result"
        elif classification == Classification.DISABLE_TOO_LONG:
            comment = "Disabled entire manifest due to excessive run time"
        else:
            comment = f'Disabled test due to failures in test file: "{filename}"'
            if classification == Classification.SECONDARY:
                comment += " (secondary)"
                if kind != Kind.WPT:
                    bug_reference = " (secondary)"
        if kind != Kind.LIST:
            self.vinfo(f"filename: {filename}")
        if kind == Kind.WPT and anyjs is not None and len(anyjs) > 1:
            comment += "\nAdditional WPT wildcard paths:"
            for p in sorted(anyjs.keys()):
                if p != filename:
                    comment += f'\n  "{p}"'
        platform, testname = self.label_to_platform_testname(label)
        if platform is not None:
            comment += "\nCommand line to reproduce (experimental):\n"
            comment += f"  \"mach try fuzzy -q '{platform}' {testname}\""
        comment += f"\nTry URL = {try_url}"
        comment += f"\nrevision = {revision}"
        comment += f"\nrepo = {repo}"
        comment += f"\nlabel = {label}"
        if task_id is not None:
            comment += f"\ntask_id = {task_id}"
            if kind != Kind.LIST:
                push_id = self.get_push_id(revision, repo)
                if push_id is not None:
                    comment += f"\npush_id = {push_id}"
                    job_id = self.get_job_id(push_id, task_id)
                    if job_id is not None:
                        comment += f"\njob_id = {job_id}"
                        (
                            suggestions_url,
                            line_number,
                            line,
                            log_url,
                        ) = self.get_bug_suggestions(repo, job_id, path, anyjs)
                        if log_url is not None:
                            comment += f"\nBug suggestions: {suggestions_url}"
                            comment += f"\nSpecifically see at line {line_number} in the attached log: {log_url}"
                            comment += f'\n\n  "{line}"\n'
        bug_summary = f"MANIFEST {manifest}"
        attachments = {}
        bugid = "TBD"
        if self.bugzilla is None:
            self.vinfo("Bugzilla has been disabled: no bugs created or updated")
        else:
            bugs = self.get_bugs_by_summary(bug_summary)
            if len(bugs) == 0:
                description = (
                    f"This bug covers excluded failing tests in the MANIFEST {manifest}"
                )
                description += "\n(generated by `mach manifest skip-fails`)"
                product, component = self.get_file_info(path)
                if self.dry_run:
                    self.warning(
                        f'Dry-run NOT creating bug: {product}::{component} "{bug_summary}"'
                    )
                else:
                    bug = self.create_bug(bug_summary, description, product, component)
                    if bug is not None:
                        bugid = bug.id
                        self.vinfo(
                            f'Created Bug {bugid} {product}::{component} : "{bug_summary}"'
                        )
            elif len(bugs) == 1:
                bugid = bugs[0].id
                product = bugs[0].product
                component = bugs[0].component
                self.vinfo(f'Found Bug {bugid} {product}::{component} "{bug_summary}"')
                if meta_bug_id is not None:
                    if meta_bug_id in bugs[0].blocks:
                        self.vinfo(
                            f"  Bug {bugid} already blocks meta bug {meta_bug_id}"
                        )
                        meta_bug_id = None  # no need to add again
                comments = bugs[0].getcomments()
                for i in range(len(comments)):
                    text = comments[i]["text"]
                    attach_rx = self._attach_rx
                    if attach_rx is not None:
                        m = attach_rx.findall(text)
                        if len(m) == 1:
                            a_task_id = m[0][1]
                            attachments[a_task_id] = m[0][0]
                            if a_task_id == task_id:
                                self.vinfo(
                                    f"  Bug {bugid} already has the compressed log attached for this task"
                                )
            else:
                raise Exception(f'More than one bug found for summary: "{bug_summary}"')
        bug_reference = f"Bug {bugid}" + bug_reference
        extra = self.get_extra(task_id)
        json.dumps(extra)
        if kind == Kind.LIST:
            comment += (
                f"\nfuzzy-if condition on line {lineno}: {skip_if} # {bug_reference}"
            )
        else:
            comment += f"\nskip-if condition: {skip_if} # {bug_reference}"
        return (comment, bug_reference, bugid, attachments)

    def resolve_failure_filename(self, path: str, kind: str, manifest: str) -> str:
        filename = DEF
        if kind == Kind.TOML:
            filename = self.get_filename_in_manifest(manifest.split(":")[-1], path)
        elif kind == Kind.WPT:
            filename = os.path.basename(path)
        elif kind == Kind.LIST:
            filename = path
        return filename

    def resolve_failure_manifest(self, path: str, kind: str, manifest: str) -> str:
        if kind == Kind.WPT:
            _path, resolved_manifest, _query, _anyjs = self.wpt_paths(path)
            if resolved_manifest:
                return resolved_manifest
            raise Exception(f"Could not resolve WPT manifest for path {path}")
        return manifest

    def skip_failure(
        self,
        manifest: str,
        kind: str,
        path: str,
        anyjs: Optional[Dict[str, bool]],
        differences: list[int],
        pixels: list[int],
        lineno: int,
        status: str,
        label: str,
        classification: str,
        task_id: Optional[str],
        try_url: str,
        revision: str,
        repo: str,
        meta_bug_id: Optional[str] = None,
    ):
        """
        Skip a failure (for TOML, WPT and REFTEST manifests)
        For wpt anyjs is a dictionary mapping from alternate basename to
        a boolean (indicating if the basename has been handled in the manifest)
        """

        self.vinfo(f"\n\n===== Skip failure in manifest: {manifest} =====")
        self.vinfo(f"    path: {path}")
        skip_if: Optional[str]
        if task_id is None:
            skip_if = "true"
        else:
            skip_if = self.task_to_skip_if(manifest, task_id, kind, path)
        if skip_if is None:
            raise Exception(
                f"Unable to calculate skip-if condition from manifest={manifest} from failure label={label}"
            )

        filename = self.resolve_failure_filename(path, kind, manifest)
        manifest = self.resolve_failure_manifest(path, kind, manifest)
        manifest_path = self.full_path(manifest)
        manifest_str = ""
        additional_comment = ""
        comment, bug_reference, bugid, attachments = self.generate_bugzilla_comment(
            manifest,
            kind,
            path,
            anyjs,
            lineno,
            label,
            classification,
            task_id,
            try_url,
            revision,
            repo,
            skip_if,
            filename,
            meta_bug_id,
        )
        if kind == Kind.WPT:
            if os.path.exists(manifest_path):
                manifest_str = io.open(manifest_path, "r", encoding="utf-8").read()
            else:
                # ensure parent directories exist
                os.makedirs(os.path.dirname(manifest_path), exist_ok=True)
            manifest_str, additional_comment = self.wpt_add_skip_if(
                manifest_str, anyjs, skip_if, bug_reference
            )
        elif kind == Kind.TOML:
            mp = ManifestParser(use_toml=True, document=True)
            try:
                mp.read(manifest_path)
            except IOError:
                raise Exception(f"Unable to find path: {manifest_path}")

            document = mp.source_documents[manifest_path]
            try:
                additional_comment = add_skip_if(
                    document,
                    filename,
                    skip_if,
                    bug_reference,
                )
            except Exception:
                # Note: this fails to find a comment at the desired index
                # Note: manifestparser len(skip_if) yields: TypeError: object of type 'bool' has no len()
                additional_comment = ""

            manifest_str = alphabetize_toml_str(document)
        elif kind == Kind.LIST:
            if lineno == 0:
                self.error(
                    f"cannot determine line to edit in manifest: {manifest_path}"
                )
            elif not os.path.exists(manifest_path):
                self.error(f"manifest does not exist: {manifest_path}")
            else:
                manifest_str = io.open(manifest_path, "r", encoding="utf-8").read()
                if status == PASS:
                    self.info(f"Unexpected status: {status}")
                if (
                    status == PASS
                    or classification == Classification.DISABLE_INTERMITTENT
                ):
                    zero = True  # refest lower ranges should include zero
                else:
                    zero = False
                manifest_str, additional_comment = self.reftest_add_fuzzy_if(
                    manifest_str,
                    filename,
                    skip_if,
                    differences,
                    pixels,
                    lineno,
                    zero,
                    bug_reference,
                )
                if not manifest_str and additional_comment:
                    self.warning(additional_comment)
        if additional_comment:
            comment += "\n" + additional_comment
        if len(manifest_str) > 0:
            fp = io.open(manifest_path, "w", encoding="utf-8", newline="\n")
            fp.write(manifest_str)
            fp.close()
            self.info(f'Edited ["{filename}"] in manifest: "{manifest}"')
            if kind != Kind.LIST:
                self.info(f'added skip-if condition: "{skip_if}" # {bug_reference}')
            if self.dry_run:
                self.info(f"Dry-run NOT adding comment to Bug {bugid}:\n{comment}")
                self.info(
                    f'Dry-run NOT editing ["{filename}"] in manifest: "{manifest}"'
                )
                self.info(f'would add skip-if condition: "{skip_if}" # {bug_reference}')
                if task_id is not None and task_id not in attachments:
                    self.info("would add compressed log for this task")
                return
            elif self.bugzilla is None:
                self.warning(f"NOT adding comment to Bug {bugid}:\n{comment}")
            else:
                self.add_bug_comment(bugid, comment, meta_bug_id)
                self.info(f"Added comment to Bug {bugid}:\n{comment}")
                if meta_bug_id is not None:
                    self.info(f"  Bug {bugid} blocks meta Bug: {meta_bug_id}")
                if task_id is not None and task_id not in attachments:
                    self.add_attachment_log_for_task(bugid, task_id)
                    self.info("Added compressed log for this task")
        else:
            self.error(f'Error editing ["{filename}"] in manifest: "{manifest}"')

    def get_variants(self):
        """Get mozinfo for each test variants"""

        if len(self.variants) == 0:
            variants_file = "taskcluster/kinds/test/variants.yml"
            variants_path = self.full_path(variants_file)
            fp = io.open(variants_path, "r", encoding="utf-8")
            raw_variants = load(fp, Loader=Loader)
            fp.close()
            for k, v in raw_variants.items():
                mozinfo = k
                if "mozinfo" in v:
                    mozinfo = v["mozinfo"]
                self.variants[k] = mozinfo
        return self.variants

    def get_task_details(self, task_id):
        """Download details for task task_id"""

        if task_id in self.tasks:  # if cached
            task = self.tasks[task_id]
        else:
            self.vinfo(f"get_task_details for task: {task_id}")
            try:
                task = get_task(task_id)
            except TaskclusterRestFailure:
                self.warning(f"Task {task_id} no longer exists.")
                return None
            self.tasks[task_id] = task
        return task

    def get_pretty_os_name(self, os: str):
        pretty = os
        if pretty == "windows":
            pretty = "win"
        elif pretty == "macosx":
            pretty = "mac"
        return pretty

    def get_pretty_os_version(self, os_version: str):
        # Ubuntu/macos version numbers
        if len(os_version) == 4:
            return os_version[0:2] + "." + os_version[2:4]
        return os_version

    def get_pretty_arch(self, arch: str):
        if arch == "x86" or arch.find("32") >= 0:
            bits = "32"
            arch = "x86"
        else:
            bits = "64"
            if arch not in ("aarch64", "ppc", "arm7"):
                arch = "x86_64"
        return (arch, bits)

    def get_extra(self, task_id):
        """Calculate extra for task task_id"""

        if task_id in self.extras:  # if cached
            extra = self.extras[task_id]
        else:
            self.get_variants()
            task = self.get_task_details(task_id) or {}
            arch = None
            bits = None
            build = None
            build_types = []
            display = None
            os = None
            os_version = None
            runtimes = []
            test_setting = task.get("extra", {}).get("test-setting", {})
            platform = test_setting.get("platform", {})
            platform_os = platform.get("os", {})
            opt = False
            debug = False
            if "name" in platform_os:
                os = self.get_pretty_os_name(platform_os["name"])
            if "version" in platform_os:
                os_version = self.get_pretty_os_version(
                    self.new_version or platform_os["version"]
                )
            if "build" in platform_os:
                build = platform_os["build"]
            if "arch" in platform:
                arch, bits = self.get_pretty_arch(platform["arch"])
            if "display" in platform:
                display = platform["display"]
            if "runtime" in test_setting:
                for k in test_setting["runtime"]:
                    if k == "no-fission" and test_setting["runtime"][k]:
                        runtimes.append("no-fission")
                    elif k in self.variants:  # draw-snapshot -> snapshot
                        runtimes.append(self.variants[k])  # adds mozinfo
            if "build" in test_setting:
                tbuild = test_setting["build"]
                for k in tbuild:
                    if k == "type":
                        if tbuild[k] == "opt":
                            opt = True
                        elif tbuild[k] == "debug":
                            debug = True
                        build_types.append(tbuild[k])
                    else:
                        build_types.append(k)
            unknown = None
            extra = {
                "arch": arch or unknown,
                "bits": bits or unknown,
                "build": build or unknown,
                "build_types": build_types,
                "debug": debug,
                "display": display or unknown,
                "opt": opt,
                "os": os or unknown,
                "os_version": os_version or unknown,
                "runtimes": runtimes,
            }
        self.extras[task_id] = extra
        return extra

    def get_opt_for_task(self, task_id):
        extra = self.get_extra(task_id)
        return extra["opt"]

    def _fetch_platform_permutations(self):
        self.info("Fetching platform permutations...")
        import taskcluster

        url: Optional[str] = None
        index = taskcluster.Index(
            {
                "rootUrl": "https://firefox-ci-tc.services.mozilla.com",
            }
        )
        route = "gecko.v2.mozilla-central.latest.source.test-info-all"
        queue = taskcluster.Queue(
            {
                "rootUrl": "https://firefox-ci-tc.services.mozilla.com",
            }
        )

        # Typing from findTask is wrong, so we need to convert to Any
        result: Optional[Dict[str, Any]] = index.findTask(route)
        if result is not None:
            task_id: str = result["taskId"]
            result = queue.listLatestArtifacts(task_id)
            if result is not None and task_id is not None:
                artifact_list: list[Dict[Literal["name"], str]] = result["artifacts"]
                for artifact in artifact_list:
                    artifact_name = artifact["name"]
                    if artifact_name.endswith("test-info-testrun-matrix.json"):
                        url = queue.buildUrl(
                            "getLatestArtifact", task_id, artifact_name
                        )
                        break

        if url is not None:
            response = requests.get(url, headers={"User-agent": "mach-test-info/1.0"})
            self.platform_permutations = response.json()
        else:
            self.info("Failed fetching platform permutations...")

    def _get_list_skip_if(self, extra):
        aa = "&&"
        nn = "!"

        os = extra.get("os")
        build_types = extra.get("build_types", [])
        runtimes = extra.get("runtimes", [])

        skip_if = None
        if os == "linux":
            skip_if = "gtkWidget"
        elif os == "win":
            skip_if = "winWidget"
        elif os == "mac":
            skip_if = "cocoaWidget"
        elif os == "android":
            skip_if = "Android"
        else:
            self.error(f"cannot calculate skip-if for unknown OS: '{os}'")
        os = extra.get("os")
        if skip_if is not None:
            debug = "debug" in build_types
            ccov = "ccov" in build_types
            asan = "asan" in build_types
            tsan = "tsan" in build_types
            optimized = (not debug) and (not ccov) and (not asan) and (not tsan)
            skip_if += aa
            if optimized:
                skip_if += "optimized"
            elif debug:
                skip_if += "isDebugBuild"
            elif ccov:
                skip_if += "isCoverageBuild"
            elif asan:
                skip_if += "AddressSanitizer"
            elif tsan:
                skip_if += "ThreadSanitizer"
            # See implicit VARIANT_DEFAULTS in
            # https://searchfox.org/mozilla-central/source/layout/tools/reftest/manifest.sys.mjs#30
            fission = "no-fission" not in runtimes
            snapshot = "snapshot" in runtimes
            swgl = "swgl" in runtimes
            if not self.implicit_vars and fission:
                skip_if += aa + "fission"
            elif not fission:  # implicit default: fission
                skip_if += aa + nn + "fission"
            if extra.get("bits") is not None:
                if extra["bits"] == "32":
                    skip_if += aa + nn + "is64Bit"  # override implicit is64Bit
                elif not self.implicit_vars and os == "winWidget":
                    skip_if += aa + "is64Bit"
            if not self.implicit_vars and not swgl:
                skip_if += aa + nn + "swgl"
            elif swgl:  # implicit default: !swgl
                skip_if += aa + "swgl"
            if os == "gtkWidget":
                if not self.implicit_vars and not snapshot:
                    skip_if += aa + nn + "useDrawSnapshot"
                elif snapshot:  # implicit default: !useDrawSnapshot
                    skip_if += aa + "useDrawSnapshot"
        return skip_if

    def task_to_skip_if(
        self, manifest: str, task_id: str, kind: str, file_path: str
    ) -> str:
        """Calculate the skip-if condition for failing task task_id"""

        if kind == Kind.WPT:
            qq = '"'
            aa = " and "
        else:
            qq = "'"
            aa = " && "
        eq = " == "
        extra = self.get_extra(task_id)
        skip_if = None
        os = extra.get("os")
        os_version = extra.get("os_version")
        if os is not None:
            if kind == Kind.LIST:
                skip_if = self._get_list_skip_if(extra)
            elif (
                os_version is not None
                and extra.get("build") is not None
                and os == "win"
                and os_version == "11"
                and extra["build"] == "2009"
            ):
                skip_if = "win11_2009"  # mozinfo.py:137
                os_version = "11.2009"
            elif os_version is not None:
                skip_if = "os" + eq + qq + os + qq
                skip_if += aa + "os_version" + eq + qq + os_version + qq

        processor = extra.get("arch")
        if skip_if is not None and kind != Kind.LIST:
            if processor is not None:
                skip_if += aa + "processor" + eq + qq + processor + qq

            failure_key = os + os_version + processor + manifest + file_path
            if self.failed_platforms.get(failure_key) is None:
                if not self.platform_permutations:
                    self._fetch_platform_permutations()
                permutations = (
                    self.platform_permutations.get(manifest, {})
                    .get(os, {})
                    .get(os_version, {})
                    .get(processor, None)
                )
                # Pushes to try made with --full may schedule tests not handled here. We ignore those
                if permutations is not None:
                    self.failed_platforms[failure_key] = FailedPlatform(
                        self.platform_permutations[manifest][os][os_version][processor]
                    )

            build_types = extra.get("build_types", [])
            failed_platform = self.failed_platforms.get(failure_key, None)
            if failed_platform is not None:
                test_variants = extra.get("runtimes", [])
                skip_if += failed_platform.get_skip_string(
                    aa, build_types, test_variants
                )
        return skip_if

    def get_file_info(self, path, product="Testing", component="General"):
        """
        Get bugzilla product and component for the path.
        Provide defaults (in case command_context is not defined
        or there isn't file info available).
        """
        if path != DEF and self.command_context is not None:
            reader = self.command_context.mozbuild_reader(config_mode="empty")
            info = reader.files_info([path])
            cp = info[path]["BUG_COMPONENT"]
            product = cp.product
            component = cp.component
        return product, component

    def get_filename_in_manifest(self, manifest: str, path: str) -> str:
        """return relative filename for path in manifest"""

        filename = os.path.basename(path)
        if filename == DEF:
            return filename
        manifest_dir = os.path.dirname(manifest)
        i = 0
        j = min(len(manifest_dir), len(path))
        while i < j and manifest_dir[i] == path[i]:
            i += 1
        if i < len(manifest_dir):
            for _ in range(manifest_dir.count("/", i) + 1):
                filename = "../" + filename
        elif i < len(path):
            filename = path[i + 1 :]
        return filename

    def get_push_id(self, revision, repo):
        """Return the push_id for revision and repo (or None)"""

        self.vinfo(f"Retrieving push_id for {repo} revision: {revision} ...")
        if revision in self.push_ids:  # if cached
            push_id = self.push_ids[revision]
        else:
            push_id = None
            push_url = f"https://treeherder.mozilla.org/api/project/{repo}/push/"
            params = {}
            params["full"] = "true"
            params["count"] = 10
            params["revision"] = revision
            r = requests.get(push_url, headers=self.headers, params=params)
            if r.status_code != 200:
                self.warning(f"FAILED to query Treeherder = {r} for {r.url}")
            else:
                response = r.json()
                if "results" in response:
                    results = response["results"]
                    if len(results) > 0:
                        r0 = results[0]
                        if "id" in r0:
                            push_id = r0["id"]
            self.push_ids[revision] = push_id
        return push_id

    def get_job_id(self, push_id, task_id):
        """Return the job_id for push_id, task_id (or None)"""

        self.vinfo(f"Retrieving job_id for push_id: {push_id}, task_id: {task_id} ...")
        k = f"{push_id}:{task_id}"
        if k in self.job_ids:  # if cached
            job_id = self.job_ids[k]
        else:
            job_id = None
            params = {}
            params["push_id"] = push_id
            r = requests.get(self.jobs_url, headers=self.headers, params=params)
            if r.status_code != 200:
                self.warning(f"FAILED to query Treeherder = {r} for {r.url}")
            else:
                response = r.json()
                if "results" in response:
                    results = response["results"]
                    if len(results) > 0:
                        for result in results:
                            if len(result) > 14:
                                if result[14] == task_id:
                                    job_id = result[1]
                                    break
            self.job_ids[k] = job_id
        return job_id

    def get_bug_suggestions(self, repo, job_id, path, anyjs=None):
        """
        Return the (suggestions_url, line_number, line, log_url)
        for the given repo and job_id
        """
        self.vinfo(
            f"Retrieving bug_suggestions for {repo} job_id: {job_id}, path: {path} ..."
        )
        suggestions_url = f"https://treeherder.mozilla.org/api/project/{repo}/jobs/{job_id}/bug_suggestions/"
        line_number = None
        line = None
        log_url = None
        r = requests.get(suggestions_url, headers=self.headers)
        if r.status_code != 200:
            self.warning(f"FAILED to query Treeherder = {r} for {r.url}")
        else:
            if anyjs is not None:
                pathdir = os.path.dirname(path) + "/"
                paths = [pathdir + f for f in anyjs.keys()]
            else:
                paths = [path]
            response = r.json()
            if len(response) > 0:
                for sugg in response:
                    for p in paths:
                        path_end = sugg.get("path_end", None)
                        # handles WPT short paths
                        if path_end is not None and p.endswith(path_end):
                            line_number = sugg["line_number"] + 1
                            line = sugg["search"]
                            log_url = f"https://treeherder.mozilla.org/logviewer?repo={repo}&job_id={job_id}&lineNumber={line_number}"
                            break
        rv = (suggestions_url, line_number, line, log_url)
        return rv

    def read_json(self, filename):
        """read data as JSON from filename"""

        fp = io.open(filename, "r", encoding="utf-8")
        data = json.load(fp)
        fp.close()
        return data

    def read_tasks(self, filename):
        """read tasks as JSON from filename"""

        if not os.path.exists(filename):
            msg = f"use-tasks JSON file does not exist: {filename}"
            raise OSError(2, msg, filename)
        tasks = self.read_json(filename)
        tasks = [Mock(task, MOCK_TASK_DEFAULTS, MOCK_TASK_INITS) for task in tasks]
        for task in tasks:
            if len(task.extra) > 0:  # pre-warm cache for extra information
                self.extras[task.id] = task.extra
        return tasks

    def read_failures(self, filename):
        """read failures as JSON from filename"""

        if not os.path.exists(filename):
            msg = f"use-failures JSON file does not exist: {filename}"
            raise OSError(2, msg, filename)
        failures = self.read_json(filename)
        return failures

    def read_bugs(self, filename):
        """read bugs as JSON from filename"""

        if not os.path.exists(filename):
            msg = f"bugs JSON file does not exist: {filename}"
            raise OSError(2, msg, filename)
        bugs = self.read_json(filename)
        bugs = [Mock(bug, MOCK_BUG_DEFAULTS) for bug in bugs]
        return bugs

    def write_json(self, filename, data):
        """saves data as JSON to filename"""
        fp = io.open(filename, "w", encoding="utf-8")
        json.dump(data, fp, indent=2, sort_keys=True)
        fp.close()

    def write_tasks(self, save_tasks, tasks):
        """saves tasks as JSON to save_tasks"""
        jtasks = []
        for task in tasks:
            if not isinstance(task, TestTask):
                continue
            jtask = {}
            jtask["id"] = task.id
            jtask["label"] = task.label
            jtask["duration"] = task.duration
            jtask["result"] = task.result
            jtask["state"] = task.state
            jtask["extra"] = self.get_extra(task.id)
            jtags = {}
            for k, v in task.tags.items():
                if k == "createdForUser":
                    jtags[k] = "ci@mozilla.com"
                else:
                    jtags[k] = v
            jtask["tags"] = jtags
            jtask["tier"] = task.tier
            jtask["results"] = [
                {"group": r.group, "ok": r.ok, "duration": r.duration}
                for r in task.results
            ]
            jtask["errors"] = None  # Bug with task.errors property??
            jft = {}
            if self.failure_types is not None and task.id in self.failure_types:
                failure_types = self.failure_types[task.id]  # use cache
            else:
                failure_types = task.failure_types
            for k in failure_types:
                jft[k] = [[f[0], f[1].value] for f in task.failure_types[k]]
            jtask["failure_types"] = jft
            jtasks.append(jtask)
        self.write_json(save_tasks, jtasks)

    def label_to_platform_testname(self, label):
        """convert from label to platform, testname for mach command line"""
        platform = None
        testname = None
        platform_details = label.split("/")
        if len(platform_details) == 2:
            platform, details = platform_details
            words = details.split("-")
            if len(words) > 2:
                platform += "/" + words.pop(0)  # opt or debug
                try:
                    _chunk = int(words[-1])
                    words.pop()
                except ValueError:
                    pass
                words.pop()  # remove test suffix
                testname = "-".join(words)
            else:
                platform = None
        return platform, testname

    def add_attachment_log_for_task(self, bugid, task_id):
        """Adds compressed log for this task to bugid"""

        log_url = f"https://firefox-ci-tc.services.mozilla.com/api/queue/v1/task/{task_id}/artifacts/public/logs/live_backing.log"
        r = requests.get(log_url, headers=self.headers)
        if r.status_code != 200:
            self.error(f"Unable to get log for task: {task_id}")
            return
        attach_fp = tempfile.NamedTemporaryFile()
        fp = gzip.open(attach_fp, "wb")
        fp.write(r.text.encode("utf-8"))
        fp.close()
        if self._initialize_bzapi():
            description = ATTACHMENT_DESCRIPTION + task_id
            file_name = TASK_LOG + ".gz"
            comment = "Added compressed log"
            content_type = "application/gzip"
            try:
                self._bzapi.attachfile(
                    [bugid],
                    attach_fp.name,
                    description,
                    file_name=file_name,
                    comment=comment,
                    content_type=content_type,
                    is_private=False,
                )
            except Fault:
                pass  # Fault expected: Failed to fetch key 9372091 from network storage: The specified key does not exist.

    def get_wpt_path_meta(self, shortpath: str):
        if shortpath.startswith(WPT0):
            path = shortpath
            meta = shortpath.replace(WPT0, WPT_META0, 1)
        elif shortpath.startswith(WPT1):
            path = shortpath
            meta = shortpath.replace(WPT1, WPT_META1, 1)
        elif shortpath.startswith(WPT2):
            path = shortpath
            meta = shortpath.replace(WPT2, WPT_META2, 1)
        elif shortpath.startswith(WPT_MOZILLA):
            shortpath = shortpath[len(WPT_MOZILLA) :]
            path = WPT2 + shortpath
            meta = WPT_META2 + shortpath
        else:
            path = WPT1 + shortpath
            meta = WPT_META1 + shortpath
        return (path, meta)

    def wpt_paths(
        self, shortpath: str
    ) -> tuple[Optional[str], Optional[str], Optional[str], Optional[str]]:
        """
        Analyzes the WPT short path for a test and returns
        (path, manifest, query, anyjs) where
        path is the relative path to the test file
        manifest is the relative path to the file metadata
        query is the test file query paramters (or None)
        anyjs is the html test file as reported by mozci (or None)
        """
        query: Optional[str] = None
        anyjs: Optional[str] = None
        i = shortpath.find("?")
        if i > 0:
            query = shortpath[i:]
            shortpath = shortpath[0:i]
        path, manifest = self.get_wpt_path_meta(shortpath)
        failure_type = not self.isdir(path)
        if failure_type:
            i = path.find(".any.")
            if i > 0:
                anyjs = path  # orig path
                manifest = manifest.replace(path[i:], ".any.js")
                path = path[0:i] + ".any.js"
            else:
                i = path.find(".window.")
                if i > 0:
                    anyjs = path  # orig path
                    manifest = manifest.replace(path[i:], ".window.js")
                    path = path[0:i] + ".window.js"
                else:
                    i = path.find(".worker.")
                    if i > 0:
                        anyjs = path  # orig path
                        manifest = manifest.replace(path[i:], ".worker.js")
                        path = path[0:i] + ".worker.js"
            manifest += ".ini"
        manifest_classic = ""
        if manifest.startswith(WPT_META0):
            manifest_classic = manifest.replace(WPT_META0, WPT_META0_CLASSIC, 1)
            if self.exists(manifest_classic):
                if self.exists(manifest):
                    self.warning(
                        f"Both classic {manifest_classic} and metadata {manifest} manifests exist"
                    )
                else:
                    self.warning(
                        f"Using the classic {manifest_classic} manifest as the metadata manifest {manifest} does not exist"
                    )
                    manifest = manifest_classic
        if not self.exists(path):
            return (None, None, None, None)
        return (path, manifest, query, anyjs)

    def wpt_add_skip_if(self, manifest_str, anyjs, skip_if, bug_reference):
        """
        Edits a WPT manifest string to add disabled condition
        anyjs is a dictionary mapping from filename and any alternate basenames to
        a boolean (indicating if the file has been handled in the manifest).
        Returns additional_comment (if any)
        """

        additional_comment = ""
        disabled_key = False
        disabled = "  disabled:"
        condition_start = "    if "
        condition = condition_start + skip_if + ": " + bug_reference
        lines = manifest_str.splitlines()
        section = None  # name of the section
        i = 0
        n = len(lines)
        while i < n:
            line = lines[i]
            if line.startswith("["):
                if section is not None and not anyjs[section]:  # not yet handled
                    if not disabled_key:
                        lines.insert(i, disabled)
                        i += 1
                    lines.insert(i, condition)
                    lines.insert(i + 1, "")  # blank line after condition
                    i += 2
                    n += 2
                    anyjs[section] = True
                section = line[1:-1]
                if section in anyjs and not anyjs[section]:
                    disabled_key = False
                else:
                    section = None  # ignore section we are not interested in
            elif section is not None:
                if line == disabled:
                    disabled_key = True
                elif line.startswith("  ["):
                    if i > 0 and i - 1 < n and lines[i - 1] == "":
                        del lines[i - 1]
                        i -= 1
                        n -= 1
                    if not disabled_key:
                        lines.insert(i, disabled)
                        i += 1
                        n += 1
                    lines.insert(i, condition)
                    lines.insert(i + 1, "")  # blank line after condition
                    i += 2
                    n += 2
                    anyjs[section] = True
                    section = None
                elif line.startswith("  ") and not line.startswith("    "):
                    if disabled_key:  # insert condition above new key
                        lines.insert(i, condition)
                        i += 1
                        n += 1
                        anyjs[section] = True
                        section = None
                        disabled_key = False
                elif line.startswith("    "):
                    if disabled_key and line == condition:
                        anyjs[section] = True  # condition already present
                        section = None
            i += 1
        if section is not None and not anyjs[section]:  # not yet handled
            if i > 0 and i - 1 < n and lines[i - 1] == "":
                del lines[i - 1]
            if not disabled_key:
                lines.append(disabled)
                i += 1
                n += 1
            lines.append(condition)
            lines.append("")  # blank line after condition
            i += 2
            n += 2
            anyjs[section] = True
        for section in anyjs:
            if not anyjs[section]:
                if i > 0 and i - 1 < n and lines[i - 1] != "":
                    lines.append("")  # blank line before condition
                    i += 1
                    n += 1
                lines.append("[" + section + "]")
                lines.append(disabled)
                lines.append(condition)
                lines.append("")  # blank line after condition
                i += 4
                n += 4
        manifest_str = "\n".join(lines) + "\n"
        return manifest_str, additional_comment

    def reftest_add_fuzzy_if(
        self,
        manifest_str,
        filename,
        fuzzy_if,
        differences,
        pixels,
        lineno,
        zero,
        bug_reference,
    ):
        """
        Edits a reftest manifest string to add disabled condition
        """

        if self.lmp is None:
            from parse_reftest import ListManifestParser

            self.lmp = ListManifestParser(
                self.implicit_vars, self.verbose, self.error, self.warning, self.info
            )
        manifest_str, additional_comment = self.lmp.reftest_add_fuzzy_if(
            manifest_str,
            filename,
            fuzzy_if,
            differences,
            pixels,
            lineno,
            zero,
            bug_reference,
        )
        return manifest_str, additional_comment

    def get_lineno_difference_pixels_status(self, task_id, manifest, allmods):
        """
        Returns
        - lineno in manifest
        - image comparison, max *difference*
        - number of differing *pixels*
        - status (PASS or FAIL)
        as cached from reftest_errorsummary.log for a task
        """

        manifest_obj = self.error_summary.get(manifest, {})
        allmods_obj = manifest_obj.get(allmods, {})
        lineno = allmods_obj.get(LINENO, 0)
        runs_obj = allmods_obj.get(RUNS, {})
        task_obj = runs_obj.get(task_id, {})
        difference = task_obj.get(DIFFERENCE, 0)
        pixels = task_obj.get(PIXELS, 0)
        status = task_obj.get(STATUS, FAIL)
        return lineno, difference, pixels, status

    def reftest_find_lineno(self, manifest, modifiers, allmods):
        """
        Return the line number with modifiers in manifest (else 0)
        """

        lineno = 0
        mods = []
        prefs = []
        for i in range(len(modifiers)):
            if modifiers[i].find("pref(") >= 0 or modifiers[i].find("skip-if(") >= 0:
                prefs.append(modifiers[i])
            else:
                mods.append(modifiers[i])
        m = len(mods)
        manifest_str = io.open(manifest, "r", encoding="utf-8").read()
        lines = manifest_str.splitlines()
        defaults = []
        found = False
        alt_lineno = 0
        for linenum in range(len(lines)):
            line = lines[linenum]
            if len(line) > 0 and line[0] == "#":
                continue
            comment_start = line.find(" #")  # MUST NOT match anchors!
            if comment_start > 0:
                line = line[0:comment_start].strip()
            words = line.split()
            n = len(words)
            if n > 1 and words[0] == "defaults":
                defaults = words[1:].copy()
                continue
            line_defaults = defaults.copy()
            i = 0
            while i < n:
                if words[i].find("pref(") >= 0 or words[i].find("skip-if(") >= 0:
                    line_defaults.append(words[i])
                    del words[i]
                    n -= 1
                else:
                    i += 1
            if (len(prefs) == 0 or prefs == line_defaults) and words == mods:
                found = True
                lineno = linenum + 1
                break
            elif m > 2 and n > 2:
                if words[-3:] == mods[-3:]:
                    alt_lineno = linenum + 1
                else:
                    bwords = [os.path.basename(f) for f in words[-2:]]
                    bmods = [os.path.basename(f) for f in mods[-2:]]
                    if bwords == bmods:
                        alt_lineno = linenum + 1
        if not found:
            if alt_lineno > 0:
                lineno = alt_lineno
                self.warning(
                    f"manifest '{manifest}' found lineno: {lineno}, but it does not contain all the prefs from modifiers,\nSEARCH: {allmods}\nFOUND : {lines[alt_lineno - 1]}"
                )
            else:
                lineno = 0
                self.error(
                    f"manifest '{manifest}' does not contain line with modifiers: {allmods}"
                )
        return lineno

    def get_allpaths(self, task_id, manifest, path):
        """
        Looks up the reftest_errorsummary.log for a task
        and caches the details in self.error_summary by
           task_id, manifest, allmods
        where allmods is the concatenation of all modifiers
        and the details include
        - image comparison, max *difference*
        - number of differing *pixels*
        - status: unexpected PASS or FAIL

        The list iof unique modifiers (allmods) for the given path are returned
        """

        allpaths = []
        words = path.split()
        if len(words) != 3 or words[1] not in TEST_TYPES:
            self.warning(
                f"reftest_errorsummary.log for task: {task_id} has unsupported test type '{path}'"
            )
            return allpaths
        if manifest in self.error_summary:
            for allmods in self.error_summary[manifest]:
                if self.error_summary[manifest][allmods][
                    TEST
                ] == path and task_id in self.error_summary[manifest][allmods].get(
                    RUNS, {}
                ):
                    allpaths.append(path)
            if len(allpaths) > 0:
                return allpaths  # cached (including self tests)
        error_url = f"https://firefox-ci-tc.services.mozilla.com/api/queue/v1/task/{task_id}/artifacts/public/test_info/reftest_errorsummary.log"
        self.vinfo(f"Requesting reftest_errorsummary.log for task: {task_id}")
        r = requests.get(error_url, headers=self.headers)
        if r.status_code != 200:
            self.error(f"Unable to get reftest_errorsummary.log for task: {task_id}")
            return allpaths
        for line in r.text.encode("utf-8").splitlines():
            summary = json.loads(line)
            group = summary.get(GROUP, "")
            if not group or not os.path.exists(group):  # not error line
                continue
            test = summary.get(TEST, None)
            if test is None:
                continue
            if not MODIFIERS in summary:
                self.warning(
                    f"reftest_errorsummary.log for task: {task_id} does not have modifiers for '{test}'"
                )
                continue
            words = test.split()
            if len(words) != 3 or words[1] not in TEST_TYPES:
                self.warning(
                    f"reftest_errorsummary.log for task: {task_id} has unsupported test '{test}'"
                )
                continue
            status = summary.get(STATUS, "")
            if status not in [FAIL, PASS]:
                self.warning(
                    f"reftest_errorsummary.log for task: {task_id} has unknown status: {status} for '{test}'"
                )
                continue
            error = summary.get(SUBTEST, "")
            mods = summary[MODIFIERS]
            allmods = " ".join(mods)
            if group not in self.error_summary:
                self.error_summary[group] = {}
            if allmods not in self.error_summary[group]:
                self.error_summary[group][allmods] = {}
            self.error_summary[group][allmods][TEST] = test
            lineno = self.error_summary[group][allmods].get(LINENO, 0)
            if lineno == 0:
                lineno = self.reftest_find_lineno(group, mods, allmods)
                if lineno > 0:
                    self.error_summary[group][allmods][LINENO] = lineno
            if RUNS not in self.error_summary[group][allmods]:
                self.error_summary[group][allmods][RUNS] = {}
            if task_id not in self.error_summary[group][allmods][RUNS]:
                self.error_summary[group][allmods][RUNS][task_id] = {}
            self.error_summary[group][allmods][RUNS][task_id][ERROR] = error
            if self._subtest_rx is None:
                self._subtest_rx = re.compile(SUBTEST_REGEX)
            m = self._subtest_rx.findall(error)
            if len(m) == 1:
                difference = int(m[0][0])
                pixels = int(m[0][1])
            else:
                difference = 0
                pixels = 0
            if difference > 0:
                self.error_summary[group][allmods][RUNS][task_id][
                    DIFFERENCE
                ] = difference
            if pixels > 0:
                self.error_summary[group][allmods][RUNS][task_id][PIXELS] = pixels
            if status != FAIL:
                self.error_summary[group][allmods][RUNS][task_id][STATUS] = status
            if test == path:
                allpaths.append(test)
        return allpaths
