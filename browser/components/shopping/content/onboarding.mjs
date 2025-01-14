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
    const addStylesheet = href => {
      if (this.doc.head.querySelector(`link[href="${href}"]`)) {
        return;
      }
      const link = this.doc.head.appendChild(this.doc.createElement("link"));
      link.rel = "stylesheet";
      link.href = href;
    };
    addStylesheet("chrome://browser/content/aboutwelcome/aboutwelcome.css");
    const reactSrc = "chrome://global/content/vendor/react.js";
    const domSrc = "chrome://global/content/vendor/react-dom.js";
    // Add React script
    const getReactReady = async () => {
      return new Promise(resolve => {
        let reactScript = this.doc.createElement("script");
        reactScript.src = reactSrc;
        this.doc.head.appendChild(reactScript);
        reactScript.addEventListener("load", resolve);
      });
    };
    // Add ReactDom script
    const getDomReady = async () => {
      return new Promise(resolve => {
        let domScript = this.doc.createElement("script");
        domScript.src = domSrc;
        this.doc.head.appendChild(domScript);
        domScript.addEventListener("load", resolve);
      });
    };

    const getDocumentReady = async () => {
      new Promise(resolve => {
        this.doc.addEventListener(
          "readystatechange",
          function onReadyStateChange() {
            if (this.doc.readyState != "complete") {
              return;
            }
            this.doc.removeEventListener(
              "readystatechange",
              onReadyStateChange
            );
            resolve();
          }
        );
      });
    };

    let reactScript = this.doc.querySelector(`[src="${reactSrc}"]`);
    let reactDomScript = this.doc.querySelector(`[src="${domSrc}"]`);

    // If either script has already been added but hasn't finished
    // loading yet, wait for the document's readyState to be complete.
    if ((reactScript || reactDomScript) && this.doc.readyState != "complete") {
      await getDocumentReady();
    }
    // Load React, then React Dom
    if (!reactScript) {
      await getReactReady();
    }
    if (!reactDomScript) {
      await getDomReady();
    }

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
