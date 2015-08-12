/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

startTest(function() {
  return Promise.resolve()
    .then(() => {
      // expect the default value to be "off"
      is(telephony.ttyMode, "off");
      telephony.ttyMode = "full";
      is(telephony.ttyMode, "full");
      telephony.ttyMode = "hco";
      is(telephony.ttyMode, "hco");
      telephony.ttyMode = "vco";
      is(telephony.ttyMode, "vco");
      telephony.ttyMode = "off";
      is(telephony.ttyMode, "off");
      finish();
    });
});
