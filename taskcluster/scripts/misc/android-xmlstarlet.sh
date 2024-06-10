#!/bin/bash -vex

set -x -e -v

ar vx $MOZ_FETCHES_DIR/xmlstarlet.deb
tar xvf data.tar.xz

mkdir -p $UPLOAD_DIR
tar -cavf $UPLOAD_DIR/android-xmlstarlet.tar.zst -C ./usr/bin/ xmlstarlet
