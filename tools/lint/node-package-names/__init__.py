# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

import json
import os

from mozlint import result
from mozlint.pathutils import expand_exclusions


def lint(paths, config, fix=None, **lintargs):
    results = []
    files = list(expand_exclusions(paths, config, lintargs["root"]))

    for file_path in files:
        if os.path.basename(file_path) != "package.json":
            continue

        try:
            with open(file_path, encoding="utf-8") as f:
                contents = json.load(f)
                if "name" in contents:
                    res = {
                        "path": file_path,
                        "message": "package.json should not supply a name unless it is published.",  # noqa
                        "level": "error",
                    }
                    results.append(result.from_config(config, **res))
        except json.decoder.JSONDecodeError:
            res = {
                "path": file_path,
                "message": "Could not load file, should it be fixed or added to an exclusion list?",
                "level": "error",
            }
            results.append(result.from_config(config, **res))

    return {"results": results, "fixed": 0}
