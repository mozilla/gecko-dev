# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import re


def read_reserved_word_list(filename, *args):

    enable_decorators = False
    enable_explicit_resource_management = False

    for arg in args:
        if arg == "--enable-decorators":
            enable_decorators = True
        elif arg == "--enable-explicit-resource-management":
            enable_explicit_resource_management = True
        else:
            raise ValueError("Unknown argument: " + arg)

    macro_pat = re.compile(r"MACRO\(([^,]+), *[^,]+, *[^\)]+\)\s*\\?")

    reserved_word_list = []
    index = 0
    with open(filename) as f:
        for line in f:
            m = macro_pat.search(line)
            if m:
                reserved_word = m.group(1)
                if reserved_word == "accessor" and not enable_decorators:
                    continue
                if reserved_word == "using" and not enable_explicit_resource_management:
                    continue
                reserved_word_list.append((index, reserved_word))
                index += 1

    assert len(reserved_word_list) != 0

    return reserved_word_list
