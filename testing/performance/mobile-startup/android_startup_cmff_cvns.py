# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import re
import statistics
import sys
import time
from datetime import datetime

import mozdevice

ITERATIONS = 50
DATETIME_FORMAT = "%Y.%m.%d"
PAGE_START_MOZ = re.compile("GeckoSession: handleMessage GeckoView:PageStart uri=")

PROD_FENIX = "fenix"
PROD_FOCUS = "focus"
PROD_GVEX = "geckoview"
PROD_CHRM = "chrome-m"
MOZILLA_PRODUCTS = [PROD_FENIX, PROD_FOCUS, PROD_GVEX]

MEASUREMENT_DATA = ["mean", "median", "standard_deviation"]
OLD_VERSION_FOCUS_PAGE_START_LINE_COUNT = 3
NEW_VERSION_FOCUS_PAGE_START_LINE_COUNT = 2
STDOUT_LINE_COUNT = 2

TEST_COLD_MAIN_FF = "cold_main_first_frame"
TEST_COLD_MAIN_RESTORE = "cold_main_session_restore"
TEST_COLD_VIEW_FF = "cold_view_first_frame"
TEST_COLD_VIEW_NAV_START = "cold_view_nav_start"
TEST_URI = "https://example.com"

PROD_TO_CHANNEL_TO_PKGID = {
    PROD_FENIX: {
        "nightly": "org.mozilla.fenix",
        "beta": "org.mozilla.firefox.beta",
        "release": "org.mozilla.firefox",
        "debug": "org.mozilla.fenix.debug",
    },
    PROD_FOCUS: {
        "nightly": "org.mozilla.focus.nightly",
        "beta": "org.mozilla.focus.beta",  # only present since post-fenix update.
        "release": "org.mozilla.focus.nightly",
        "debug": "org.mozilla.focus.debug",
    },
    PROD_GVEX: {
        "nightly": "org.mozilla.geckoview_example",
        "release": "org.mozilla.geckoview_example",
    },
    PROD_CHRM: {
        "nightly": "com.android.chrome",
        "release": "com.android.chrome",
    },
}
TEST_LIST = [
    "cold_main_first_frame",
    "cold_view_nav_start",
    "cold_view_first_frame",
    "cold_main_session_restore",
]
# "cold_view_first_frame", "cold_main_session_restore" are 2 disabled tests(broken)


class AndroidStartUpUnknownTestError(Exception):
    """
    Test name provided is not one avaiable to test, this is either because
    the test is currently not being tested or a typo in the spelling
    """

    pass


class AndroidStartUpMatchingError(Exception):
    """
    We expected a certain number of matches but did not get them
    """

    pass


