/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Took from dom/notification/test/mochitest/GleanTest.js, modified a bit because we don't use cateogry label

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
    get(gleanTestObj, catProp) {
      if (catProp in gleanTestObj) {
        return gleanTestObj[catProp];
      }

      return new Proxy(
        {},
        {
          get(categoryObj, metProp) {
            return {
              async testGetValue() {
                return SpecialPowers.spawnChrome(
                  [catProp, metProp],
                  async (categoryName, metricName) => {
                    await Services.fog.testFlushAllChildren();
                    const window = this.browsingContext.topChromeWindow;
                    return window.Glean[categoryName][
                      metricName
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
