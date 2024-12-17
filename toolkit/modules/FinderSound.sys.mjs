// vim: set ts=2 sw=2 sts=2 tw=80:
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

const kSoundEnabledPref = "accessibility.typeaheadfind.enablesound";
const kNotFoundSoundPref = "accessibility.typeaheadfind.soundURL";
const kWrappedSoundPref = "accessibility.typeaheadfind.wrappedSoundURL";

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "isSoundEnabled",
  kSoundEnabledPref,
  false
);

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "notFoundSoundURL",
  kNotFoundSoundPref,
  ""
);

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "wrappedSoundURL",
  kWrappedSoundPref,
  ""
);

let gSound = null;

export function resetSound() {
  gSound = null;
}

export function initSound() {
  if (!gSound && lazy.isSoundEnabled) {
    try {
      gSound = Cc["@mozilla.org/sound;1"].getService(Ci.nsISound);
      gSound.init();
    } catch (ex) {}
  }
}

export function playSound(event) {
  if (!lazy.isSoundEnabled) {
    return;
  }

  initSound();
  if (!gSound) {
    return;
  }

  let soundUrl;

  switch (event) {
    case "not-found":
      soundUrl = lazy.notFoundSoundURL;
      break;
    case "wrapped":
      soundUrl = lazy.wrappedSoundURL;
      break;
    default:
      return;
  }

  if (soundUrl === "") {
    return;
  }

  if (soundUrl == "beep") {
    gSound.beep();
  } else {
    if (soundUrl == "default") {
      soundUrl = "chrome://global/content/notfound.wav";
    }
    gSound.play(Services.io.newURI(soundUrl));
  }
}
