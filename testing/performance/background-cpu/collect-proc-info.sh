#!/bin/bash
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# waiting for test start signal
while true; do
    if [ -f $TESTING_DIR/test_start.signal ]; then
        break
    fi
done

print_info() {
    while true; do
        echo "Collecting info for $1..."
        sleep 5
    done
}

collect_proc_info(){
    print_info "$1" & print_info_pid=$!

    while true; do
        get_proc_info "$1"

        if [ -f $TESTING_DIR/test_end.signal ]; then
            sleep 1
            break
        fi
    done

    kill "$print_info_pid"
}

get_proc_info(){
    # name: process name
    # res: rss
    # shr: shared memory
    # swap: swap I/O
    # cpu: processor being used
    # c: total %CPU used since start
    # time: CPU time consumed
    # %cpu: percentage of CPU time used
    OUTPUT=$(adb shell ps -p "$1" -o name=,res=,shr=,swap=,cpu=,c=,time=,%cpu=)
    if [[ $OUTPUT ]]; then
        echo $OUTPUT >> $TESTING_DIR/tmp.txt
    fi
}

get_process_ids(){
    (adb shell pgrep -f "$1")
}

# note that the pids are collected only once in this test case. When a new process is created, this will not include the new pids
# such as when a new tab is opened. You would need to gather the pids in a cumulative way -infinitely- when working with multiple tabs.
pids=$(get_process_ids $BROWSER_BINARY)

for pid in $pids; do
    collect_proc_info "$pid" & # collect data for each process (main + child procs)
done