#!/bin/sh

set -ex

ARCH="$1"

git clone --no-checkout --depth=1 --branch=v1.23.0 https://github.com/GoogleContainerTools/kaniko .
git checkout 98df8ebfc7834a720c83b81bd0b1d54f4f480477
if [ "$ARCH" = arm64 ]; then
    make GOARCH=arm64
else
    make
fi
