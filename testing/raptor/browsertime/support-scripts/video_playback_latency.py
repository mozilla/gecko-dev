# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import filters
from base_python_support import BasePythonSupport


class VideoPlaybackLatency(BasePythonSupport):
    def __init__(self, **kwargs):
        super().__init__(**kwargs)
        self._is_android = False
        self._is_chrome = False

    def setup_test(self, test, args):
        from cmdline import CHROME_ANDROID_APPS, CHROMIUM_DISTROS, DESKTOP_APPS

        self._is_android = args.app not in DESKTOP_APPS
        self._is_chrome = (
            args.app in CHROMIUM_DISTROS or args.app in CHROME_ANDROID_APPS
        )

    def modify_command(self, cmd, test):
        # Because of the aspect ratio of Android during recording,
        # most pixels are white, so we need to use a lower fraction.
        fraction = "0.25" if self._is_android else "0.7"

        # Firefox/Safari configuration allows video playback but
        # Chrome needs an explicit switch to enable it.
        if self._is_chrome:
            cmd += [
                "--chrome.enableVideoAutoplay",
                "true",
            ]

        cmd += [
            "--visualMetricsKeyColor",
            "poster",
            "0",
            "128",
            "220",
            "255",
            "220",
            "255",
            fraction,
            "--visualMetricsKeyColor",
            "firstFrame",
            "220",
            "255",
            "0",
            "60",
            "0",
            "60",
            fraction,
            "--visualMetricsKeyColor",
            "secondFrame",
            "0",
            "60",
            "0",
            "60",
            "220",
            "255",
            fraction,
            "--visualMetricsKeyColor",
            "lastFrame",
            "220",
            "255",
            "220",
            "255",
            "0",
            "128",
            fraction,
        ]

    def handle_result(self, bt_result, raw_result, last_result=False, **kwargs):
        measurements = {
            "poster": [],
            "posterEnd": [],
            "firstFrame": [],
            "secondFrame": [],
            "lastFrame": [],
            "estimatedFirstFrameLatency": [],
            "estimatedAnyFrameLatency": [],
        }

        fps = 30.0
        total_duration_ms = 1000.0
        frame_duration_ms = total_duration_ms / fps

        offsets = {
            "firstFrame": 0.0,
            "posterEnd": 0.0,
            "secondFrame": frame_duration_ms * 3.0,
            "lastFrame": total_duration_ms - frame_duration_ms,
        }

        # Gather the key frame start times of each page/cycle
        for cycle in raw_result["visualMetrics"]:
            measurement = {}
            for key, frames in cycle["KeyColorFrames"].items():
                if key not in measurements or not len(frames):
                    continue
                measurement[key] = frames[0]["startTimestamp"]
                if key == "poster":
                    measurement["posterEnd"] = frames[0]["endTimestamp"]

            for key in ["firstFrame", "posterEnd", "secondFrame", "lastFrame"]:
                if key not in measurement:
                    continue
                normalized_value = measurement[key] - offsets[key]
                if normalized_value <= 0:
                    continue
                measurements["estimatedFirstFrameLatency"].append(normalized_value)
                break

            for key, value in measurement.items():
                measurements[key].append(value)
                if key not in offsets:
                    continue
                normalized_value = value - offsets[key]
                if normalized_value <= 0:
                    continue
                measurements["estimatedAnyFrameLatency"].append(normalized_value)

        for measurement, values in measurements.items():
            bt_result["measurements"].setdefault(measurement, []).extend(values)

    def _build_subtest(self, measurement_name, replicates, test):
        unit = test.get("unit", "ms")
        if test.get("subtest_unit"):
            unit = test.get("subtest_unit")

        return {
            "name": measurement_name,
            "lowerIsBetter": test.get("lower_is_better", True),
            "alertThreshold": float(test.get("alert_threshold", 2.0)),
            "unit": unit,
            "replicates": replicates,
            "value": round(filters.geometric_mean(replicates), 3),
        }

    def summarize_test(self, test, suite, **kwargs):
        suite["type"] = "pageload"
        if suite["subtests"] == {}:
            suite["subtests"] = []
        for measurement_name, replicates in test["measurements"].items():
            if not replicates:
                continue
            suite["subtests"].append(
                self._build_subtest(measurement_name, replicates, test)
            )
        suite["subtests"].sort(key=lambda subtest: subtest["name"])
