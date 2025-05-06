# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

config = {
    "stage_platform": "android-x86_64-lite",
    "mozconfig_platform": "android-x86_64",
    "extra_mozconfig_content": ["ac_add_options --enable-geckoview-lite"],
}
