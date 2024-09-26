#!/bin/bash -vex

set -x -e

echo "running as" $(id)

set -v

cd $GECKO_PATH

export PATH=$PATH:$MOZ_FETCHES_DIR/node/bin

./mach eslint --setup

# Remove symlinks that we don't want to package.
rm node_modules/eslint-plugin-mozilla
rm node_modules/eslint-plugin-spidermonkey-js

mkdir -p /builds/worker/artifacts
tar caf /builds/worker/artifacts/node-modules.tar.zst node_modules
