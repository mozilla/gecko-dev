/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineLazyGetter(lazy, "console", () => {
  return console.createInstance({
    prefix: "UserCharacteristicsPage",
    maxLogLevelPref: "toolkit.telemetry.user_characteristics_ping.logLevel",
  });
});

export class UserCharacteristicsScreenInfoChild extends JSWindowActorChild {
  populateScreenInfo() {
    if (
      this.document.location.href === "about:fingerprintingprotection" ||
      !this.document.body.firstElementChild
    ) {
      return;
    }

    const result = [
      this.contentWindow.outerHeight,
      this.contentWindow.innerHeight,
      this.contentWindow.outerWidth,
      this.contentWindow.innerWidth,
      this.contentWindow.screen.availHeight,
      this.contentWindow.screen.availWidth,
    ];

    if (result.some(v => v <= 0)) {
      return;
    }

    this.sendAsyncMessage("ScreenInfo:Populate", result);
  }

  async receiveMessage(msg) {
    lazy.console.debug("Got ", msg.name);
    switch (msg.name) {
      case "ScreenInfo:PopulateFromDocument":
        if (this.document.readyState == "complete") {
          this.populateScreenInfo();
        }
        break;
    }

    return null;
  }

  async handleEvent(event) {
    lazy.console.debug("Got ", event.type);
    switch (event.type) {
      case "DOMContentLoaded":
        this.populateScreenInfo();
        break;
    }
  }
}
