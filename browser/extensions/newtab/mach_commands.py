# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import json
import os
import re
import shutil
import subprocess
import tempfile
import time
from datetime import datetime, timedelta, timezone
from pathlib import Path

import requests
from colorama import Fore, Style
from mach.decorators import (
    Command,
    CommandArgument,
    SubCommand,
)

FIREFOX_L10N_REPO = "https://github.com/mozilla-l10n/firefox-l10n.git"
FLUENT_FILE = "newtab.ftl"
WEBEXT_LOCALES_PATH = Path("browser", "extensions", "newtab", "webext-glue", "locales")
LOCAL_EN_US_PATH = Path("browser", "locales", "en-US", "browser", "newtab", FLUENT_FILE)
COMPARE_TOOL_PATH = Path(
    "third_party", "python", "moz.l10n", "moz", "l10n", "bin", "compare.py"
)
REPORT_PATH = Path(WEBEXT_LOCALES_PATH, "locales-report.json")
REPORT_LEFT_JUSTIFY_CHARS = 15
FLUENT_FILE_ANCESTRY = Path("browser", "newtab")
SUPPORTED_LOCALES_PATH = Path(WEBEXT_LOCALES_PATH, "supported-locales.json")

# We query whattrainisitnow.com to get some key dates for both beta and
# release in order to compute whether or not strings have been available on
# the beta channel long enough to consider falling back (currently, that's
# 3 weeks of time on the beta channel).
BETA_SCHEDULE_QUERY = "https://whattrainisitnow.com/api/release/schedule/?version=beta"
RELEASE_SCHEDULE_QUERY = (
    "https://whattrainisitnow.com/api/release/schedule/?version=release"
)
BETA_FALLBACK_THRESHOLD = timedelta(weeks=3)


@Command(
    "newtab",
    category="misc",
    description="Run a command for the newtab built-in addon",
    virtualenv_name="newtab",
)
def newtab(command_context):
    """
    Desktop New Tab build and update utilities.
    """
    command_context._sub_mach(["help", "newtab"])
    return 1


def run_mach(command_context, cmd, **kwargs):
    return command_context._mach_context.commands.dispatch(
        cmd, command_context._mach_context, **kwargs
    )


@SubCommand(
    "newtab",
    "watch",
    description="Parses the current locales-report.json and produces something human readable.",
)
def watch(command_context):
    processes = []

    try:
        p1 = subprocess.Popen(
            ["./mach", "npm", "run", "watchmc", "--prefix=browser/extensions/newtab"]
        )
        p2 = subprocess.Popen(["./mach", "watch"])
        processes.extend([p1, p2])
        print("Watching subprocesses started. Press Ctrl-C to terminate them.")

        while True:
            time.sleep(1)

    except KeyboardInterrupt:
        print("\nSIGINT received. Terminating subprocesses...")
        for p in processes:
            p.terminate()
        for p in processes:
            p.wait()
        print("All watching subprocesses terminated.")

    # Rebundle to avoid having all of the sourcemaps stick around.
    run_mach(
        command_context,
        "npm",
        args=["run", "bundle", "--prefix=browser/extensions/newtab"],
    )


