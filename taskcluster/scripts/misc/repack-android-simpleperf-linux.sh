#!/bin/bash
set -x -e -v

# This script is for fetching and repacking the Android Simpleperf NDK (for
# Linux), the tool required to profile Android applications.

mkdir -p $UPLOAD_DIR

# Populate /builds/worker/.mozbuild/android-ndk-$VER.
cd $GECKO_PATH
./mach python python/mozboot/mozboot/android.py --ndk-only --no-interactive

mv $HOME/.mozbuild/android-ndk-*/simpleperf $HOME/.mozbuild/android-simpleperf
tar cavf $UPLOAD_DIR/android-simpleperf.tar.zst -C /builds/worker/.mozbuild android-simpleperf

ls -al $UPLOAD_DIR
