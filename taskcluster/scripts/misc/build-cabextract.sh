#!/bin/bash
set -x -e -v

# This script is for building cabextract for Linux.
PROJECT="cabextract"

pushd "${MOZ_FETCHES_DIR}/${PROJECT}"
./configure
make
popd

mkdir "${PROJECT}"
mv "${MOZ_FETCHES_DIR}/${PROJECT}/cabextract" "${PROJECT}/cabextract"
tar -acf "${PROJECT}.tar.zst" "${PROJECT}"

mkdir -p "$UPLOAD_DIR"
mv "${PROJECT}.tar.zst" "$UPLOAD_DIR"
