"""Get the link to download Fx for any supported platform.

Takes two arguments: <language> <version>"""

from platform import uname
from sys import argv, exit

FX_DOWNLOAD_DIR_URL = "https://archive.mozilla.org/pub/firefox/releases/"


def get_fx_executable_name(version):
    u = uname()

    if u.system == "Darwin":
        platform = "mac"
        executable_name = f"Firefox {version}.dmg"

    if u.system == "Linux":
        if "64" in u.machine:
            platform = "linux-x86_64"
        else:
            platform = "linux-x86_64"
        if int(version.split(".")[0]) < 135:
            executable_name = f"firefox-{version}.tar.bz2"
        else:
            executable_name = f"firefox-{version}.tar.xz"

    if u.system == "Windows":
        if u.machine == "ARM64":
            platform = "win64-aarch64"
        elif "64" in u.machine:
            platform = "win64"
        else:
            platform = "win32"
        executable_name = f"Firefox Setup {version}.exe"

    return platform, executable_name.replace(" ", "%20")


def main():
    if len(argv) < 3:
        print("Usage: python collect_executables.py <language> <version>")
        print("    Example: python collect_executables.py en-US 135.0b2")
        return 1

    language, version = argv[1], argv[2]
    platform, executable_name = get_fx_executable_name(version)

    fx_download_executable_url = (
        rf"{FX_DOWNLOAD_DIR_URL}{version}/{platform}/{language}/{executable_name}"
    )
    print(fx_download_executable_url)
    return 0


if __name__ == "__main__":
    exit(main())
