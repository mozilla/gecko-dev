# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import sys
from os import path
from pathlib import Path

import mozunit

# Shenanigans to import the metrics index's list of metrics.yamls
FOG_ROOT_PATH = path.abspath(
    path.join(path.dirname(__file__), path.pardir, path.pardir)
)
sys.path.append(FOG_ROOT_PATH)
from metrics_index import pings_yamls

# Shenanigans to import run_glean_parser
sys.path.append(path.join(FOG_ROOT_PATH, "build_scripts", "glean_parser_ext"))
import run_glean_parser

# Shenanigans to import the in-tree glean_parser
GECKO_PATH = path.join(FOG_ROOT_PATH, path.pardir, path.pardir, path.pardir)
sys.path.append(path.join(GECKO_PATH, "third_party", "python", "glean_parser"))
from glean_parser import parser, util


def test_no_metadata_use_ohttp():
    """
    Of all the pings included in this build, none should use `metadata.use_ohttp`.
    If so, they must switch to `uploader_capabilities: ['ohttp']`

    (This also checks other lints, as a treat.)
    """
    with open("browser/config/version.txt") as version_file:
        app_version = version_file.read().strip()

    options = run_glean_parser.get_parser_options(app_version, False)
    paths = [Path(x) for x in pings_yamls]
    all_objs = parser.parse_objects(paths, options)
    assert not util.report_validation_errors(all_objs)

    pings = all_objs.value["pings"]
    for [ping_name, ping] in pings.items():
        assert (
            "use_ohttp" not in ping.metadata
        ), f"Ping {ping_name} uses `use_ohttp`. Switch to `uploader_capabilities`."


if __name__ == "__main__":
    mozunit.main()
