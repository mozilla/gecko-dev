/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env webextensions */

/*
Establish communication with native application.
*/
const WEB_CHANNEL_BACKGROUND_MESSAGING_ID = "mozacWebchannelBackground";
let port = browser.runtime.connectNative(WEB_CHANNEL_BACKGROUND_MESSAGING_ID);
/*
Handle messages from native application, register content script for specific url.
*/
port.onMessage.addListener(event => {
  if (event.type == "overrideFxAServer") {
    // To allow localhost to be matched, we create a url pattern so that localhost can be included
    // without the ports.
    // See: https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/Match_patterns#path
    const url = new URL(event.url);
    const urlPattern = `${url.protocol}//${url.hostname}/*`;

    browser.contentScripts.register({
      matches: [urlPattern],
      js: [{ file: "fxawebchannel.js" }],
      runAt: "document_start",
    });
    port.disconnect();
  }
});
