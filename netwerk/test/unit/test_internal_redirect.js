/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { HttpServer } = ChromeUtils.importESModule(
  "resource://testing-common/httpd.sys.mjs"
);

const SUCCESS_TEXT = "success!";

function successResponseHandler(req, resp) {
  var text = SUCCESS_TEXT;
  resp.setHeader("Content-Type", "text/plain", false);
  resp.bodyOutputStream.write(text, text.length);
}

function onBeforeConnect(callback) {
  Services.obs.addObserver(
    {
      observe(subject) {
        Services.obs.removeObserver(this, "http-on-before-connect");
        callback(subject.QueryInterface(Ci.nsIHttpChannel));
      },
    },
    "http-on-before-connect"
  );
}

class EventSinkListener {
  constructor(internalRedirect) {
    this.internalRedirect = internalRedirect;
  }

  getInterface(iid) {
    if (iid.equals(Ci.nsIChannelEventSink)) {
      return this;
    }
    throw Components.Exception("", Cr.NS_ERROR_NO_INTERFACE);
  }
  asyncOnChannelRedirect(oldChan, newChan, flags, callback) {
    Assert.ok(
      !!(flags & Ci.nsIChannelEventSink.REDIRECT_INTERNAL) ===
        this.internalRedirect,
      `REDIRECT_INTERNAL flag should ${
        this.internalRedirect ? "be" : "not be"
      } set`
    );
    callback.onRedirectVerifyCallback(Cr.NS_OK);
  }
}

EventSinkListener.prototype.QueryInterface = ChromeUtils.generateQI([
  "nsIInterfaceRequestor",
  "nsIChannelEventSink",
]);

async function test_redirect(internalRedirect) {
  var server = new HttpServer();
  await server.start(-1);
  registerCleanupFunction(async () => {
    await server.stop();
  });

  server.registerPathHandler("/success", successResponseHandler);

  const baseUrl = `http://localhost:${server.identity.primaryPort}/`;
  const successUrl = baseUrl + "success";

  onBeforeConnect(chan => {
    chan.suspend();
    Promise.resolve().then(() => {
      try {
        if (internalRedirect) {
          chan.internalRedirectTo(Services.io.newURI(successUrl));
        } else {
          chan.redirectTo(Services.io.newURI(successUrl));
        }
      } catch (e) {
        do_throw(e);
      }
      chan.resume();
    });
  });

  let chan = NetUtil.newChannel({
    uri: baseUrl,
    loadUsingSystemPrincipal: true,
  }).QueryInterface(Ci.nsIHttpChannelInternal);
  let listener = new EventSinkListener(internalRedirect);
  chan.notificationCallbacks = listener;

  await new Promise(resolve => {
    chan.asyncOpen(
      new ChannelListener(
        (req, resp) => {
          // Should hit /success for both.
          Assert.equal(resp, SUCCESS_TEXT, "Should redirect to success");
          resolve();
        },
        null,
        CL_ALLOW_UNKNOWN_CL
      )
    );
  });
}

add_task(async function test_internal_redirect() {
  await test_redirect(true);
});

add_task(async function test_normal_redirect() {
  await test_redirect(false);
});
