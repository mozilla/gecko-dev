/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

module.metadata = {
  "stability": "unstable"
};

// This module is manually loaded by bootstrap.js in a sandbox and immediatly
// put in module cache so that it is never loaded in any other way.

/* Workarounds to include dependencies in the manifest
require('chrome')                  // Otherwise CFX will complain about Components
require('toolkit/loader')          // Otherwise CFX will stip out loader.js
require('sdk/addon/runner')        // Otherwise CFX will stip out addon/runner.js
*/

const { classes: Cc, Constructor: CC, interfaces: Ci, utils: Cu } = Components;

// `loadSandbox` is exposed by bootstrap.js
const loaderURI = module.uri.replace("sdk/loader/cuddlefish.js",
                                     "toolkit/loader.js");
const xulappURI = module.uri.replace("loader/cuddlefish.js",
                                     "system/xul-app.jsm");
// We need to keep a reference to the sandbox in order to unload it in
// bootstrap.js

const loaderSandbox = loadSandbox(loaderURI);
const loaderModule = loaderSandbox.exports;

const { incompatibility } = Cu.import(xulappURI, {}).XulApp;

const { override, load } = loaderModule;

function CuddlefishLoader(options) {
  let { manifest } = options;

  options = override(options, {
    // Put `api-utils/loader` and `api-utils/cuddlefish` loaded as JSM to module
    // cache to avoid subsequent loads via `require`.
    modules: override({
      'toolkit/loader': loaderModule,
      'sdk/loader/cuddlefish': exports
    }, options.modules),
    resolve: function resolve(id, requirer) {
      let entry = requirer && requirer in manifest && manifest[requirer];
      let uri = null;

      // If manifest entry for this requirement is present we follow manifest.
      // Note: Standard library modules like 'panel' will be present in
      // manifest unless they were moved to platform.
      if (entry) {
        let requirement = entry.requirements[id];
        // If requirer entry is in manifest and it's requirement is not, than
        // it has no authority to load since linker was not able to find it.
        if (!requirement)
          throw Error('Module: ' + requirer + ' has no authority to load: '
                      + id, requirer);

        uri = requirement;
      } else {
        // If requirer is off manifest than it's a system module and we allow it
        // to go off manifest by resolving a relative path.
        uri = loaderModule.resolve(id, requirer);
      }
      return uri;
    },
    load: function(loader, module) {
      let result;
      let error;

      // In order to get the module's metadata, we need to load the module.
      // if an exception is raised here, it could be that is due to application
      // incompatibility. Therefore the exception is stored, and thrown again
      // only if the module seems be compatible with the application currently
      // running. Otherwise the incompatibility message takes the precedence.
      try {
        result = load(loader, module);
      }
      catch (e) {
        error = e;
      }

      error = incompatibility(module) || error;

      if (error)
        throw error;

      return result;
    }
  });

  let loader = loaderModule.Loader(options);
  // Hack to allow loading from `toolkit/loader`.
  loader.modules[loaderURI] = loaderSandbox;
  return loader;
}

exports = override(loaderModule, {
  Loader: CuddlefishLoader
});
