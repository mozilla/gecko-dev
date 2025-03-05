# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os

import mozinfo
import pytest
from manifestparser import expression


def update_mozinfo():
    """Walk up directories to find mozinfo.json and update the info."""
    # check for fetched target.mozinfo.json first
    fetches = os.getenv("MOZ_FETCHES_DIR")
    if fetches:
        target_mozinfo = os.path.join(fetches, "target.mozinfo.json")
        if os.path.isfile(target_mozinfo):
            mozinfo.update(target_mozinfo)
            return

    # search for local mozinfo.json
    path = os.path.abspath(os.path.realpath(os.path.dirname(__file__)))
    dirs = set()
    while path != os.path.expanduser("~"):
        if path in dirs:
            break
        dirs.add(path)
        path = os.path.split(path)[0]
    mozinfo.find_and_update_from_json(*dirs)


def pytest_configure(config):
    """Register skip_mozinfo marker to avoid pytest warning."""
    config.addinivalue_line(
        "markers", "skip_mozinfo(expression): skip if mozinfo expression is matched"
    )


@pytest.fixture(autouse=True)
def skip_using_mozinfo(request):
    """Gives tests the ability to skip based on values from mozinfo.

    Example:
        @pytest.mark.skip_mozinfo("!e10s || os == 'linux'")
        def test_foo():
            pass
    """
    update_mozinfo()

    skip_mozinfo = request.node.get_closest_marker("skip_mozinfo")
    if skip_mozinfo:
        value = skip_mozinfo.args[0]
        if expression.parse(value, **mozinfo.info):
            pytest.skip("skipped due to mozinfo match: {}".format(value))
