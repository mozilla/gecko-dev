#!/bin/bash -vex

set -x -e

echo "running as" $(id)

set -v

cd $GECKO_PATH

export PATH=$PATH:$MOZ_FETCHES_DIR/node/bin

./mach configure --disable-compile-environment
./mach npm ci --prefix tools/lint/eslint/eslint-plugin-mozilla

# We have tools/lint/eslint/eslint-plugin-mozilla/{node_modules,...} and want
# eslint-plugin-mozilla/{node_modules}.
mkdir -p /builds/worker/artifacts
cd tools/lint/eslint/
tar caf /builds/worker/artifacts/eslint-plugin-mozilla.tar.zst eslint-plugin-mozilla
