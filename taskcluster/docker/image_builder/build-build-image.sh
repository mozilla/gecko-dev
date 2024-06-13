#!/bin/sh

set -ex

TOOLCHAIN=1.76.0
TARGET=x86_64-unknown-linux-musl

# Install our Rust toolchain and the `musl` target.  We patch the
# command-line we pass to the installer so that it won't attempt to
# interact with the user or fool around with TTYs.  We also set the default
# `--target` to musl so that our users don't need to keep overriding it
# manually.
curl https://sh.rustup.rs -sSf | \
    sh -s -- -y \
        --profile minimal \
        --default-toolchain $TOOLCHAIN \
        --target $TARGET

# Set up our path with all our binary directories, including those for the
# musl-gcc toolchain and for our Rust toolchain.
export PATH="/home/rust/.cargo/bin:$PATH"

# --out-dir is not yet stable
export RUSTC_BOOTSTRAP=1
# Build our application.
cargo build --target $TARGET --out-dir=bin --release -Zunstable-options
