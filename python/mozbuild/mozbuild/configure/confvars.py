# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import mozbuild.shellutil


class ConfVarsSyntaxError(SyntaxError):
    def __init__(self, msg, file, lineno, colnum, line):
        super().__init__(msg, (file, lineno, colnum, line))


def parse(path):
    with open(path) as confvars:
        keyvals = {}
        for lineno, rawline in enumerate(confvars, start=1):
            line = rawline.rstrip()
            # Empty line / comment.
            line_no_leading_blank = line.lstrip()
            if not line_no_leading_blank or line_no_leading_blank.startswith("#"):
                continue

            head, sym, tail = line.partition("=")
            if sym != "=" or "#" in head:
                raise ConfVarsSyntaxError(
                    "Expecting key=value format", path, lineno, 1, line
                )
            key = head.strip()

            # Verify there's no unexpected spaces.
            if key != head:
                colno = 1 + line.index(key)
                raise ConfVarsSyntaxError(
                    f"Expecting no spaces around '{key}'", path, lineno, colno, line
                )
            if tail.lstrip() != tail:
                colno = 1 + line.index(tail)
                raise ConfVarsSyntaxError(
                    f"Expecting no spaces between '=' and '{tail.lstrip()}'",
                    path,
                    lineno,
                    colno,
                    line,
                )

            # Verify we don't have duplicate keys.
            if key in keyvals:
                raise ConfVarsSyntaxError(
                    f"Invalid redefinition for '{key}'",
                    path,
                    lineno,
                    1 + line.index(key),
                    line,
                )

            # Parse value.
            try:
                values = mozbuild.shellutil.split(tail)
            except mozbuild.shellutil.MetaCharacterException as e:
                raise ConfVarsSyntaxError(
                    f"Unquoted, non-escaped special character '{e.char}'",
                    path,
                    lineno,
                    1 + line.index(e.char),
                    line,
                )
            except Exception as e:
                raise ConfVarsSyntaxError(
                    e.args[0].replace(" in command", ""),
                    path,
                    lineno,
                    1 + line.index("="),
                    line,
                )
            value = values[0] if values else ""

            # Finally, commit the key<> value pair \o/.
            keyvals[key] = value
        return keyvals
