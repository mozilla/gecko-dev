import platform
import subprocess
import sys

from optparse import OptionParser

TARGET_REFRESH_RATE = {
    "11.20": "53.00",
    "default": "60.00",
}

TARGET_RESOLUTION = {"default": "1920 x 1080"}


def get_target_rate():
    if platform.system() == "Darwin":
        (release, versioninfo, machine) = platform.mac_ver()
        versionNums = release.split(".")[:2]
        os_version = "%s.%s" % (versionNums[0], versionNums[1].ljust(2, "0"))
        if os_version in TARGET_REFRESH_RATE:
            return TARGET_REFRESH_RATE[os_version]
        return TARGET_REFRESH_RATE["default"]
    return 0


def get_target_resolution():
    if platform.system() == "Darwin":
        (release, versioninfo, machine) = platform.mac_ver()
        versionNums = release.split(".")[:2]
        os_version = "%s.%s" % (versionNums[0], versionNums[1].ljust(2, "0"))
        if os_version in TARGET_RESOLUTION:
            return TARGET_RESOLUTION[os_version]
        return TARGET_RESOLUTION["default"]
    return 0


def get_refresh_rate():
    if platform.system() == "Darwin":
        # 11.20/aarch64 (mac mini m1) - always has 53.00
        # 14.70 - mix between 60.00 and 24.00
        cmd = "system_profiler SPDisplaysDataType | grep 'UI' | cut -d '@' -f 2 | cut -d ' ' -f 2 | sed 's/Hz//'"
    else:
        return 0

    target_rate = get_target_rate()

    result = subprocess.run(cmd, shell=True, capture_output=True, text=True)
    refresh_rate = result.stdout.strip()
    try:
        refresh_rate = "%.02f" % refresh_rate
    except TypeError:
        pass  # have non numeric type
    print(f"Refresh Rate: {refresh_rate} Hz")

    if str(refresh_rate) != str(target_rate):
        print(
            f"ERROR: expected refresh rate = {target_rate}, instead got {refresh_rate}."
        )
        return 1
    return 0


def get_resolution():
    if platform.system() == "Darwin":
        """
        system_profiler SPDisplaysDataType | grep Resolution
          Resolution: 1920 x 1080 (1080p FHD - Full High Definition)
        """
        cmd = "system_profiler SPDisplaysDataType | grep 'Resolution' | cut -d '(' -f 1 | cut -d ':' -f 2"
    else:
        return 0

    target_resolution = get_target_resolution()

    result = subprocess.run(cmd, shell=True, capture_output=True, text=True)
    resolution = result.stdout.strip()
    print(f"Resolution: {resolution}")

    if str(resolution) != str(target_resolution):
        print(
            f"ERROR: expected resolution = {target_resolution}, instead got {resolution}."
        )
        return 1
    return 0


def main():
    # NOTE: this script is only designed for macosx.
    parser = OptionParser()
    parser.add_option(
        "--check",
        dest="check",
        type="string",
        default="resolution",
        help="Determines the test to run (refresh-rate || resolution).",
    )
    (options, args) = parser.parse_args()
    if options.check == "resolution":
        return get_resolution()
    return get_refresh_rate()


sys.exit(main())
