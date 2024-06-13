#!/bin/bash

set -ex

packages=(
    build-essential
    ca-certificates
    curl
    musl-dev
    musl-tools
)

apt-get update
apt-get install "${packages[@]}"
useradd rust --user-group --create-home --shell /bin/bash
install -d -m 755 -o rust -g rust /home/rust/src