@SubCommand(
    "newtab",
    "update-locales",
    description="Update the locales snapshot.",
    virtualenv_name="newtab",
)
def update_locales(command_context):
    try:
        os.mkdir(WEBEXT_LOCALES_PATH)
    except FileExistsError:
        pass

    # Step 1: We download the latest reckoning of strings from firefox-l10n
    print("Cloning the latest HEAD of firefox-l10n repository")
    with tempfile.TemporaryDirectory() as clone_dir:
        subprocess.check_call(
            ["git", "clone", "--depth=1", FIREFOX_L10N_REPO, clone_dir]
        )
        # Step 2: Get some metadata about what we just pulled down -
        # specifically, the revision.
        revision = subprocess.check_output(
            ["git", "rev-parse", "HEAD"],
            cwd=str(clone_dir),
            universal_newlines=True,
        ).strip()

        # Step 3: Recursively find all files matching the filename for our
        # FLUENT_FILE, and copy them into WEBEXT_LOCALES_PATH/AB_CD/FLUENT_FILE
        root_dir = Path(clone_dir)
        fluent_file_matches = list(root_dir.rglob(FLUENT_FILE))
        for fluent_file_abs_path in fluent_file_matches:
            relative_path = fluent_file_abs_path.relative_to(root_dir)
            # The first element of the path is the locale code, which we want
            # to recreate under WEBEXT_LOCALES_PATH
            locale = relative_path.parts[0]
            destination_file = WEBEXT_LOCALES_PATH.joinpath(
                locale, FLUENT_FILE_ANCESTRY, FLUENT_FILE
            )
            destination_file.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(fluent_file_abs_path, destination_file)

        # Now clean up the temporary directory.
        shutil.rmtree(clone_dir)

    # Step 4: Now copy the local version of FLUENT_FILE in LOCAL_EN_US_PATH
    # into WEBEXT_LOCALES_PATH/en-US/FLUENT_FILE
    print(f"Cloning local en-US copy of {FLUENT_FILE}")
    dest_en_ftl_path = WEBEXT_LOCALES_PATH.joinpath(
        "en-US", FLUENT_FILE_ANCESTRY, FLUENT_FILE
    )
    dest_en_ftl_path.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(LOCAL_EN_US_PATH, dest_en_ftl_path)

    # Step 4.5: Now compute the commit dates of each of the strings inside of
    # LOCAL_EN_US_PATH.
    print("Computing local message commit dates…")
    message_dates = get_message_dates(LOCAL_EN_US_PATH)

    # Step 5: Now compare that en-US Fluent file with all of the ones we just
    # cloned and create a report with how many strings are still missing.
    print("Generating localization report…")

    source_ftl_path = WEBEXT_LOCALES_PATH.joinpath("en-US")
    paths = list(WEBEXT_LOCALES_PATH.rglob(FLUENT_FILE))

    # There are 2 parent folders of each FLUENT_FILE (see FLUENT_FILE_ANCESTRY),
    # and we want to get at the locale folder root for our comparison.
    ANCESTRY_LENGTH = 2

    # Get the full list of supported locales that we just pulled down
    supported_locales = sorted([path.parents[ANCESTRY_LENGTH].name for path in paths])
    path_strs = [path.parents[ANCESTRY_LENGTH].as_posix() for path in paths]

    # Verbosity on the compare.py tool appears to be a count value, which is
    # incremented for each -v flag. We want an elevated verbosity so that we
    # get back
    verbosity = ["-v", "-v"]
    # A bug in compare.py means that the source folder must be passed in as
    # an absolute path.
    source = ["--source=%s" % source_ftl_path.absolute().as_posix()]
    other_flags = ["--json"]

    # The moz.l10n compare tool is currently designed to be invoked from the
    # command line interface. We'll use subprocess to invoke it and capture
    # its output.
    python = command_context.virtualenv_manager.python_path

    def on_line(line):
        locales = json.loads(line)

        # The compare tool seems to produce non-deterministic ordering of
        # missing message IDs, which makes reasoning about changes to the
        # report JSON difficult. We sort each locales list of missing message
        # IDs alphabetically.
        REPORT_FILE_PATH = f"browser/newtab/{FLUENT_FILE}"
        for locale, locale_data in locales.items():
            missing = locale_data.get("missing", None)
            if isinstance(missing, dict):
                entries = missing.get(REPORT_FILE_PATH, None)
                if isinstance(entries, list):
                    entries.sort()

        report = {
            "locales": locales,
            "meta": {
                "repository": FIREFOX_L10N_REPO,
                "revision": revision,
                "updated": datetime.utcnow().isoformat(),
            },
            "message_dates": message_dates,
        }
        with open(REPORT_PATH, "w") as file:
            json.dump(report, file, indent=4)
        display_report(report)
        print("Wrote report to %s" % REPORT_PATH)

    command_context.run_process(
        [python, str(COMPARE_TOOL_PATH)] + other_flags + source + verbosity + path_strs,
        pass_thru=False,
        line_handler=on_line,
    )

    print("Writing supported locales to %s" % SUPPORTED_LOCALES_PATH)
    with open(SUPPORTED_LOCALES_PATH, "w") as file:
        json.dump(supported_locales, file, indent=4)

    print("Done")


