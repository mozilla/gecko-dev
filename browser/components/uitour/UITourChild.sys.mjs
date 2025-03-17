/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const PREF_TEST_ORIGINS = "browser.uitour.testingOrigins";
const UITOUR_PERMISSION = "uitour";

export class UITourChild extends JSWindowActorChild {
  handleEvent(event) {
    if (!this.ensureTrustedOrigin()) {
      return;
    }

    this.sendAsyncMessage("UITour:onPageEvent", {
      detail: event.detail,
      type: event.type,
      pageVisibilityState: this.document.visibilityState,
    });
  }

  isTestingOrigin(aURI) {
    let testingOrigins = Services.prefs.getStringPref(PREF_TEST_ORIGINS, "");
    if (!testingOrigins) {
      return false;
    }

    // Allow any testing origins (comma-seperated).
    for (let origin of testingOrigins.split(/\s*,\s*/)) {
      try {
        let testingURI = Services.io.newURI(origin);
        if (aURI.prePath == testingURI.prePath) {
          return true;
        }
      } catch (ex) {
        console.error(ex);
      }
    }
    return false;
  }

  // This function is copied from UITour.sys.mjs.
  isSafeScheme(aURI) {
    let allowedSchemes = new Set(["https", "about"]);
    if (!allowedSchemes.has(aURI.scheme)) {
      return false;
    }

    return true;
  }

  ensureTrustedOrigin() {
    if (this.browsingContext.top != this.browsingContext) {
      return false;
    }

    let uri = this.document.documentURIObject;

    if (uri.schemeIs("chrome")) {
      return true;
    }

    if (!this.isSafeScheme(uri)) {
      return false;
    }

    let permission = Services.perms.testPermissionFromPrincipal(
      this.document.nodePrincipal,
      UITOUR_PERMISSION
    );
    if (permission == Services.perms.ALLOW_ACTION) {
      return true;
    }

    return this.isTestingOrigin(uri);
  }

  receiveMessage(aMessage) {
    switch (aMessage.name) {
      case "UITour:SendPageCallback":
        this.sendPageEvent("Response", aMessage.data);
        break;
      case "UITour:SendPageNotification":
        this.sendPageEvent("Notification", aMessage.data);
        break;
    }
  }

  sendPageEvent(type, detail) {
    if (!this.ensureTrustedOrigin()) {
      return;
    }

    let win = this.contentWindow;
    let eventName = "mozUITour" + type;
    let event = new win.CustomEvent(eventName, {
      bubbles: true,
      detail: Cu.cloneInto(detail, win),
    });
    win.document.dispatchEvent(event);
  }
}
