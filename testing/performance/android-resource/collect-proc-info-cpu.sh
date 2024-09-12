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

get_cpu_info(){
    # name: process name
    # res: rss
    # shr: shared memory
    # swap: swap I/O
    # cpu: processor being used
    # c: total %CPU used since start
    # time: CPU time consumed
    # %cpu: percentage of CPU time used
    adb shell ps -p "$1" -o name=,cpu=,c=,time=,%cpu= >> $2
}

collect_cpu_at() {
    for pid in $(get_process_ids $BROWSER_BINARY); do
        echo "Collecting cpu info at $1s for $pid"
        get_cpu_info "$pid" $TESTING_DIR/cpu_info_$2.txt &
    done
}

collect_resources_at() {
    sleep $1
    collect_cpu_at $1 $2
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
