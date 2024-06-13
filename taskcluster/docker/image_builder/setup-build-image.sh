#!/bin/bash

set -ex

ARCH=$1

packages=(
    build-essential
    ca-certificates
    curl
    musl-dev
    musl-tools
)

if [ "$ARCH" = arm64 ]; then
    dpkg --add-architecture arm64
    packages+=(
        gcc-aarch64-linux-gnu
        libc6-dev:arm64
    )
fi

apt-get update
apt-get install "${packages[@]}"
useradd rust --user-group --create-home --shell /bin/bash
install -d -m 755 -o rust -g rust /home/rust/src
