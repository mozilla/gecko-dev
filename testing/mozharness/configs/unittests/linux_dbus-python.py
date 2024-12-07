# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os

#####
config = {
    ###
    "virtualenv_modules": [
        "dbus-python>=1.2.18,<=1.3.2",
        "python-dbusmock==0.32.2",
    ],
    "find_links": [
        "https://pypi.pub.build.mozilla.org/pub/",
        os.path.abspath(os.environ.get("MOZ_FETCHES_DIR")),
    ],
}
