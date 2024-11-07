# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import copy

import filters
from base_python_support import BasePythonSupport
from logger.logger import RaptorLogger
from results import (
    NON_FIREFOX_BROWSERS,
    NON_FIREFOX_BROWSERS_MOBILE,
    MissingResultsError,
)

LOG = RaptorLogger(component="perftest-support-class")

conversion = (
    ("fnbpaint", "firstPaint"),
    ("fcp", ["paintTiming", "first-contentful-paint"]),
    ("dcf", "timeToDomContentFlushed"),
    ("loadtime", "loadEventEnd"),
    ("largestContentfulPaint", ["largestContentfulPaint", "renderTime"]),
)


def _get_raptor_val(mdict, mname, retval=False):
    # gets the measurement requested, returns the value
    # if one was found, or retval if it couldn't be found
    #
    # mname: either a path to follow (as a list) to get to
    #        a requested field value, or a string to check
    #        if mdict contains it. i.e.
    #        'first-contentful-paint'/'fcp' is found by searching
    #        in mdict['paintTiming'].
    # mdict: a dictionary to look through to find the mname
    #        value.

    if type(mname) is not list:
        if mname in mdict:
            return mdict[mname]
        return retval
    target = mname[-1]
    tmpdict = mdict
    for name in mname[:-1]:
        tmpdict = tmpdict.get(name, {})
    if target in tmpdict:
        return tmpdict[target]

    return retval


