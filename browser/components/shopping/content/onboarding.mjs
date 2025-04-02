/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const BUNDLE_SRC =
  "chrome://browser/content/aboutwelcome/aboutwelcome.bundle.js";

class Onboarding {
  constructor({ win } = {}) {
    this.doc = win.document;
    win.addEventListener("RenderWelcome", () => this._addScriptsAndRender());
  }

  async _addScriptsAndRender() {
    // Load the bundle to render the content as configured.
    this.doc.querySelector(`[src="${BUNDLE_SRC}"]`)?.remove();
    let bundleScript = this.doc.createElement("script");
    bundleScript.src = BUNDLE_SRC;
    this.doc.head.appendChild(bundleScript);
  }

  static getOnboarding() {
    if (!this.onboarding) {
      this.onboarding = new Onboarding({ win: window });
    }
    return this.onboarding;
  }
}

const OnboardingContainer = Onboarding.getOnboarding();
export default OnboardingContainer;