@SubCommand(
    "newtab",
    "locales-report",
    description="Parses the current locales-report.json and produces something human readable.",
    virtualenv_name="newtab",
)
@CommandArgument(
    "--details", default=None, help="Which locale to pull up details about"
)
def locales_report(command_context, details):
    with open(REPORT_PATH) as file:
        report = json.load(file)
        display_report(report, details)


def get_message_dates(fluent_file_path):
    """Computes the landing dates of strings in fluent_file_path.

    This is returned as a dict of Fluent message names mapped
    to ISO-formatted dates for their landings.
    """
    result = subprocess.run(
        ["git", "blame", "--line-porcelain", fluent_file_path],
        stdout=subprocess.PIPE,
        text=True,
    )

    pattern = re.compile(r"^([a-z-]+[^\s]+) ")
    entries = {}
    entry = {}

    for line in result.stdout.splitlines():
        if line.startswith("\t"):
            code = line[1:]
            match = pattern.match(code)
            if match:
                key = match.group(1)
                timestamp = int(entry.get("committer-time", 0))
                commit_time = datetime.fromtimestamp(timestamp)
                # Only store the first time it was introduced (which blame gives us)
                entries[key] = commit_time.isoformat()
            entry = {}
        else:
            if " " in line:
                key, val = line.split(" ", 1)
                entry[key] = val

    return entries


def get_date_manually():
    """Requests a date from the user in yyyy/mm/dd format.

    This will loop until a valid date is computed. Returns a datetime.
    """
    while True:
        try:
            typed_chars = input("Enter date manually (yyyy/mm/dd): ")
            manual_date = datetime.strptime(typed_chars, "%Y/%m/%d")
            return manual_date
        except ValueError:
            print("Invalid date format. Please use yyyy/mm/dd.")


