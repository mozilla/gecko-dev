#!/bin/bash -vex

set -x -e

echo "running as" $(id)

set -v

cd $GECKO_PATH

export PATH=$PATH:$MOZ_FETCHES_DIR/node/bin

rm -rf $2/node_modules
npm ci --prefix $2

# We have $2/{node_modules,...} and want $1/{node_modules}.
mkdir -p /builds/worker/artifacts
cd $2
cd ..
tar caf /builds/worker/artifacts/$1-node-modules.tar.zst $1/node_modules
