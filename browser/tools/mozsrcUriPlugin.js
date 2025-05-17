/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// This plugin supports finding files with particular resource:// URIs
// and translating the uri into a relative filesytem path where the file may be
// found when running within the Karma / Mocha test framework.

/* eslint-env node */
const path = require("path");

module.exports = {
  MozSrcUriPlugin: class MozSrcUriPlugin {
    /**
     * The base directory of the repository.
     */
    #baseDir;

    /**
     * @param {object} options
     *  Object passed during the instantiation of MozSrcUriPlugin
     * @param {string} options.baseDir
     *   The base directory of the repository.
     */
    constructor({ baseDir }) {
      this.#baseDir = baseDir;
    }

    apply(compiler) {
      compiler.hooks.compilation.tap(
        "MozSrcUriPlugin",
        (compilation, { normalModuleFactory }) => {
          normalModuleFactory.hooks.resolveForScheme
            .for("moz-src")
            .tap("MozSrcUriPlugin", resourceData => {
              const url = new URL(resourceData.resource);

              // path.join() is necessary to normalize the path on Windows.
              // Without it, the path may contain backslashes, resulting in
              // different build output on Windows than on Unix systems.
              const pathname = path.join(this.#baseDir, url.pathname);
              resourceData.path = pathname;
              resourceData.query = url.search;
              resourceData.fragment = url.hash;
              resourceData.resource = pathname + url.search + url.hash;
              return true;
            });
        }
      );
    }
  },
};
