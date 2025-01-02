#!/usr/bin/python3
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import re
import subprocess
import sys

# Constants
MOBILE_ANDROID_DIR = os.path.abspath(os.path.dirname(__file__))
CHANGELOG_FILE = os.path.join(
    MOBILE_ANDROID_DIR, "android-components/docs/changelog.md"
)
SOURCE_JSON = os.path.join(
    MOBILE_ANDROID_DIR, "../../services/settings/dumps/main/search-telemetry-v2.json"
)
TARGET_JSON = os.path.join(
    MOBILE_ANDROID_DIR,
    "android-components/components/feature/search/src/main/assets/search/search_telemetry_v2.json",
)
EXPIRED_STRING_VERSION_OFFSET = 3


def check_ripgrep_installed():
    """Check if ripgrep (rg) is installed."""
    try:
        subprocess.run(["rg", "--version"], capture_output=True, check=True)
    except FileNotFoundError:
        print(
            "ERROR: ripgrep (rg) is not installed. Please install ripgrep and try again."
        )
        print(
            "See installation instructions here: https://github.com/BurntSushi/ripgrep?tab=readme-ov-file#installation"
        )
        sys.exit(1)


def check_uncommitted_changes():
    """Check for uncommitted changes in the git repository."""
    result = subprocess.run(
        ["git", "status", "--porcelain", "--untracked-files=no"],
        capture_output=True,
        text=True,
        check=False,
    )
    if result.stdout.strip():
        print("ERROR: Please commit changes before continuing.")
        sys.exit(1)


def get_bug_id():
    """Get BUG_ID from script arguments."""
    if len(sys.argv) < 2:
        print("Usage: python script.py BUG_ID")
        sys.exit(1)
    return sys.argv[1]


def get_previous_version():
    """Extract the previous version number from the changelog."""
    with open(CHANGELOG_FILE, "r") as file:
        content = file.read()
    match = re.search(r"# (\d+)\.0 \(In Development\)", content)
    if not match:
        print(
            "ERROR: Unable to extract the previous version number from the changelog file."
        )
        sys.exit(1)
    return int(match.group(1))


def update_changelog(previous_version, new_version):
    """Update the changelog with the new version number."""
    with open(CHANGELOG_FILE, "r") as file:
        content = file.read()
    updated_content = content.replace(
        f"# {previous_version}.0 (In Development)",
        f"# {new_version}.0 (In Development)\n\n# {previous_version}.0",
    )
    with open(CHANGELOG_FILE, "w") as file:
        file.write(updated_content)


def find_expired_strings(expired_string_version):
    """Find strings to be removed."""
    rg_command = [
        "rg",
        "-g",
        "**/values/**",
        "-U",
        f'(<!--.*-->[\\r\\n\\s]*)?<string[^>]*moz:removedIn="{expired_string_version}"[^>]*>.*?</string>',
        MOBILE_ANDROID_DIR,
    ]
    result = subprocess.run(rg_command, capture_output=True, text=True, check=False)
    expired_strings = []
    if result.stdout.strip():
        for line in result.stdout.splitlines():
            match = re.search(r'<string name="([^"]+)"', line)
            if match:
                expired_strings.append(match.group(1))
    return expired_strings


def remove_expired_strings(expired_string_version):
    """Remove expired strings in string.xml files using the original ripgrep."""
    rg_command = [
        "rg",
        "-g",
        "**/values/**",
        "-l",
        f'moz:removedIn="{expired_string_version}"',
        MOBILE_ANDROID_DIR,
    ]
    result = subprocess.run(rg_command, capture_output=True, text=True, check=False)
    if result.stdout.strip():
        files = result.stdout.strip().splitlines()
        bash_command = (
            f"echo {' '.join(files)} | xargs perl -0777 -pi -e "
            f'"s/(\\s*<!--(?:(?!<!--)[\\s\\S])*?-->\\s*)?<string[^>]*moz:removedIn=\\"{expired_string_version}\\"[^>]*>[^<]*<\\/string>//g"'
        )
        subprocess.run(bash_command, shell=True, check=True, executable="/bin/bash")
        return True
    return False


