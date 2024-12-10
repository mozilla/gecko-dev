#!/usr/bin/env python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/. */

from os.path import join, dirname
import re
from urllib.request import urlopen


def main(filename):
    names = [
        re.search('>([^>]+)(</dfn>|<a class="self-link")', line.decode()).group(1)
        for line in urlopen("https://drafts.csswg.org/css-counter-styles/")
        if b'data-dfn-for="<counter-style-name>"' in line
        or b'data-dfn-for="<counter-style>"' in line
    ]
    with open(filename, "w") as f:
        f.write(
            """\
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

predefined! {
"""
        )
        for name in names:
            f.write('    "%s",\n' % name)
        f.write("}\n")


if __name__ == "__main__":
    main(join(dirname(__file__), "predefined.rs"))
