/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineLazyGetter(lazy, "console", () => {
  return console.createInstance({
    prefix: "CaptchaDetectionParent",
    maxLogLevelPref: "captchadetection.loglevel",
  });
});

/**
 * This actor parent is responsible for recording the state of captchas
 * or communicating with parent browsing context.
 */
export class CaptchaDetectionParent extends JSWindowActorParent {
  actorCreated() {
    lazy.console.debug("actorCreated");
  }
}
