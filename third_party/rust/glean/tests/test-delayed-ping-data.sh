#!/bin/bash

# Test harness for testing the RLB processes from the outside.
#
# Some behavior can only be observed when properly exiting the process running Glean,
# e.g. when an uploader runs in another thread.
# On exit the threads will be killed, regardless of their state.

# Remove the temporary data path on all exit conditions
cleanup() {
  if [ -n "$datapath" ]; then
    rm -r "$datapath"
  fi
}
trap cleanup INT ABRT TERM EXIT

tmp="${TMPDIR:-/tmp}"
datapath=$(mktemp -d "${tmp}/glean_ping_lifetime_flush.XXXX")

cmd="cargo run -p glean --example delayed-ping-data -- $datapath"

# First run "crashes" -> no increment stored
$cmd accumulate_one_and_pretend_crash
count=$(ls -1q "$datapath/sent_pings" | wc -l)
if [[ "$count" -ne 0 ]]; then
  echo "test result: FAILED."
  exit 101
fi

# Second run increments and orderly shuts down -> increment flushed to disk.
# No ping is sent.
$cmd accumulate_ten_and_orderly_shutdown
count=$(ls -1q "$datapath/sent_pings" | wc -l)
if [[ "$count" -ne 0 ]]; then
  echo "1/3 test result: FAILED. Expected 0, got $count pings"
  exit 101
fi

# Third run sends the ping.
$cmd submit_ping
count=$(ls -1q "$datapath/sent_pings" | wc -l)
if [[ "$count" -ne 1 ]]; then
  echo "2/3 test result: FAILED. Expect 1, got $count pings"
  exit 101
fi

if ! grep -q '"test.metrics.sample_counter":10' "$datapath"/sent_pings/*; then
  echo "3/3 test result: FAILED. Inaccurate number of sample counts."
  exit 101
fi

echo "test result: ok."
exit 0
