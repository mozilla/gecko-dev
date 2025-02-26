#!/usr/bin/env bash

source $(dirname "$0")/tools.sh

# Fetch artifact if needed.
fetch_dist

export DIST=${PWD}/dist

# tests write to the source dir (and its parent), so move the source tree to
# our workspace from the (cached) checkout dir
cp -a "${VCS_PATH}/nss" .

# Run tests.
cd nss/tests && ./all.sh
