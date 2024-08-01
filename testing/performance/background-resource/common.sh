#!/bin/bash
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

print_info() {
    # Periodically output some text to prevent output timeouts
    while true; do
        echo "Collecting test resource usage..."
        sleep 5
    done
}

get_process_ids(){
    (adb shell pgrep -f "$1")
}
