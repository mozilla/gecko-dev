# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import filters

ADDITIONAL_METRICS = ["powerUsage"]


class BasePythonSupport:
    def __init__(self, **kwargs):
        self.power_test = None

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

        `raw_result` is a single browser-cycle/iteration from Browsertime. Use object
        attributes to store values across browser-cycles, and produce overall results
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
        return measurement_name in ADDITIONAL_METRICS

    def add_additional_metrics(self, test, suite, **kwargs):
        """Adds any additional metrics to a perfherder suite result.

        This method can be called in a test script during summarize_test to
        add any additional metrics that were produced by the test to the suite.
        """
        if self.power_test and "powerUsage" in test["measurements"]:
            suite["subtests"].append(
                self._build_standard_subtest(
                    test,
                    test["measurements"]["powerUsage"],
                    "powerUsage",
                    unit="uWh",
                    lower_is_better=True,
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
