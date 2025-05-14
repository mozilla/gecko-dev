#!/usr/bin/env bash

source $(dirname "$0")/tools.sh

# Fetch Cryptofuzz artifact.
if [ "$TASKCLUSTER_ROOT_URL" = "https://taskcluster.net" ] || [ -z "$TASKCLUSTER_ROOT_URL" ]; then
    url=https://queue.taskcluster.net/v1/task/$TC_PARENT_TASK_ID/artifacts/public/cryptofuzz.tar.bz2
else
    url=$TASKCLUSTER_ROOT_URL/api/queue/v1/task/$TC_PARENT_TASK_ID/artifacts/public/cryptofuzz.tar.bz2
fi

if [ ! -d "cryptofuzz" ]; then
    curl --retry 3 -Lo cryptofuzz.tar.bz2 $url
    tar xvjf cryptofuzz.tar.bz2
fi

# Clone corpus.
mkdir -p nss/fuzz/corpus/cryptofuzz

pushd nss/fuzz/corpus/cryptofuzz
curl -O "https://storage.googleapis.com/cryptofuzz-backup.clusterfuzz-external.appspot.com/corpus/libFuzzer/cryptofuzz_cryptofuzz-nss/public.zip"
unzip public.zip
rm -f public.zip
popd

# Generate dictionary
./cryptofuzz/generate_dict

# Run Cryptofuzz.
# Decrease the default ASAN quarantine size of 256 MB as we tend to run
# out of memory on 32-bit.
ASAN_OPTIONS="quarantine_size_mb=64" ./cryptofuzz/cryptofuzz -dict="cryptofuzz-dict.txt" --force-module=nss "nss/fuzz/corpus/cryptofuzz" "$@"
