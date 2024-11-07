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

ChromeUtils.defineESModuleGetters(lazy, {
  setTimeout: "resource://gre/modules/Timer.sys.mjs",
});

export class UserCharacteristicsWindowInfoChild extends JSWindowActorChild {
  constructor() {
    super();

    // Expected properties to collect before
    // sending the "WindowInfo::Done" message
    this.targetProperties = new Set(["ScreenInfo", "PointerInfo"]);
    this.collectedProperties = new Set();
    // Event handlers to remove when the actor is destroyed
    this.handlers = {};
    this.destroyed = false;
  }

  /*
   * This function populates the screen info from the tab it is running in,
   * and sends the properties to the parent actor.
   */
  populateScreenInfo() {
    // I'm not sure if this is really necessary, but it's here to prevent
    // the actor from running in the about:fingerprintingprotection page
    // which is a hidden browser with no toolbars etc.
    if (this.document.location.href === "about:fingerprintingprotection") {
      return;
    }

    const result = {
      outerHeight: this.contentWindow.outerHeight,
      innerHeight: this.contentWindow.innerHeight,
      outerWidth: this.contentWindow.outerWidth,
      innerWidth: this.contentWindow.innerWidth,
      availHeight: this.contentWindow.screen.availHeight,
      availWidth: this.contentWindow.screen.availWidth,
    };

    if (Object.values(result).some(v => v <= 0)) {
      return;
    }

    this.sendMessage("ScreenInfo:Populated", result);
    this.propertyCollected("ScreenInfo");
  }

  /*
   * This function listens for touchstart and pointerdown events
   * and sends the properties of the event to the parent actor.
   * The reason that we listen for both events is because rotationAngle
   * is only available in touch events and the rest of the properties
   * are only available in pointer events.
   */
  populatePointerInfo() {
    const { promise, resolve } = Promise.withResolvers();

    const touchStartHandler = e => mergeEvents(e);
    this.contentWindow.windowRoot.addEventListener(
      "touchstart",
      touchStartHandler,
      { once: true }
    );

    // Allow some time for the touchstart event to be recorded
    const pointerDownHandler = e => lazy.setTimeout(() => mergeEvents(e), 500);
    this.contentWindow.windowRoot.addEventListener(
      "pointerdown",
      pointerDownHandler,
      { once: true }
    );

    const mergedEvents = {};
    const mergeEvents = event => {
      if (event.type === "touchstart") {
        mergedEvents.touch = event;
      } else {
        mergedEvents.pointer = event;
      }

      // Resolve the promise if we got pointerdown
      if (mergedEvents.pointer) {
        resolve(mergedEvents);
      }
    };

    promise.then(e => {
      this.sendMessage("PointerInfo:Populated", {
        pointerPressure: e.pointer.pressure,
        pointerTangentinalPressure: e.pointer.tangentialPressure,
        pointerTiltx: e.pointer.tiltX,
        pointerTilty: e.pointer.tiltY,
        pointerTwist: e.pointer.twist,
        pointerWidth: e.pointer.width,
        pointerHeight: e.pointer.height,
        touchRotationAngle: e.touch?.rotationAngle || 0,
      });
      this.propertyCollected("PointerInfo");
    });

    this.handlers.touchstart = touchStartHandler;
    this.handlers.pointerdown = pointerDownHandler;
  }

  sendMessage(name, obj, transferables) {
    if (this.destroyed) {
      return;
    }

    this.sendAsyncMessage(name, obj, transferables);
  }

  propertyCollected(name) {
    this.collectedProperties.add(name);
    if (this.targetProperties.difference(this.collectedProperties).size === 0) {
      this.sendMessage("WindowInfo::Done");
    }
  }

  didDestroy() {
    this.destroyed = true;
    for (const [type, handler] of Object.entries(this.handlers)) {
      this.contentWindow?.windowRoot?.removeEventListener(type, handler);
    }
  }

  async receiveMessage(msg) {
    lazy.console.debug("Actor Child: Got ", msg.name);
    switch (msg.name) {
      case "WindowInfo:PopulateFromDocument":
        if (this.document.readyState == "complete") {
          this.populateScreenInfo();
          this.populatePointerInfo();
        }
        break;
    }

    return null;
  }

  async handleEvent(event) {
    lazy.console.debug("Actor Child: Got ", event.type);
    switch (event.type) {
      case "DOMContentLoaded":
        this.populateScreenInfo();
        this.populatePointerInfo();
        break;
    }
  }
}