def update_json_if_necessary():
    """Check if JSON files differ and copy if necessary."""
    if os.path.exists(SOURCE_JSON) and os.path.exists(TARGET_JSON):
        result = subprocess.run(["cmp", "-s", SOURCE_JSON, TARGET_JSON], check=False)
        if result.returncode != 0:  # Files differ
            subprocess.run(["cp", SOURCE_JSON, TARGET_JSON], check=False)
            return True
    return False


def search_remaining_occurrences(removed_strings):
    """Search for remaining occurrences of each removed string."""
    remaining_use_message = ""
    for name in removed_strings:
        rg_command = [
            "rg",
            "-n",
            "--pcre2",
            f"{name}(?![a-zA-Z0-9_-])",
            MOBILE_ANDROID_DIR,
            "-g",
            "!**/strings.xml",
        ]
        result = subprocess.run(rg_command, capture_output=True, text=True, check=False)
        if result.stdout.strip():
            lines = result.stdout.strip().splitlines()
            remaining_use_message += (
                f"\n- \033[31m\033[1m{name}\033[0m ({len(lines)}):\n"
            )
            for line in lines:
                remaining_use_message += f"\t· {line}\n"
    return remaining_use_message


def commit_changes(bug_id, new_version_number, strings_removed, json_updated):
    """Commit all changes with a constructed commit message."""
    commit_message = (
        f"Bug {bug_id} - Start the nightly {new_version_number} development cycle\n\n"
    )
    if strings_removed:
        commit_message += f"Strings expiring in version {new_version_number - EXPIRED_STRING_VERSION_OFFSET} have been removed\n"
    if json_updated:
        commit_message += (
            "search_telemetry_v2.json was updated in Android Components, based on the "
            "content of services/settings/dumps/main/search-telemetry-v2.json\n"
        )
    subprocess.run(["git", "add", "-u"], check=False)
    subprocess.run(["git", "commit", "--quiet", "-m", commit_message], check=False)


def main():
    check_ripgrep_installed()
    check_uncommitted_changes()
    bug_id = get_bug_id()
    previous_version = get_previous_version()
    new_version = previous_version + 1
    expired_string_version = new_version - EXPIRED_STRING_VERSION_OFFSET

    # Update changelog
    update_changelog(previous_version, new_version)

    # Find and remove expired strings
    expired_strings = find_expired_strings(expired_string_version)
    if expired_strings:
        strings_removed = remove_expired_strings(expired_string_version)
        remaining_use_message = search_remaining_occurrences(expired_strings)
    else:
        strings_removed = False
        remaining_use_message = ""

    # Check JSON update
    json_updated = update_json_if_necessary()

    # Commit changes
    commit_changes(bug_id, new_version, strings_removed, json_updated)

    # Output final message
    print(f"✅ Changelog updated to version {new_version}")
    if strings_removed:
        print(f"✅ Removed 'moz:removedIn=\"{expired_string_version}\"' entries")
    else:
        print(f"ℹ️  No 'moz:removedIn=\"{expired_string_version}\"' entries found")

    if json_updated:
        print("✅ search_telemetry_v2.json was updated in Android Components")
    else:
        print("ℹ️  search_telemetry_v2.json was already up to date and was not modified")
    print(f"✅ Changes committed with Bug ID {bug_id}.")

    if remaining_use_message:
        print(
            "\n⚠️  Some of the strings that were removed are still used in the codebase. Please remove their uses and amend the commit."
        )
        print(remaining_use_message)

    print("\n\033[1mPlease make sure you complete the following steps:\033[0m")
    if remaining_use_message:
        print(
            "☐ Remove the remaining uses of the removed strings and amend the commit."
        )
    print("☐ Review the changes and make sure they are correct")
    print("☐ Run `moz-phab submit --no-wip`")
    print(
        "☐ Run `mach try --preset firefox-android` and add a comment with the try link on the patch"
    )


if __name__ == "__main__":
    main()
