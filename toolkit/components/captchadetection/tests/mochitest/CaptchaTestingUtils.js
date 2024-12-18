/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const CaptchaTestingUtils = {
  waitForMessage(shouldResolveOrString) {
    return new Promise(resolve => {
      window.addEventListener("message", function (event) {
        if (
          (typeof shouldResolveOrString === "string" &&
            event.data === shouldResolveOrString) ||
          (typeof shouldResolveOrString === "function" &&
            shouldResolveOrString(event))
        ) {
          resolve();
        }
      });
    });
  },
  waitForMetricSet() {
    return this.waitForMessage("Testing:MetricIsSet");
  },
  async createIframeAndWaitForMessage(
    iframeSrc,
    appendTo,
    shouldResolveOrString
  ) {
    const iframe = document.createElement("iframe");
    const message = this.waitForMessage(shouldResolveOrString);
    iframe.src = iframeSrc;
    appendTo.appendChild(iframe);
    await message;
    return iframe;
  },
  async clearPrefs() {
    await SpecialPowers.clearUserPref("captchadetection.hasUnsubmittedData");
    await SpecialPowers.clearUserPref("captchadetection.lastSubmission");
  },
};
