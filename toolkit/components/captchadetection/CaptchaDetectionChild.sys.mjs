/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineLazyGetter(lazy, "console", () => {
  return console.createInstance({
    prefix: "CaptchaDetectionChild",
    maxLogLevelPref: "captchadetection.loglevel",
  });
});

/**
 * This actor runs in the captcha's frame. It provides information
 * about the captcha's state to the parent actor.
 */
export class CaptchaDetectionChild extends JSWindowActorChild {
  actorCreated() {
    lazy.console.debug("actorCreated");
  }

  handleEvent(event) {
    lazy.console.debug("handleEvent", event);
  }
}
