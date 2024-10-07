#!/bin/bash
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This script installs Node v18 LTS for PDF.js
# This is different from the nodejs used in the toolchain, but hopefully that won't be an issue

wget -O node.xz --progress=dot:mega https://nodejs.org/dist/v18.20.4/node-v18.20.4-linux-x64.tar.xz
echo '592eb35c352c7c0c8c4b2ecf9c19d615e78de68c20b660eb74bd85f8c8395063' node.xz | sha256sum -c
tar -C /usr/local -xJ --strip-components 1 < node.xz
node -v  # verify
npm -v
