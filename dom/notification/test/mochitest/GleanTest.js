/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Bug 1799977: Using workaround to test telemetry in plain mochitests
const GleanTest = new Proxy(
  {
    async testResetFOG() {
      return SpecialPowers.spawnChrome([], async () => {
        await Services.fog.testFlushAllChildren();
        Services.fog.testResetFOG();
      });
    },
    async flush() {
      return SpecialPowers.spawnChrome([], async () => {
        await Services.fog.testFlushAllChildren();
      });
    },
  },
  {
    get(gleanTestObj, gleanTestProp) {
      if (gleanTestProp in gleanTestObj) {
        return gleanTestObj[gleanTestProp];
      }

      return new Proxy(
        {},
        {
          get(categoryObj, categoryProp) {
            return new Proxy(
              {},
              {
                get(metricObj, metricProp) {
                  return {
                    async testGetValue() {
                      return SpecialPowers.spawnChrome(
                        [gleanTestProp, categoryProp, metricProp],
                        async (categoryName, metricName, label) => {
                          await Services.fog.testFlushAllChildren();
                          const window = this.browsingContext.topChromeWindow;
                          return window.Glean[categoryName][metricName][
                            label
                          ].testGetValue();
                        }
                      );
                    },
                  };
                },
              }
            );
          },
        }
      );
    },
  }
);
