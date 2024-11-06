#!/bin/sh
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This script builds the official interpreter for the python language,
# while also packing in a few default extra packages.

set -e
set -x

UPLOAD_DIR=$(pwd)/artifacts
mkdir -p "${UPLOAD_DIR}"

ls -hal "${MOZ_FETCHES_DIR}"
WHEEL_DIR=$(find "$MOZ_FETCHES_DIR/" -maxdepth 1 -mindepth 1 -type d -not -name "python")

export PATH="$MOZ_FETCHES_DIR/python/bin":/builds/worker/.local/bin:$PATH

python3 -m pip install --upgrade pip==23.0
python3 -m pip install -r "${GECKO_PATH}/build/wheel_requirements.txt"

cd "$WHEEL_DIR"
python3 setup.py bdist_wheel
whl=$(ls dist/*.whl)

cp "$whl" "$UPLOAD_DIR/"