def display_report(report, details=None):
    """Displays a report about the current newtab localization state.

    This report is calculated using the REPORT_PATH file generated
    via the update-locales command, along with the merge-to-beta
    dates of the most recent beta and release versions of the browser,
    as well as the current date.

    Details about a particular locale can be requested via the details
    argument.
    """
    print("New Tab locales report")
    # We need some key dates to determine which strings are currently awaiting
    # translations on the beta channel, and which strings are just missing
    # (where missing means that they've been available for translation on the
    # beta channel for more than the BETA_FALLBACK_THRESHOLD, and are still
    # not available).
    #
    # We need to get the last merge date of the current beta, and the merge
    # date of the current release.
    try:
        response = requests.get(BETA_SCHEDULE_QUERY, timeout=10)
        response.raise_for_status()
        beta_merge_date = datetime.fromisoformat(response.json()["merge_day"])
    except (requests.RequestException, requests.HTTPError):
        print(f"Failed to compute last beta merge day for {FLUENT_FILE}.")
        beta_merge_date = get_date_manually()

    beta_merge_date = beta_merge_date.replace(tzinfo=timezone.utc)
    print(f"Beta date: {beta_merge_date}")

    # The release query needs to be different because the endpoint doesn't
    # actually tell us the merge-to-beta date for the version on the release
    # channel. We guesstimate it by getting at the build date for the first
    # beta of that version, and finding the last prior Monday.
    try:
        response = requests.get(RELEASE_SCHEDULE_QUERY, timeout=10)
        response.raise_for_status()
        release_merge_date = datetime.fromisoformat(response.json()["beta_1"])
        # weekday() defaults to Monday.
        release_merge_date = release_merge_date - timedelta(
            days=release_merge_date.weekday()
        )
    except (requests.RequestException, requests.HTTPError):
        print(
            f"Failed to compute the merge-to-beta day the current release for {FLUENT_FILE}."
        )
        release_merge_date = get_date_manually()

    release_merge_date = release_merge_date.replace(tzinfo=timezone.utc)
    print(f"Release merge-to-beta date: {release_merge_date}")

    # These two dates will be used later on when we start calculating which
    # untranslated strings should be considered "pending" (we're still waiting
    # for them to be on beta for at least 3 weeks), and which should be
    # considered "missing" (they've been on beta for more than 3 weeks and
    # still aren't translated).

    meta = report["meta"]
    message_date_strings = report["message_dates"]
    # Convert each message date into a datetime object
    message_dates = {
        key: datetime.fromisoformat(value).replace(tzinfo=timezone.utc)
        for key, value in message_date_strings.items()
    }

    print(f"Locales last updated: {meta['updated']}")
    print(f"From {meta['repository']} - revision: {meta['revision']}")
    print("------")
    if details:
        if details not in report["locales"]:
            print(f"Unknown locale '{details}'")
            return
        sorted_locales = [details]
    else:
        sorted_locales = sorted(report["locales"].keys(), key=lambda x: x.lower())
    for locale in sorted_locales:
        print(Style.RESET_ALL, end="")
        # import pdb;pdb.set_trace()
        if report["locales"][locale]["missing"]:
            missing_translations = report["locales"][locale]["missing"][
                str(FLUENT_FILE_ANCESTRY.joinpath(FLUENT_FILE))
            ]
            # For each missing string, see if any of them have been in the
            # en-US locale for less than BETA_FALLBACK_THRESHOLD. If so, these
            # strings haven't been on the beta channel long enough to consider
            # falling back. We're still awaiting translations on them.
            total_pending_translations = 0
            total_missing_translations = 0
            for missing_translation in missing_translations:
                message_id = missing_translation.split(".")[0]
                if message_dates[message_id] < release_merge_date:
                    # Anything landed prior to the most recent release
                    # merge-to-beta date has clearly been around long enough
                    # to be translated. This is a "missing" string.
                    total_missing_translations = total_missing_translations + 1
                    if details:
                        print(
                            Fore.YELLOW
                            + f"Missing: {message_dates[message_id]}: {message_id}"
                        )
                elif message_dates[message_id] < beta_merge_date and (
                    datetime.now(timezone.utc) - beta_merge_date
                    > BETA_FALLBACK_THRESHOLD
                ):
                    # Anything that landed after the release merge to beta, but
                    # before the most recent merge to beta, we'll consider its
                    # age to be the beta_merge_date, rather than the
                    # message_dates entry. If we compare the beta_merge_date
                    # with the current time and see that BETA_FALLBACK_THRESHOLD
                    # has passed, then the string is missing.
                    total_missing_translations = total_missing_translations + 1
                    if details:
                        print(
                            Fore.YELLOW
                            + f"Missing: {message_dates[message_id]}: {message_id}"
                        )
                else:
                    # Otherwise, this string has not been on beta long enough
                    # to have been translated. This is a pending string.
                    total_pending_translations = total_pending_translations + 1
                    if details:
                        print(
                            Fore.RED
                            + f"Pending: {message_dates[message_id]}: {message_id}"
                        )

            if total_pending_translations > 10:
                color = Fore.RED
            else:
                color = Fore.YELLOW
            print(
                color
                + f"{locale.ljust(REPORT_LEFT_JUSTIFY_CHARS)}{total_pending_translations} pending translations, {total_missing_translations} missing translations"
            )

        else:
            print(
                Fore.GREEN
                + f"{locale.ljust(REPORT_LEFT_JUSTIFY_CHARS)}0 missing translations"
            )
    print(Style.RESET_ALL, end="")
