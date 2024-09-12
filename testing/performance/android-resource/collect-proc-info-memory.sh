#!/bin/bash
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
source $1/common.sh

TEST_TIME=$2

# waiting for test start signal
while true; do
    if [ -f $TESTING_DIR/test_start.signal ]; then
        break
    fi
done

collect_mem_at() {
    echo "Collecting mem info at $1s"
    TRIES=0
    while
        OUTPUT=$(adb shell dumpsys meminfo)
        TRIES=$(($TRIES+1))
        [ -z "$OUTPUT" -a $TRIES -lt 5 ]
    do true; done
    echo "Finished collecting mem info for $1s"
    echo "$OUTPUT" > $TESTING_DIR/mem_info_$2.txt
}

collect_resources_at() {
    sleep $1
    collect_mem_at $1 $2
}

collect_resources_info() {
    collect_resources_at 0 "01" & # at start
    collect_resources_at $(($TEST_TIME/10)) "02" & # at 10%
    collect_resources_at $(($TEST_TIME/2)) "03" & # at 50%
    collect_resources_at $TEST_TIME "04" # at test end
}

print_info "$1" & print_info_pid=$!
collect_resources_info
kill "$print_info_pid"
