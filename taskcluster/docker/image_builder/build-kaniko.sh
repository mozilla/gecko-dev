#!/bin/sh

set -ex

git clone --no-checkout --depth=1 --branch=v1.0.0 https://github.com/GoogleContainerTools/kaniko .
git checkout 146ec6a9cd6f87b4a12e8119ded575d5edca35ac
make
