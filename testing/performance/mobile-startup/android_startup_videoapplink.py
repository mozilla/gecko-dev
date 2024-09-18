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

PROD_FENIX = "fenix"
PROD_CHRM = "chrome-m"


class ImageAnalzer:
    def __init__(self, browser):
        self.video = None
        self.browser = browser
        self.intent = ""
        self.width = 0
        self.height = 0
        self.video_name = ""
        self.package_name = os.environ["BROWSER_BINARY"]

        adb_shell("mkdir -p /sdcard/Download")

        if self.browser == PROD_FENIX:
            self.intent = "org.mozilla.fenix/.IntentReceiverActivity"
            adb_shell(
                "am start-activity -W -a android.intent.action.MAIN --ez "
                "performancetest true -n org.mozilla.fenix/org.mozilla.fenix.App"
            )
        elif self.browser == PROD_CHRM:
            self.intent = "com.android.chrome/com.google.android.apps.chrome.Main"
            adb_shell(f"am start-activity -W -n {self.intent}")
            time.sleep(5)
            adb_shell("input tap 500 1900")  # tap on the skip account creation
            time.sleep(5)
        else:
            raise Exception("bad browser name")
        adb_shell(
            f"pm grant {self.package_name} android.permission.POST_NOTIFICATIONS"
        )  # enable notifications
        time.sleep(10)  # allow enable notifications to propagate
        force_stop(self.package_name)
        time.sleep(1)

    def get_video(self, run):
        self.video_name = f"vid{run}_{browser}.mp4"
        nav_start_command = (
            f"am start-activity -W -n {self.intent} -a "
            "android.intent.action.VIEW -d https://theme-crave-demo.myshopify.com/"
        )
        video_location = f"/sdcard/Download/{self.video_name}"

        # Start Recording
        recording = subprocess.Popen(
            [
                "adb",
                "shell",
                "screenrecord",
                video_location,
            ]
        )
        time.sleep(1)

        # Navigate to a page
        adb_shell(nav_start_command)
        time.sleep(5)
        recording.kill()

        time.sleep(10)
        subprocess.Popen(
            [
                "adb",
                "pull",
                "-a",
                video_location,
                os.environ["TESTING_DIR"],
            ]
        )

        time.sleep(4)
        video_location = pathlib.Path(os.environ["TESTING_DIR"], self.video_name)

        self.video = cv2.VideoCapture(video_location)
        self.width = self.video.get(cv2.CAP_PROP_FRAME_WIDTH)
        self.height = self.video.get(cv2.CAP_PROP_FRAME_HEIGHT)
        force_stop(self.package_name)

    def get_image(self, frame_position):
        self.video.set(cv2.CAP_PROP_POS_FRAMES, frame_position)
        ret, frame = self.video.read()
        if not ret:
            raise Exception("Frame not read")
        return frame

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


def adb_shell(args):
    subprocess.getoutput([f"adb shell {args}"])


def force_stop(package_name):
    adb_shell(f"am force-stop {package_name}")


if __name__ == "__main__":
    if len(sys.argv) < 2:
        raise Exception("Didn't pass the arg properly :(")
    browser = sys.argv[1]
    start_video_timestamp = []

    ImageObject = ImageAnalzer(browser)
    for iteration in range(50):
        ImageObject.get_video(iteration)
        nav_done_frame = ImageObject.get_cold_view_nav_end_frame()
        start_video_timestamp += [ImageObject.get_time_from_frame_num(nav_done_frame)]
    print(
        'perfMetrics: {"values": ',
        start_video_timestamp,
        ', "name": "cold_view_nav_end", "shouldAlert": true',
        "}",
    )
