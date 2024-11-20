#!/bin/bash
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

set -euxo pipefail

# Variables
WORKSPACE="$HOME/workspace"
DEPOT_TOOLS_DIR="${DEPOT_TOOLS_DIR:-$HOME/fetches/depot_tools.git}"
CHROMIUM_DIR="$WORKSPACE/chromium"
BUILD_OUTPUT_DIR="$CHROMIUM_DIR/src/out/Default"
UPLOAD_DIR="${UPLOAD_DIR:-$WORKSPACE/upload}"

# Target chromium revision
CHROMIUM_REVISION="${CHROMIUM_REVISION:-main}"

# Note that tar will choose type of compression based on provided filename (ie: xz/zst/gz)
TOOLCHAIN_ARTIFACT_PATH="${TOOLCHAIN_ARTIFACT:-public/build/zucchini.tar.xz}"
ARTIFACT_FILENAME=$(basename "$TOOLCHAIN_ARTIFACT_PATH")

# Disable depot_tools auto-update
export DEPOT_TOOLS_UPDATE=0

export PATH="$PATH:$DEPOT_TOOLS_DIR"

# Log the current revision of depot_tools for easier tracking
DEPOT_TOOLS_REV=$(git -C "$DEPOT_TOOLS_DIR" rev-parse HEAD)
echo "Current depot_tools revision: $DEPOT_TOOLS_REV"

# Set XDG_CONFIG_HOME to avoid write permission issues
# By default the user in the worker can't write to ~/.config
# Forces depot_tools to write config to a different folder
# https://chromium.googlesource.com/chromium/tools/depot_tools/+/refs/heads/main/utils.py#39
export XDG_CONFIG_HOME="$WORKSPACE/.config"

mkdir -p "$CHROMIUM_DIR/src"

# The steps below simulate `fetch --nohooks --no-history chromium` but on a pinned revision

# Setup gclient
cd "$CHROMIUM_DIR"
gclient config --spec 'solutions = [
  {
    "name": "src",
    "url": "https://chromium.googlesource.com/chromium/src.git",
    "managed": False,
    "custom_deps": {},
    "custom_vars": {},
  },
]'

# Fetch Chromium source
cd "$CHROMIUM_DIR/src"
git init
git remote add origin https://chromium.googlesource.com/chromium/src
git fetch --depth 1 origin $CHROMIUM_REVISION
git checkout FETCH_HEAD
git config diff.ignoreSubmodules dirty


# Run chromium hooks
gclient sync --nohooks --no-history
gclient runhooks

# Create build output directory and set GN args
mkdir -p "$BUILD_OUTPUT_DIR"
cat > "$BUILD_OUTPUT_DIR/args.gn" <<EOF
enable_nacl = false
symbol_level = 0
target_cpu = "x64"
enable_remoting = false
is_debug = false
EOF

# Generate Ninja build files and build Zucchini
gn gen "$BUILD_OUTPUT_DIR"
autoninja -C "$BUILD_OUTPUT_DIR" zucchini

# Package the Zucchini binary
cd "$BUILD_OUTPUT_DIR"
tar --create --auto-compress --file "$ARTIFACT_FILENAME" zucchini

# Move the artifact to the upload directory
mkdir -p "$UPLOAD_DIR"
mv "$ARTIFACT_FILENAME" "$UPLOAD_DIR/$ARTIFACT_FILENAME"

echo "Build and packaging completed successfully."
