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

cmd="cargo run -p glean --example ping-lifetime-flush -- $datapath"

# First run "crashes" -> no increment stored
$cmd accumulate_one_and_pretend_crash
count=$(ls -1q "$datapath/sent_pings" | wc -l)
if [[ "$count" -ne 0 ]]; then
  echo "test result: FAILED."
  exit 101
fi

# Second run increments, waits, increments -> increment flushed to disk.
# No ping is sent.
$cmd accumulate_ten_and_wait
count=$(ls -1q "$datapath/sent_pings" | wc -l)
if [[ "$count" -ne 0 ]]; then
  echo "test result: FAILED."
  exit 101
fi

# Third run sends the ping.
$cmd submit_ping
count=$(ls -1q "$datapath/sent_pings" | wc -l)
if [[ "$count" -ne 1 ]]; then
  echo "test result: FAILED."
  exit 101
fi

if ! grep -q '"test.metrics.sample_counter":20' "$datapath"/sent_pings/*; then
  echo "test result: FAILED."
  exit 101
fi

echo "test result: ok."
exit 0
