# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import json
import logging
import sys

from intermittent_failures import IntermittentFailuresFetcher
from mach.decorators import Command, CommandArgument, SubCommand


@Command(
    "intermittents",
    category="testing",
    description="Analyze intermittent test failures",
)
def intermittents(command_context):
    """
    Utility to analyze intermittent test failures in Firefox.
    """
    # Print help text when no subcommand is provided
    print("usage: mach intermittents <subcommand> [options]")
    print()
    print("Analyze intermittent test failures in Firefox.")
    print()
    print("subcommands:")
    print("  list    List the most frequent intermittent test failures")
    print()
    print("Run 'mach intermittents <subcommand> --help' for more information.")
    sys.exit(0)


@SubCommand(
    "intermittents",
    "list",
    description="List the most frequent intermittent test failures",
)
@CommandArgument(
    "--days",
    type=int,
    default=7,
    help="Number of days to look back for failures (default: 7)",
)
@CommandArgument(
    "--threshold",
    type=int,
    default=30,
    help="Minimum number of failures to include (default: 30)",
)
@CommandArgument(
    "--branch",
    default="trunk",
    help="Branch to query (default: trunk)",
)
@CommandArgument(
    "--json",
    action="store_true",
    dest="json_output",
    help="Output results as JSON",
)
@CommandArgument(
    "--verbose",
    action="store_true",
    help="Show additional details for each failure",
)
@CommandArgument(
    "--all",
    action="store_true",
    help="Show all bugs (by default only single tracking bugs with test paths are shown)",
)
def list_intermittents(
    command_context,
    days=7,
    threshold=30,
    branch="trunk",
    json_output=False,
    verbose=False,
    all=False,
):
    """List the most frequent intermittent test failures"""

    # Logging setup
    if not json_output:
        command_context.log(
            logging.INFO,
            "intermittents",
            {},
            f"Fetching intermittent failures from the last {days} days with at least {threshold} occurrences...",
        )

    fetcher = IntermittentFailuresFetcher(
        days=days, threshold=threshold, verbose=verbose and not json_output
    )

    try:
        results = fetcher.get_failures(branch=branch)
    except Exception as e:
        command_context.log(
            logging.ERROR,
            "intermittents",
            {"error": str(e)},
            "Error fetching failures: {error}",
        )
        return 1

    if not all:
        results = [
            result
            for result in results
            if result.get("test_path") and "single tracking bug" in result["summary"]
        ]

    if not results:
        if not json_output:
            message = f"No bugs found with at least {threshold} failures in the last {days} days."
            if not all:
                message = f"No single tracking bugs with test paths found with at least {threshold} failures in the last {days} days. Use --all to see all bugs."
            command_context.log(
                logging.INFO,
                "intermittents",
                {},
                message,
            )
        else:
            print(json.dumps([]))
        return 0

    results.sort(key=lambda x: x["failure_count"], reverse=True)

    if json_output:
        print(json.dumps(results, indent=2))
    else:
        command_context.log(
            logging.INFO,
            "intermittents",
            {"count": len(results), "threshold": threshold},
            "Found {count} bugs with at least {threshold} failures:",
        )
        print()

        for i, result in enumerate(results, 1):
            print(f"{i}. Bug {result['bug_id']}: {result['failure_count']} failures")
            if result.get("test_path"):
                print(f"   Test: {result['test_path']}")
            print(f"   Summary: {result['summary']}")
            print(f"   Status: {result['status']}", end="")
            if result.get("resolution"):
                print(f" - {result['resolution']}")
            else:
                print()
            if result.get("creation_time"):
                created = result["creation_time"].split("T")[0]  # Just the date part
                print(f"   Created: {created}")
            if result.get("last_change_time"):
                updated = result["last_change_time"].split("T")[0]  # Just the date part
                print(f"   Last updated: {updated}")
            if result.get("comment_count") is not None:
                print(f"   Comments: {result['comment_count']}")
            print(
                f"   URL: https://bugzilla.mozilla.org/show_bug.cgi?id={result['bug_id']}"
            )
            print()

    return 0
