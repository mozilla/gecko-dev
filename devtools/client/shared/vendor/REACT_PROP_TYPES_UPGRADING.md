[//]: # (
  This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
)

# Upgrading react-prop-types

## Getting the Source

```bash
git clone git@github.com:facebook/prop-types.git --depth 1 --single-branch -b v15.6.0 # or the version you are targetting (and update the version here)
cd prop-types
```

## Apply a patch to prop-types

The following patch will address the lack of process symbol in the ES Module scope:
```diff
diff --git a/checkPropTypes.js b/checkPropTypes.js
index 481f2cf..fb722f4 100644
--- a/checkPropTypes.js
+++ b/checkPropTypes.js
@@ -7,6 +7,8 @@

 'use strict';

+const process = {env: {NODE_ENV: "production"}};
+
 var printWarning = function() {};

 if (process.env.NODE_ENV !== 'production') {
diff --git a/factoryWithTypeCheckers.js b/factoryWithTypeCheckers.js
index a88068e..5be5931 100644
--- a/factoryWithTypeCheckers.js
+++ b/factoryWithTypeCheckers.js
@@ -7,6 +7,8 @@

 'use strict';

+const process = {env: {NODE_ENV: "production"}};
+
 var ReactIs = require('react-is');
 var assign = require('object-assign');

diff --git a/index.js b/index.js
index e9ef51d..8c4778a 100644
--- a/index.js
+++ b/index.js
@@ -5,6 +5,8 @@
  * LICENSE file in the root directory of this source tree.
  */

+const process = {env: {NODE_ENV: "production"}};
+
 if (process.env.NODE_ENV !== 'production') {
   var ReactIs = require('react-is');

```

## Building

Create `rollup.config.mjs` file into the prop-types folder with the following content:
```
import nodeResolve from "@rollup/plugin-node-resolve";
import commonjs from 'rollup-plugin-commonjs';
import injectProcessEnv from 'rollup-plugin-inject-process-env';

export default function (commandLineArgs) {
  const plugins = [nodeResolve(), commonjs(), injectProcessEnv({
    NODE_ENV: process.env.NODE_ENV,
  })];

  return {
    input: "index.js",
    output: {
      file: "output.mjs",
      format: "esm",
    },
    plugins,
  };
}
```

And then run the following
```bash
yarn add rollup @rollup/plugin-node-resolve rollup-plugin-commonjs rollup-plugin-inject-process-env

NODE_ENV=production yarn rollup -c
cp output.mjs ../react-prop-types.mjs

NODE_ENV=development yarn rollup -c
cp output.mjs ../react-prop-types-dev.mjs
```
