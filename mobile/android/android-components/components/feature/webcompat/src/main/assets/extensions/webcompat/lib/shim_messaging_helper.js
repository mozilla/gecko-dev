/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* globals browser */

if (!window.Shims) {
  window.Shims = new Map();
}

if (!window.ShimIdsToPorts) {
  // map mapping shimIds to their connected message port
  // used for sending messages from the background script the shim.
  window.ShimIdsToPorts = new Map();
}

if (!window.ShimsHelperReady) {
  window.ShimsHelperReady = true;

  browser.runtime.onMessage.addListener(details => {
    const { shimId, topic, data, warning } = details;
    if (!shimId) {
      return;
    }
    switch (topic) {
      case "smartblock:unblock-embed":
        sendMessage(shimId, { topic, data });
        break;
      default:
        window.Shims.set(shimId, details);
        if (warning) {
          console.warn(warning);
        }
    }
  });

  async function sendMessage(shimId, message) {
    const port = window.ShimIdsToPorts.get(shimId);
    if (!port) {
      console.error("Shim must connect to background script first");
      return;
    }
    const messageId = crypto.randomUUID();
    port.postMessage({ messageId, message });
  }

  async function handleMessage(port, shimId, messageId, message) {
    let response;
    const shim = window.Shims.get(shimId);
    if (shim) {
      const { needsShimHelpers, origin } = shim;
      if (origin === location.origin) {
        if (needsShimHelpers?.includes(message)) {
          const msg = { shimId, message };
          try {
            response = await browser.runtime.sendMessage(msg);
          } catch (_) {}
        }
      }
    }
    port.postMessage({ messageId, response });
  }

  window.addEventListener(
    "ShimConnects",
    e => {
      e.stopPropagation();
      e.preventDefault();
      const { port, pendingMessages, shimId } = e.detail;
      const shim = window.Shims.get(shimId);
      if (!shim) {
        return;
      }
      port.onmessage = ({ data }) => {
        handleMessage(port, shimId, data.messageId, data.message);
      };
      window.ShimIdsToPorts.set(shimId, port);
      for (const [messageId, message] of pendingMessages) {
        handleMessage(port, shimId, messageId, message);
      }
    },
    true
  );

  window.dispatchEvent(new CustomEvent("ShimHelperReady"));
}
