# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import filters
from cmdline import FIREFOX_APPS
from utils import flatten

ADDITIONAL_METRICS = [
    "cpuTime",
    "powerUsage",
    "powerUsagePageload",
    "powerUsageSupport",
    "wallclock-for-tracking-only",
]


class BasePythonSupport:
    def __init__(self, **kwargs):
        self.power_test = None
        self.app = None
        self.raw_result = []
        self.bt_result = []

    def save_data(self, raw_result, bt_result):
        """
        This function is used to save the bt_result, raw_result that way we can reference
        and use this data across the different BasePythonSupport classes. Each of the
        elements in self.raw_result is an individual page_cycle. Each of the values within
        a metric in one of those page_cycles is a separate browser iteration.

        :param dict raw_result: all non-browsertime parts of the test, should
            include things like the android version, ttfb, and cpu usage
        :param dict bt_result: browsertime results/version info from the test

        return: None
        """
        self.raw_result += [raw_result]
        self.bt_result += [bt_result]

    def setup_test(self, test, args):
        """Used to setup the test.

        The `test` arg is the test itself with all of its current settings.
        It can be modified as needed to add additional information to the
        test that will run.

        The `args` arg contain all the user-specified args, or the default
        settings for them. These can be useful for changing the behaviour
        based on the app, or if we're running locally.

        No return is expected. The `test` argument can be changed directly.
        """
        self.power_test = args.power_test
        self.app = args.app

    def modify_command(self, cmd, test):
        """Used to modify the Browsertime command before running the test.

        The `cmd` arg holds the current browsertime command to run. It can
        be changed directly to change how browsertime runs.

        The `test` arg is the test itself with all of its current settings.
        It can be modified as needed to add additional information to the
        test that will run.
        """
        pass

    def handle_result(self, bt_result, raw_result, last_result=False, **kwargs):
        """Parse a result for the required results.

        This method handles parsing a new result from Browsertime. The
        expected data returned should follow the following format:
        {
            "custom_data": True,
            "measurements": {
                "fcp": [0, 1, 1, 2, ...],
                "custom-metric-name": [9, 9, 9, 8, ...]
            }
        }

        `bt_result` holds that current results that have been parsed. Add
        new measurements as a dictionary to `bt_result["measurements"]`. Watch
        out for overriding other measurements.

        `raw_result` is a single page-cycle/iteration from Browsertime. Use object
        attributes to store values across page-cycles, and produce overall results
        on the last run (denoted by `last_result`).
        """
        pass

    def summarize_test(self, test, suite, **kwargs):
        """Summarize the measurements found in the test as a suite with subtests.

        Note that the same suite will be passed when the test is the same.

        Here's a small example of an expected suite result
        (see performance-artifact-schema.json for more information):
            {
                "name": "pageload-benchmark",
                "type": "pageload",
                "extraOptions": ["fission", "cold", "webrender"],
                "tags": ["fission", "cold", "webrender"],
                "lowerIsBetter": true,
                "unit": "ms",
                "alertThreshold": 2.0,
                "subtests": [{
                    "name": "totalTimePerSite",
                    "lowerIsBetter": true,
                    "alertThreshold": 2.0,
                    "unit": "ms",
                    "shouldAlert": false,
                    "replicates": [
                        6490.47, 6700.73, 6619.47,
                        6823.07, 6541.53, 7152.67,
                        6553.4, 6471.53, 6548.8, 6548.87
                    ],
                    "value": 6553.4
            }

        Some fields are setup by default for the suite:
            {
                "name": test["name"],
                "type": test["type"],
                "extraOptions": extra_options,
                "tags": test.get("tags", []) + extra_options,
                "lowerIsBetter": test["lower_is_better"],
                "unit": test["unit"],
                "alertThreshold": float(test["alert_threshold"]),
                "subtests": {},
            }
        """
        pass

    def summarize_suites(self, suites):
        """Used to summarize all the suites.

        The `suites` arg provides all the suites that were built
        in this test run. This method can be used to modify those,
        or to create new ones based on the others. For instance,
        it can be used to create "duplicate" suites that use
        different methods for the summary value.

        Note that the subtest/suite names should be changed if
        existing suites are duplicated so that no conflicts arise
        during perfherder data ingestion.
        """
        pass

    def _build_standard_subtest(
        self,
        test,
        replicates,
        measurement_name,
        unit=None,
        lower_is_better=None,
        should_alert=True,
    ):
        """Produce a standard subtest entry with the given parameters."""
        return {
            "unit": unit or test.get("unit", "ms"),
            "alertThreshold": float(test.get("alert_threshold", 2.0)),
            "lowerIsBetter": (
                lower_is_better
                or test.get(
                    "subtest_lower_is_better", test.get("lower_is_better", True)
                )
            ),
            "name": measurement_name,
            "replicates": replicates,
            "shouldAlert": should_alert,
            "value": round(filters.mean(replicates), 3),
        }

    def is_additional_metric(self, measurement_name):
        """Helper method for determining additional metrics.

        For any additional metrics, there is usually a single way of processing
        them (see add_additional_metrics). For example, the power usage data
        is always produced, and handled in the same way no matter which test
        is being run. However, the measurements can get mixed in with data
        that is specific to the test itself and this method can help with skipping
        them.
        """
        return measurement_name in ADDITIONAL_METRICS or any(
            metric in measurement_name for metric in ADDITIONAL_METRICS
        )

    def _gather_browser_cycles(self, test, results):
        """Searches, and returns the browser-cycle results for a test.

        :param dict test: The test for search for.
        :param list results: The results to search through (pairings of raw_result,
            and bt_result).
        :return list: A list containing a tuple pairing for the raw_result, and
            bt_result.
        """
        for raw_result, bt_result in results:
            if bt_result["name"] == test["name"]:
                return [(raw_result, bt_result)]
        raise Exception(f"Unable to find the test {test['name']} in the saved results")

    def _gather_page_cycles(self, test, results):
        """Searches, and returns the page-cycle results for a test.

        :param dict test: The test for search for.
        :param list results: The results to search through (pairings of raw_result,
            and bt_result).
        :return list: A list containing all tuple pairings for the raw_result, and
            bt_result across page-cycles.
        """
        page_cycle_results = []

        found_first = False
        for raw_result, bt_result in results:
            if bt_result["name"] != test["name"]:
                continue
            if not found_first:
                found_first = True
            else:
                page_cycle_results.append((raw_result, bt_result))

        if not page_cycle_results:
            raise Exception(f"Unable to find any page cycles for test {test['name']}")

        return page_cycle_results

    def _gather_power_usage_measurements(self, raw_result):
        """Gathers all possible power usage measurements from a result.

        :param dict raw_result: The results of the test.

        :return dict: A dict containing the measurements found.
        """
        default_power_settings = {"unit": "uWh", "lower_is_better": True}
        power_usage_measurements = {}

        def __convert_from_pico_to_micro(vals):
            return [round(v * (1 * 10**-6), 2) for v in vals]

        # Gather power usage measurements produced in SupportMeasurements
        # or as part of the profiling.js code (for Windows 11 power usage)
        for res in raw_result["extras"]:
            power_usage_search_name = "powerUsagePageload"
            if any("powerUsageSupport" in metric for metric in res):
                power_usage_search_name = "powerUsageSupport"

            for metric, vals in res.items():
                if power_usage_search_name not in metric:
                    continue
                if any(isinstance(val, dict) for val in vals):
                    flat_power_data = flatten(vals, (), sep="_")
                    for powerMetric, powerVals in flat_power_data.items():
                        power_usage_measurements.setdefault(
                            powerMetric.replace(power_usage_search_name, "powerUsage"),
                            dict(default_power_settings),
                        ).setdefault("replicates", []).extend(
                            __convert_from_pico_to_micro(powerVals)
                        )
                else:
                    power_usage_measurements.setdefault(
                        metric.replace(power_usage_search_name, "powerUsage"),
                        dict(default_power_settings),
                    ).setdefault("replicates", []).extend(
                        __convert_from_pico_to_micro(vals)
                    )

        # Gather pageload measurements produced by browsertime only if there
        # is no power usage data gathered from above since that one is test
        # specific
        if not power_usage_measurements:
            power_vals = raw_result.get("android").get("power", {})
            if power_vals:
                power_usage_measurements.setdefault(
                    "powerUsage", dict(default_power_settings)
                ).setdefault("replicates", []).extend(
                    __convert_from_pico_to_micro(
                        [vals["powerUsage"] for vals in power_vals]
                    )
                )

        return power_usage_measurements

    def _gather_cputime_measurements(self, raw_result):
        """Gathers all possible cpuTime measurements from a result.

        :param dict raw_result: The results of the test.

        :return dict: A dict containing the measurements found.
        """
        default_cputime_settings = {"unit": "ms", "lower_is_better": True}
        cpuTime_measurements = {}

        # Gather support cpuTime measurements (e.g. benchmarks)
        for res in raw_result["extras"]:
            for metric, vals in res.items():
                if metric != "cpuTime":
                    continue
                cpuTime_measurements.setdefault(
                    "cpuTime", dict(default_cputime_settings)
                ).setdefault("replicates", []).extend(vals)

        # Gather pageload cpuTime measurements, but only if benchmark
        # cpuTime wasn't gathered since they both use the same name
        if "cpuTime" not in cpuTime_measurements:
            cpu_vals = raw_result.get("cpu", [])
            if cpu_vals and self.app in FIREFOX_APPS:
                cpuTime_measurements.setdefault(
                    "cpuTime", dict(default_cputime_settings)
                )["replicates"] = cpu_vals

        return cpuTime_measurements

    def _gather_wallclock_measurements(self, raw_result):
        """Gathers the wallclock measurements from a result.

        :param dict raw_result: The results of the test.

        :return dict: A dict containing the measurements found.
        """
        wallclock_measurements = {}
        for res in raw_result["extras"]:
            for metric, vals in res.items():
                if metric != "wallclock-for-tracking-only":
                    continue
                wallclock_measurements.setdefault(
                    metric, {"unit": "ms", "lower_is_better": True}
                ).setdefault("replicates", []).extend(vals)
        return wallclock_measurements

    def _gather_additional_measurements(self, raw_result, bt_result):
        """Gathers all possible measurements from a result.

        :param dict raw_result: The results of the test.
        :param dict bt_result: Information about the test.

        :return dict: A dict containing the measurements found.
        """
        measurements = {}

        measurements.update(self._gather_power_usage_measurements(raw_result))
        measurements.update(self._gather_cputime_measurements(raw_result))
        measurements.update(self._gather_wallclock_measurements(raw_result))

        return measurements

    def add_additional_metrics(self, test, suite, exclude=[], cycle_type="", **kwargs):
        """Adds any additional metrics to a perfherder suite result.

        This method can be called in a test script during summarize_test to
        add any additional metrics that were produced by the test to the suite.
        By default, this method will attempt to gather all posible additional
        metrics. Use the `exclude` argument to exclude metrics.

        :param dict test: The test to gather measurements from.
        :param dict suite: The suite to add parsed measurements to.
        :param list exclude: A list of metrics not to parse.
        :param str cycle_type: The type of cycle to gather measurements from. Can
            either be "browser-cycle" or "page-cycle". By default, all cycles
            will be parsed into a single metric. Used when parsing information
            from the saved data (raw_result/bt_result). The "browser-cycle" data
            comes from the first raw_result entry for a test. "page-cycle" data
            comes from all the other entries in the raw_result, and excludes the
            first one.
        """
        results_to_parse = zip(self.raw_result, self.bt_result)
        if cycle_type == "browser-cycle":
            results_to_parse = self._gather_browser_cycles(test, results_to_parse)
        elif cycle_type == "page-cycle":
            results_to_parse = self._gather_page_cycles(test, results_to_parse)

        all_measurements = {}
        for raw_result, bt_result in results_to_parse:
            measurements = self._gather_additional_measurements(raw_result, bt_result)

            for measurement, measurement_info in measurements.items():
                if measurement not in all_measurements:
                    all_measurements[measurement] = measurement_info
                else:
                    all_measurements[measurement]["replicates"].extend(
                        measurement_info["replicates"]
                    )

        # Add any requested additional metrics
        for measurement, measurement_info in all_measurements.items():
            if measurement in exclude:
                continue

            if kwargs.get(measurement, None):
                kwargs["unit"] = kwargs[measurement].get(
                    "unit", measurement_info["unit"]
                )
                kwargs["lower_is_better"] = kwargs[measurement].get(
                    "lower_is_better", measurement_info["lower_is_better"]
                )
            else:
                kwargs["unit"] = measurement_info["unit"]
                kwargs["lower_is_better"] = measurement_info["lower_is_better"]

            if isinstance(suite["subtests"], dict):
                suite["subtests"][measurement] = self._build_standard_subtest(
                    test, measurement_info["replicates"], measurement, **kwargs
                )
            else:
                suite["subtests"].append(
                    self._build_standard_subtest(
                        test, measurement_info["replicates"], measurement, **kwargs
                    )
                )

    def report_test_success(self):
        """Used to denote custom test failures.

        If a test fails, and gets detected in the support scripts, this
        method can be used to return False and fail the test run. If the
        test is successfull, True should be returned (which is the default).
        """
        return True

    def clean_up(self):
        """Perform cleanup operations to release resources."""
        pass
