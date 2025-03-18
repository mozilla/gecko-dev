# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import filters
from base_python_support import BasePythonSupport


class JetStreamSupport(BasePythonSupport):
    def handle_result(self, bt_result, raw_result, **kwargs):
        """Parse a result for the required results.

        See base_python_support.py for what's expected from this method.
        """
        print("raw")
        print(raw_result)
        print("bt")
        print(bt_result)

        score_tracker = {}

        for k, v in raw_result["extras"][0]["js3_res"]["tests"].items():

            score_tracker[k + "-" + "Geometric"] = v["metrics"]["Score"]["current"]
            for measure, metrics in v["tests"].items():
                score_tracker[k + "-" + measure] = metrics["metrics"]["Time"]["current"]
        geometric_measure = [v[0] for k, v in score_tracker.items() if "Geometric" in k]
        jetstream_overall_score = [round(filters.geometric_mean(geometric_measure), 3)]

        for k, v in score_tracker.items():
            bt_result["measurements"][k] = v

        bt_result["measurements"]["score"] = jetstream_overall_score

    def _build_subtest(self, measurement_name, replicates, test):
        unit = test.get("unit", "ms")
        if test.get("subtest_unit"):
            unit = test.get("subtest_unit")

        lower_is_better = test.get(
            "subtest_lower_is_better", test.get("lower_is_better", True)
        )
        if "score" in measurement_name:
            lower_is_better = False
            unit = "score"

        subtest = {
            "unit": unit,
            "alertThreshold": float(test.get("alert_threshold", 2.0)),
            "lowerIsBetter": lower_is_better,
            "name": measurement_name,
            "replicates": replicates,
            "value": round(filters.mean(replicates), 3),
        }

        return subtest

    def summarize_test(self, test, suite, **kwargs):
        """Summarize the measurements found in the test as a suite with subtests.

        See base_python_support.py for what's expected from this method.
        """
        suite["type"] = "benchmark"
        if suite["subtests"] == {}:
            suite["subtests"] = []
        for measurement_name, replicates in test["measurements"].items():
            if not replicates:
                continue
            if self.is_additional_metric(measurement_name):
                continue
            suite["subtests"].append(
                self._build_subtest(measurement_name, replicates, test)
            )

        self.add_additional_metrics(test, suite, **kwargs)
        suite["subtests"].sort(key=lambda subtest: subtest["name"])

        score = 0
        for subtest in suite["subtests"]:
            if subtest["name"] == "score":
                score = subtest["value"]
                break
        suite["value"] = score

    def modify_command(self, cmd, test):
        """Modify the browsertime command to have the appropriate suite name in
        cases where we have multiple variants/versions
        """

        cmd += ["--browsertime.suite_name", test.get("suite_name")]
