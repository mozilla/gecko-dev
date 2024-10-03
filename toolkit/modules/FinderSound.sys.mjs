// vim: set ts=2 sw=2 sts=2 tw=80:
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

const kSoundEnabledPref = "accessibility.typeaheadfind.enablesound";
const kNotFoundSoundPref = "accessibility.typeaheadfind.soundURL";

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

let gSound = null;

export function resetSound() {
  gSound = null;
}

export function initNotFoundSound() {
  if (!gSound && lazy.isSoundEnabled && lazy.notFoundSoundURL) {
    try {
      gSound = Cc["@mozilla.org/sound;1"].getService(Ci.nsISound);
      gSound.init();
    } catch (ex) {}
  }
}

export function playNotFoundSound() {
  if (!lazy.isSoundEnabled || !lazy.notFoundSoundURL) {
    return;
  }

  initNotFoundSound();
  if (!gSound) {
    return;
  }

  let soundUrl = lazy.notFoundSoundURL;
  if (soundUrl == "beep") {
    gSound.beep();
  } else {
    if (soundUrl == "default") {
      soundUrl = "chrome://global/content/notfound.wav";
    }
    gSound.play(Services.io.newURI(soundUrl));
  }
}
