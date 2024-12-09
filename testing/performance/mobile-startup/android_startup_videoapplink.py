# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import os
import pathlib
import subprocess
import sys
import time

# Add the python packages installed by mozperftest
sys.path.insert(0, os.environ["PYTHON_PACKAGES"])

import cv2
import numpy as np
from mozdevice import ADBDevice

APP_LINK_STARTUP_WEBSITE = "https://theme-crave-demo.myshopify.com/"
PROD_FENIX = "fenix"
PROD_CHRM = "chrome-m"
BACKGROUND_TABS = [
    "https://www.google.com/search?q=toronto+weather",
    "https://en.m.wikipedia.org/wiki/Anemone_hepatica",
    "https://www.amazon.ca/gp/aw/gb?ref_=navm_cs_gb&discounts-widget",
    "https://www.espn.com/nfl/game/_/gameId/401671793/chiefs-falcons",
]
ITERATIONS = 5


class ImageAnalzer:
    def __init__(self, browser):
        self.video = None
        self.browser = browser
        self.width = 0
        self.height = 0
        self.video_name = ""
        self.package_name = os.environ["BROWSER_BINARY"]
        self.device = ADBDevice()
        if self.browser == PROD_FENIX:
            self.intent = "org.mozilla.fenix/org.mozilla.fenix.IntentReceiverActivity"
        elif self.browser == PROD_CHRM:
            self.intent = (
                "com.android.chrome/com.google.android.apps.chrome.IntentDispatcher"
            )
        else:
            raise Exception("Bad browser name")
        self.nav_start_command = (
            f"am start-activity -W -n {self.intent} -a "
            f"android.intent.action.VIEW -d "
        )

        self.device.shell("mkdir -p /sdcard/Download")
        self.device.shell("settings put global window_animation_scale 1")
        self.device.shell("settings put global transition_animation_scale 1")
        self.device.shell("settings put global animator_duration_scale 1")

    def app_setup(self):
        self.device.shell(f"pm clear {self.package_name}")
        time.sleep(3)
        self.skip_onboarding()
        self.device.shell(
            f"pm grant {self.package_name} android.permission.POST_NOTIFICATIONS"
        )  # enabling notifications
        self.create_background_tabs()
        self.device.shell(f"am force-stop {self.package_name}")

    def skip_onboarding(self):
        # Skip onboarding for chrome and fenix
        if self.browser == PROD_CHRM:
            self.device.shell(
                '\'echo "chrome --no-default-browser-check --no-first-run --disable-fre" '
                "> /data/local/tmp/chrome-command-line'"
            )
            self.device.shell("am set-debug-app --persistent com.android.chrome")
        elif self.browser == PROD_FENIX:
            self.device.shell(
                "am start-activity -W -a android.intent.action.MAIN --ez "
                "performancetest true -n org.mozilla.fenix/org.mozilla.fenix.App"
            )

    def create_background_tabs(self):
        # Add background tabs that allow us to see the impact of having background tabs open
        # when we do the cold applink startup test. This makes the test workload more realistic
        # and will also help catch regressions that affect per-open-tab startup work.
        for website in BACKGROUND_TABS:
            self.device.shell(self.nav_start_command + website)
            time.sleep(3)

    def get_video(self, run):
        self.video_name = f"vid{run}_{self.browser}.mp4"
        video_location = f"/sdcard/Download/{self.video_name}"

        # Bug 1927548 - Recording command doesn't use mozdevice shell because the mozdevice shell
        # outputs an adbprocess obj whose adbprocess.proc.kill() does not work when called
        recording = subprocess.Popen(
            [
                "adb",
                "shell",
                "screenrecord",
                "--bugreport",
                video_location,
            ]
        )

        # Navigate to a page
        self.device.shell(self.nav_start_command + APP_LINK_STARTUP_WEBSITE)
        time.sleep(5)
        recording.kill()
        time.sleep(5)
        self.device.command_output(
            ["pull", "-a", video_location, os.environ["TESTING_DIR"]]
        )

        time.sleep(4)
        video_location = pathlib.Path(os.environ["TESTING_DIR"], self.video_name)

        self.video = cv2.VideoCapture(video_location)
        self.width = self.video.get(cv2.CAP_PROP_FRAME_WIDTH)
        self.height = self.video.get(cv2.CAP_PROP_FRAME_HEIGHT)
        self.device.shell(f"am force-stop {self.package_name}")

    def get_image(self, frame_position):
        self.video.set(cv2.CAP_PROP_POS_FRAMES, frame_position)
        ret, frame = self.video.read()
        if not ret:
            raise Exception("Frame not read")
        # We crop out the top 100 pixels in each image as when we have --bug-report in the
        # screen-recording command it displays a timestamp which interferes with the image comparisons
        return frame[100 : int(self.height), 0 : int(self.width)]

    def error(self, img1, img2):
        h = img1.shape[0]
        w = img1.shape[1]
        diff = cv2.subtract(img1, img2)
        err = np.sum(diff**2)
        mse = err / (float(h * w))
        return mse

    def get_cold_view_nav_end_frame(self):
        """
        Returns the index of the frame where the main image on the shopify demo page is displayed
        for the first time.
        Specifically, we find the index of the first frame whose image is within an error of 20
        compared to the final frame, via binary search. The binary search assumes that the error
        compared to the final frame decreases monotonically in the captured frames.
        """
        final_frame_index = self.video.get(cv2.CAP_PROP_FRAME_COUNT) - 1
        final_frame = self.get_image(final_frame_index)

        lo = 0
        hi = final_frame_index

        while lo < hi:
            mid = (lo + hi) // 2
            diff = self.error(self.get_image(mid), final_frame)
            if diff <= 20:
                hi = mid
            else:
                lo = mid + 1
        return lo

    def get_time_from_frame_num(self, frame_num):
        self.video.set(cv2.CAP_PROP_POS_FRAMES, frame_num)
        self.video.read()
        return self.video.get(cv2.CAP_PROP_POS_MSEC)


if __name__ == "__main__":
    if len(sys.argv) < 2:
        raise Exception("Didn't pass the arg properly :(")
    browser = sys.argv[1]
    start_video_timestamp = []

    ImageObject = ImageAnalzer(browser)
    for iteration in range(ITERATIONS):
        ImageObject.app_setup()
        ImageObject.get_video(iteration)
        nav_done_frame = ImageObject.get_cold_view_nav_end_frame()
        start_video_timestamp += [ImageObject.get_time_from_frame_num(nav_done_frame)]
    print(
        'perfMetrics: {"values": ',
        start_video_timestamp,
        ', "name": "applink_startup", "shouldAlert": true',
        "}",
    )