class PageloadSupport(BasePythonSupport):
    def __init__(self, **kwargs):
        super().__init__(**kwargs)
        self.perfstats = False
        self.browsertime_visualmetrics = False
        self.accept_zero_vismet = False
        self.subtest_alert_on = ""
        self.app = None
        self.extra_summary_methods = []
        self.test_type = ""
        self.measure = None
        self.power_test = False
        self.failed_vismets = []

    def setup_test(self, next_test, args):
        self.perfstats = next_test.get("perfstats", False)
        self.browsertime_visualmetrics = args.browsertime_visualmetrics
        self.accept_zero_vismet = next_test.get("accept_zero_vismet", False)
        self.subtest_alert_on = next_test.get("alert_on", "")
        self.app = args.app
        self.extra_summary_methods = args.extra_summary_methods
        self.test_type = next_test.get("type", "")
        self.measure = next_test.get("measure", [])
        self.power_test = args.power_test

    def handle_result(self, bt_result, raw_result, last_result=False, **kwargs):
        # extracting values from browserScripts and statistics
        for bt, raptor in conversion:
            if self.measure is not None and bt not in self.measure:
                continue
            # chrome and safari we just measure fcp and loadtime; skip fnbpaint and dcf
            if (
                self.app
                and self.app.lower()
                in NON_FIREFOX_BROWSERS + NON_FIREFOX_BROWSERS_MOBILE
                and bt
                in (
                    "fnbpaint",
                    "dcf",
                )
            ):
                continue

            # FCP uses a different path to get the timing, so we need to do
            # some checks here
            if bt == "fcp" and not _get_raptor_val(
                raw_result["browserScripts"][0]["timings"],
                raptor,
            ):
                continue

            # XXX looping several times in the list, could do better
            for cycle in raw_result["browserScripts"]:
                if bt not in bt_result["measurements"]:
                    bt_result["measurements"][bt] = []
                val = _get_raptor_val(cycle["timings"], raptor)
                if not val:
                    raise MissingResultsError(
                        "Browsertime cycle missing {} measurement".format(raptor)
                    )
                bt_result["measurements"][bt].append(val)

            # let's add the browsertime statistics; we'll use those for overall values
            # instead of calculating our own based on the replicates
            bt_result["statistics"][bt] = _get_raptor_val(
                raw_result["statistics"]["timings"], raptor, retval={}
            )

        if self.perfstats:
            for cycle in raw_result["geckoPerfStats"]:
                for metric in cycle:
                    bt_result["measurements"].setdefault(
                        "perfstat-" + metric, []
                    ).append(cycle[metric])

        if self.browsertime_visualmetrics:
            for cycle in raw_result["visualMetrics"]:
                for metric in cycle:
                    if "progress" in metric.lower():
                        # Bug 1665750 - Determine if we should display progress
                        continue

                    if metric not in self.measure:
                        continue

                    val = cycle[metric]
                    if not self.accept_zero_vismet:
                        if val == 0:
                            self.failed_vismets.append(metric)
                            continue

                    bt_result["measurements"].setdefault(metric, []).append(val)
                    bt_result["statistics"][metric] = raw_result["statistics"][
                        "visualMetrics"
                    ][metric]

        power_vals = raw_result.get("android").get("power", {})
        if power_vals:
            bt_result["measurements"].setdefault("powerUsage", []).extend(
                [round(vals["powerUsage"] * (1 * 10**-6), 2) for vals in power_vals]
            )

    def _process_measurements(self, suite, test, measurement_name, replicates):
        subtest = {}
        subtest["name"] = measurement_name
        subtest["lowerIsBetter"] = test["subtest_lower_is_better"]
        subtest["alertThreshold"] = float(test["alert_threshold"])

        unit = test["subtest_unit"]
        if measurement_name == "cpuTime":
            unit = "ms"
        elif measurement_name == "powerUsage":
            unit = "uWh"
        subtest["unit"] = unit

        # Add the alert window settings if needed here too in case
        # there is no summary value in the test
        for schema_name in (
            "minBackWindow",
            "maxBackWindow",
            "foreWindow",
        ):
            if suite.get(schema_name, None) is not None:
                subtest[schema_name] = suite[schema_name]

        # if 'alert_on' is set for this particular measurement, then we want to set
        # the flag in the perfherder output to turn on alerting for this subtest
        if self.subtest_alert_on is not None:
            if measurement_name in self.subtest_alert_on:
                LOG.info(
                    "turning on subtest alerting for measurement type: %s"
                    % measurement_name
                )
                subtest["shouldAlert"] = True
                if self.app in (
                    "chrome",
                    "chrome-m",
                    "custom-car",
                    "cstm-car-m",
                ):
                    subtest["shouldAlert"] = False
            else:
                # Explicitly set `shouldAlert` to False so that the measurement
                # is not alerted on. Otherwise Perfherder defaults to alerting.
                LOG.info(
                    "turning off subtest alerting for measurement type: %s"
                    % measurement_name
                )
                subtest["shouldAlert"] = False

        if self.power_test and measurement_name == "powerUsage":
            subtest["shouldAlert"] = True

        subtest["replicates"] = replicates
        return subtest

    def summarize_test(self, test, suite, **kwargs):
        for measurement_name, replicates in test["measurements"].items():
            new_subtest = self._process_measurements(
                suite, test, measurement_name, replicates
            )
            if measurement_name not in suite["subtests"]:
                suite["subtests"][measurement_name] = new_subtest
            else:
                suite["subtests"][measurement_name]["replicates"].extend(
                    new_subtest["replicates"]
                )

        # Handle chimeras here, by default the add_additional_metrics
        # parses for all the results together regardless of cold/warm
        cycle_type = "browser-cycle"
        if "warm" in suite["extraOptions"]:
            cycle_type = "page-cycle"

        self.add_additional_metrics(test, suite, cycle_type=cycle_type)

        # Don't alert on cpuTime metrics
        for measurement_name, measurement_info in suite["subtests"].items():
            if "cputime" in measurement_name.lower():
                measurement_info["shouldAlert"] = False

    def summarize_suites(self, suites):
        def _process_geomean(subtest):
            data = subtest["replicates"]
            subtest["value"] = round(filters.geometric_mean(data), 1)

        def _process_alt_method(subtest, alternative_method):
            data = subtest["replicates"]
            if alternative_method == "median":
                subtest["value"] = filters.median(data)

        # converting suites and subtests into lists, and sorting them
        def _process(subtest, method="geomean"):
            if self.test_type == "power":
                subtest["value"] = filters.mean(subtest["replicates"])
            elif method == "geomean":
                _process_geomean(subtest)
            else:
                _process_alt_method(subtest, method)
            return subtest

        for suite in suites:
            suite["subtests"] = [
                _process(subtest)
                for subtest in suite["subtests"].values()
                if subtest["replicates"]
            ]

            # Duplicate for different summary values if needed
            if self.extra_summary_methods:
                new_subtests = []
                for subtest in suite["subtests"]:
                    try:
                        for alternative_method in self.extra_summary_methods:
                            new_subtest = copy.deepcopy(subtest)
                            new_subtest["name"] = (
                                f"{new_subtest['name']} ({alternative_method})"
                            )
                            _process(new_subtest, alternative_method)
                            new_subtests.append(new_subtest)
                    except Exception as e:
                        # Ignore failures here
                        LOG.info(f"Failed to summarize with alternative methods: {e}")
                        pass
                suite["subtests"].extend(new_subtests)

            suite["subtests"].sort(key=lambda subtest: subtest["name"])

    def report_test_success(self):
        if len(self.failed_vismets) > 0:
            LOG.critical(
                "TEST-UNEXPECTED-FAIL | Some visual metrics have an erroneous value of 0."
            )
            LOG.info("Visual metric tests failed: %s" % str(self.failed_vismets))
            return False
        return True
