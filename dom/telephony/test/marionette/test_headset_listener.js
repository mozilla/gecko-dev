/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_CONTEXT = "chrome";
MARIONETTE_TIMEOUT = 60000;

XPCOMUtils.defineLazyServiceGetter(this, "gAudioService",
                                   "@mozilla.org/telephony/audio-service;1",
                                   "nsITelephonyAudioService");



function verifyHeadPhoneState(aState) {
  let headsetState = {
    unknown: Ci.nsITelephonyAudioService.HEADSET_STATE_UNKNOWN,
    off: Ci.nsITelephonyAudioService.HEADSET_STATE_OFF,
    headset: Ci.nsITelephonyAudioService.HEADSET_STATE_HEADSET,
    headphone: Ci.nsITelephonyAudioService.HEADSET_STATE_HEADPHONE
  }[aState];

  let listener = {
    headsetState: Ci.nsITelephonyAudioService.HEADSET_STATE_OFF,
    notifyHeadsetStateChanged: function(aState) {
      this.headsetState = aState;
    }
  };

  function notifyHeadphonesStatus(aState) {
    SpecialPowers.notifyObserversInParentProcess(null,
                                                 "headphones-status-changed",
                                                 aState);
  }

  gAudioService.registerListener(listener);
  notifyHeadphonesStatus(aState);
  is(listener.headsetState, headsetState, "verify " + aState);
  gAudioService.unregisterListener(listener);
}

// Start the test
["unknown", "headset", "headphone", "off"]
  .forEach(aState => verifyHeadPhoneState(aState));

finish();
