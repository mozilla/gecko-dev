#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


# Usage: check_source_count.py SEARCH_TERM COUNT ERROR_LOCATION REPLACEMENT [FILES...]
#   Checks that FILES contains exactly COUNT matches of SEARCH_TERM. If it does
#   not, an error message is printed, quoting ERROR_LOCATION, which should
#   probably be the filename and line number of the erroneous call to
#   check_source_count.py.
import re
import sys

search_string = sys.argv[1]
expected_count = int(sys.argv[2])
error_location = sys.argv[3]
replacement = sys.argv[4]
files = sys.argv[5:]

details = {}

count = 0
for f in files:
    text = file(f).read()
    match = re.findall(search_string, text)
    if match:
        num = len(match)
        count += num
        details[f] = num

if count == expected_count:
    print(f"TEST-PASS | check_source_count.py {search_string} | {expected_count}")

else:
    print(
        f"TEST-UNEXPECTED-FAIL | check_source_count.py {search_string} | ",
        end="",
    )
    if count < expected_count:
        print(
            f"There are fewer occurrences of /{search_string}/ than expected. "
            "This may mean that you have removed some, but forgotten to "
            f"account for it {error_location}."
        )
    else:
        print(
            f"There are more occurrences of /{search_string}/ than expected. We're trying "
            f"to prevent an increase in the number of {search_string}'s, using {replacement} if "
            "possible. If it is unavoidable, you should update the expected "
            f"count {error_location}."
        )

    print(f"Expected: {expected_count}; found: {count}")
    for k in sorted(details):
        print(f"Found {details[k]} occurences in {k}")
    sys.exit(-1)
