/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Bug 1799977: Using workaround to test telemetry in plain mochitests.
// Also see Bug 1940064.

var GleanTest;
(function () {
  async function testResetFOG() {
    return SpecialPowers.spawnChrome([], async () => {
      await Services.fog.testFlushAllChildren();
      Services.fog.testResetFOG();
    });
  }

  async function flush() {
    return SpecialPowers.spawnChrome([], async () => {
      await Services.fog.testFlushAllChildren();
    });
  }

  async function testGetValue(chain) {
    return SpecialPowers.spawnChrome([chain], async chain => {
      await Services.fog.testFlushAllChildren();
      const window = this.browsingContext.topChromeWindow;
      let glean = window.Glean;
      while (chain.length) {
        glean = glean[chain.shift()];
      }
      return glean.testGetValue();
    });
  }

  function recurse(chain = []) {
    return new Proxy(
      {},
      {
        get(_, prop) {
          if (chain.length === 0) {
            if (prop === "testResetFOG") {
              return testResetFOG;
            } else if (prop === "flush") {
              return flush;
            }
          }
          if (chain.length >= 2 && prop === "testGetValue") {
            return () => testGetValue(chain);
          }
          return recurse(chain.concat(prop));
        },
      }
    );
  }

  window.GleanTest = recurse();
})();
