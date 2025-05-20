#!/usr/bin/env bash

set -v -e

test "$(whoami)" == 'root'

apt-get update
apt-get install \
    python-is-python3 \
    sudo \
    python3-yaml \
    python3-pip

pip install --break-system-packages --disable-pip-version-check --quiet --no-cache-dir orjson==3.10.15

apt-get remove --purge python3-pip
apt-get autoremove --purge
apt-get clean
apt-get autoclean
rm -rf /var/lib/apt/lists/
rm "$0"
