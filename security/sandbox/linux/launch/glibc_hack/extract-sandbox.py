#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

import re

with open("sandbox/linux/services/namespace_sandbox.cc") as fd:
    content = fd.read()

match = re.search(
    r"#if defined\(LIBC_GLIBC\).*?#endif  // defined\(LIBC_GLIBC\)", content, re.DOTALL
)
if not match:
    raise ValueError("Invalid patch extraction pattern")

patch = match.group()

with open("namespace_sandbox.inc", "w") as fd:
    fd.write(patch)
