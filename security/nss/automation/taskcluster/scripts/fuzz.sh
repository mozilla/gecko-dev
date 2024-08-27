#!/usr/bin/env bash

source $(dirname "$0")/tools.sh

target="$1"
corpus="$2"
shift 2

# Fetch artifact if needed.
fetch_dist

# Create and change to corpus directory.
mkdir -p "nss/fuzz/corpus/$corpus"
cd "nss/fuzz/corpus/$corpus"

# Fetch and unzip the public OSS-Fuzz corpus.
curl -O "https://storage.googleapis.com/nss-backup.clusterfuzz-external.appspot.com/corpus/libFuzzer/nss_$corpus/public.zip"
unzip public.zip
rm public.zip

# Change back to previous working directory.
cd $OLDPWD

# Fetch objdir name.
objdir=$(cat dist/latest)

# Run nssfuzz.
dist/"$objdir"/bin/nssfuzz-"$target" "nss/fuzz/corpus/$corpus" "$@"