class Startup_test:
    def __init__(self, browser, startup_test):
        self.test_name = startup_test
        self.product = browser

    def run(self):
        self.device = mozdevice.ADBDevice(use_root=False)
        self.release_channel = "nightly"
        self.architecture = "arm64-v8a"
        self.startup_cache = True
        self.package_id = PROD_TO_CHANNEL_TO_PKGID[self.product][self.release_channel]
        self.proc_start = re.compile(
            rf"ActivityManager: Start proc \d+:{self.package_id}/"
        )
        self.key_name = f"{self.product}_nightly_{self.architecture}.apk"
        results = self.run_tests()

        # Cleanup
        if self.product in MOZILLA_PRODUCTS:
            self.device.uninstall_app(self.package_id)

        return results

    def should_alert(self, key_name):
        if MEASUREMENT_DATA[2] in key_name:
            return False
        return True

    def run_tests(self):
        measurements = {}
        # Iterate through the tests in the test list
        print(f"Running {self.test_name} on {self.package_id}...")
        time.sleep(self.get_warmup_delay_seconds())
        self.skip_onboarding(self.test_name)
        test_measurements = []

        for i in range(ITERATIONS):
            start_cmd_args = self.get_start_cmd(self.test_name)
            print(start_cmd_args)
            self.device.stop_application(self.package_id)
            time.sleep(1)
            print(f"iteration {i + 1}")
            self.device.shell("logcat -c")
            process = self.device.shell_output(start_cmd_args).splitlines()
            test_measurements.append(self.get_measurement(self.test_name, process))

        self.device.stop_application(self.package_id)
        print(f"{self.test_name}: {str(test_measurements)}")
        # Bug 1934023 - create way to pass median and still have replicates available
        measurements[f"{self.test_name}.{MEASUREMENT_DATA[0]}"] = [
            statistics.mean(test_measurements)
        ]
        print(f"Mean: {statistics.mean(test_measurements)}")
        measurements[f"{self.test_name}.{MEASUREMENT_DATA[1]}"] = [
            statistics.median(test_measurements)
        ]
        print(f"Median: {statistics.median(test_measurements)}")
        if ITERATIONS > 1:
            measurements[f"{self.test_name}.{MEASUREMENT_DATA[2]}"] = [
                statistics.stdev(test_measurements)
            ]
            print(f"Standard Deviation: {statistics.stdev(test_measurements)}")

        return measurements

    def get_measurement(self, test_name, stdout):
        if test_name in [TEST_COLD_MAIN_FF, TEST_COLD_VIEW_FF]:
            return self.get_measurement_from_am_start_log(stdout)
        elif (
            test_name in [TEST_COLD_VIEW_NAV_START, TEST_COLD_MAIN_RESTORE]
            and self.product in MOZILLA_PRODUCTS
        ):
            # We must sleep until the Navigation::Start event occurs. If we don't
            # the script will fail. This can take up to 14s on the G5
            time.sleep(17)
            proc = self.device.shell_output("logcat -d")
            return self.get_measurement_from_nav_start_logcat(proc)
        else:
            raise AndroidStartUpUnknownTestError(
                "invalid test settings selected, please double check that "
                "the test name is valid and that the test is supported for "
                "the browser you are testing"
            )

    def get_measurement_from_am_start_log(self, stdout):
        total_time_prefix = "TotalTime: "
        matching_lines = [line for line in stdout if line.startswith(total_time_prefix)]
        if len(matching_lines) != 1:
            raise AndroidStartUpMatchingError(
                f"Each run should only have 1 {total_time_prefix}."
                f"However, this run unexpectedly had {matching_lines} matching lines"
            )
        duration = int(matching_lines[0][len(total_time_prefix) :])
        return duration

    def get_measurement_from_nav_start_logcat(self, process_output):
        def __line_to_datetime(line):
            date_str = " ".join(line.split(" ")[:2])  # e.g. "05-18 14:32:47.366"
            # strptime needs microseconds. logcat outputs millis so we append zeroes
            date_str_with_micros = date_str + "000"
            return datetime.strptime(date_str_with_micros, "%m-%d %H:%M:%S.%f")

        def __get_proc_start_datetime():
            # This regex may not work on older versions of Android: we don't care
            # yet because supporting older versions isn't in our requirements.
            proc_start_lines = [line for line in lines if self.proc_start.search(line)]
            if len(proc_start_lines) != 1:
                raise AndroidStartUpMatchingError(
                    f"Expected to match 1 process start string but matched {len(proc_start_lines)}"
                )
            return __line_to_datetime(proc_start_lines[0])

        def __get_page_start_datetime():
            page_start_lines = [line for line in lines if PAGE_START_MOZ.search(line)]
            page_start_line_count = len(page_start_lines)
            page_start_assert_msg = "found len=" + str(page_start_line_count)

            # In focus versions <= v8.8.2, it logs 3 PageStart lines and these include actual uris.
            # We need to handle our assertion differently due to the different line count. In focus
            # versions >= v8.8.3, this measurement is broken because the logcat were removed.
            is_old_version_of_focus = (
                "about:blank" in page_start_lines[0] and self.product == PROD_FOCUS
            )
            if is_old_version_of_focus:
                assert (
                    page_start_line_count
                    == OLD_VERSION_FOCUS_PAGE_START_LINE_COUNT  # should be 3
                ), page_start_assert_msg  # Lines: about:blank, target URL, target URL.
            else:
                assert (
                    page_start_line_count
                    == NEW_VERSION_FOCUS_PAGE_START_LINE_COUNT  # Should be 2
                ), page_start_assert_msg  # Lines: about:blank, target URL.
            return __line_to_datetime(
                page_start_lines[1]
            )  # 2nd PageStart is for target URL.

        lines = process_output.split("\n")
        elapsed_seconds = (
            __get_page_start_datetime() - __get_proc_start_datetime()
        ).total_seconds()
        elapsed_millis = round(elapsed_seconds * 1000)
        return elapsed_millis

    def get_warmup_delay_seconds(self):
        """
        We've been told the start up cache is populated ~60s after first start up. As such,
        we should measure start up with the start up cache populated. If the
        args say we shouldn't wait, we only wait a short duration ~= visual completeness.
        """
        return 60 if self.startup_cache else 5

    def get_start_cmd(self, test_name):
        intent_action_prefix = "android.intent.action.{}"
        if test_name in [TEST_COLD_MAIN_FF, TEST_COLD_MAIN_RESTORE]:
            intent = (
                f"-a {intent_action_prefix.format('MAIN')} "
                f"-c android.intent.category.LAUNCHER"
            )
        elif test_name in [TEST_COLD_VIEW_FF, TEST_COLD_VIEW_NAV_START]:
            intent = f"-a {intent_action_prefix.format('VIEW')} -d {TEST_URI}"
        else:
            raise AndroidStartUpUnknownTestError(
                "Unknown test provided please double check the test name and spelling"
            )

        # You can't launch an app without an pkg_id/activity pair
        component_name = self.get_component_name_for_intent(intent)
        cmd = f"am start-activity -W -n {component_name} {intent} "

        # If focus skip onboarding: it is not stateful so must be sent for every cold start intent
        if self.product == PROD_FOCUS:
            cmd += "--ez performancetest true"

        return cmd

    def get_component_name_for_intent(self, intent):
        resolve_component_args = (
            f"cmd package resolve-activity --brief {intent} {self.package_id}"
        )
        result_output = self.device.shell_output(resolve_component_args)
        stdout = result_output.splitlines()
        if len(stdout) != STDOUT_LINE_COUNT:  # Should be 2
            if "No activity found" in stdout:
                raise Exception("Please verify your apk is installed")
            raise AndroidStartUpMatchingError(f"expected 2 lines. Got: {stdout}")
        return stdout[1]

    def skip_onboarding(self, test_name):
        self.device.enable_notifications(self.package_id)
        if self.product in MOZILLA_PRODUCTS:
            self.skip_app_onboarding()

        if self.product == PROD_FOCUS or test_name not in {
            TEST_COLD_MAIN_FF,
            TEST_COLD_MAIN_RESTORE,
        }:
            return

    def skip_app_onboarding(self):
        """
        We skip onboarding for focus in measure_start_up.py because it's stateful
        and needs to be called for every cold start intent.
        Onboarding only visibly gets in the way of our MAIN test results.
        """
        # This sets mutable state we only need to pass this flag once, before we start the test
        self.device.shell(
            f"am start-activity -W -a android.intent.action.MAIN --ez "
            f"performancetest true -n {self.package_id}/org.mozilla.fenix.App"
        )
        time.sleep(4)  # ensure skip onboarding call has time to propagate.


if __name__ == "__main__":
    if len(sys.argv) < 2:
        raise Exception("Didn't pass the arg properly :(")
    print(len(sys.argv))
    browser = sys.argv[1]
    test = sys.argv[2]
    start_video_timestamp = []

    Startup = Startup_test(browser, test)
    startup_data = Startup.run()
    for measurement in MEASUREMENT_DATA:
        print(
            'perfMetrics: {"values": ',
            startup_data[f"{test}.{measurement}"],
            ', "name": "' + f"{test}.{measurement}" + '", "shouldAlert": true',
            "}",
        )
