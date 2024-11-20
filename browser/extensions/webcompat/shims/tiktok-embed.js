/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

if (!window.smartblockTiktokShimInitialized) {
  // Guard against this script running multiple times
  window.smartblockTiktokShimInitialized = Object.freeze(true);

  // Original URL of the Instagram embed script.
  const ORIGINAL_URL = "https://www.tiktok.com/embed.js";

  const LOGO_URL = "https://smartblock.firefox.etp/tiktok.svg";

  let originalEmbedContainers = document.querySelectorAll(".tiktok-embed");
  let embedPlaceholders = [];

  // Bug 1925582: this should be a common snippet for use in multiple shims.
  const sendMessageToAddon = (function () {
    const shimId = "TiktokEmbed";
    const pendingMessages = new Map();
    const channel = new MessageChannel();
    channel.port1.onerror = console.error;
    channel.port1.onmessage = event => {
      const { messageId, response, message } = event.data;
      const resolve = pendingMessages.get(messageId);
      if (resolve) {
        // message is a response to a previous message
        pendingMessages.delete(messageId);
        resolve(response);
      } else {
        addonMessageHandler(message);
      }
    };
    function reconnect() {
      const detail = {
        pendingMessages: [...pendingMessages.values()],
        port: channel.port2,
        shimId,
      };
      window.dispatchEvent(new CustomEvent("ShimConnects", { detail }));
    }
    window.addEventListener("ShimHelperReady", reconnect);
    reconnect();
    return function (message) {
      const messageId = crypto.randomUUID();
      return new Promise(resolve => {
        const payload = { message, messageId, shimId };
        pendingMessages.set(messageId, resolve);
        channel.port1.postMessage(payload);
      });
    };
  })();

  function addonMessageHandler(message) {
    let { topic, data } = message;
    if (topic === "smartblock:unblock-embed") {
      if (data != window.location.hostname) {
        // host name does not match the original hostname, user must have navigated
        // away, skip replacing embeds
        return;
      }
      // remove embed placeholders
      embedPlaceholders.forEach((p, idx) => {
        p.replaceWith(originalEmbedContainers[idx]);
      });

      // recreate scripts
      let scriptElement = document.createElement("script");
      scriptElement.src = ORIGINAL_URL;
      document.body.appendChild(scriptElement);
    }
  }

  async function createShimPlaceholders() {
    const [titleString, descriptionString, buttonString] =
      await sendMessageToAddon("smartblockGetFluentString");

    originalEmbedContainers.forEach(originalEmbedContainer => {
      // this string has to be defined within this function to avoid linting errors
      // see: https://github.com/mozilla/eslint-plugin-no-unsanitized/issues/259
      const SMARTBLOCK_PLACEHOLDER_HTML_STRING = `
      <style>
        #smartblock-placeholder-wrapper {
          min-height: 225px;
          width: 400px;
          padding: 32px 24px;

          display: block;
          align-content: center;
          text-align: center;

          background-color: light-dark(rgb(255, 255, 255), rgb(28, 27, 34));
          color: light-dark(rgb(43, 42, 51), rgb(251, 251, 254));

          border-radius: 8px;
          border: 2px dashed #0250bb;

          font-size: 14px;
          line-height: 1.2;
          font-family: system-ui;
        }

        #smartblock-placeholder-button {
          min-height: 32px;
          padding: 8px 14px;

          border-radius: 4px;
          font-weight: 600;
          border: 0;

          /* Colours match light/dark theme from
            https://searchfox.org/mozilla-central/source/browser/themes/addons/light/manifest.json
            https://searchfox.org/mozilla-central/source/browser/themes/addons/dark/manifest.json */
          background-color: light-dark(rgb(0, 97, 224), rgb(0, 221, 255));
          color: light-dark(rgb(251, 251, 254), rgb(43, 42, 51));
        }

        #smartblock-placeholder-button:hover {
          /* Colours match light/dark theme from
            https://searchfox.org/mozilla-central/source/browser/themes/addons/light/manifest.json
            https://searchfox.org/mozilla-central/source/browser/themes/addons/dark/manifest.json */
          background-color: light-dark(rgb(2, 80, 187), rgb(128, 235, 255));
        }

        #smartblock-placeholder-button:hover:active {
          /* Colours match light/dark theme from
            https://searchfox.org/mozilla-central/source/browser/themes/addons/light/manifest.json
            https://searchfox.org/mozilla-central/source/browser/themes/addons/dark/manifest.json */
          background-color: light-dark(rgb(5, 62, 148), rgb(170, 242, 255));
        }

        #smartblock-placeholder-title {
          margin-block: 14px;
          font-size: 16px;
          font-weight: bold;
        }

        #smartblock-placeholder-desc {
          margin-block: 14px;
        }
      </style>
      <div id="smartblock-placeholder-wrapper">
        <img id="smartblock-placeholder-image" width="24" height="24" />
        <p id="smartblock-placeholder-title"></p>
        <p id="smartblock-placeholder-desc"></p>
        <button id="smartblock-placeholder-button"></button>
      </div>`;

      // Create the placeholder inside a shadow dom
      const placeholderDiv = document.createElement("div");
      embedPlaceholders.push(placeholderDiv);

      const shadowRoot = placeholderDiv.attachShadow({ mode: "closed" });

      shadowRoot.innerHTML = SMARTBLOCK_PLACEHOLDER_HTML_STRING;
      shadowRoot.getElementById("smartblock-placeholder-image").src = LOGO_URL;
      shadowRoot.getElementById("smartblock-placeholder-title").textContent =
        titleString;
      shadowRoot.getElementById("smartblock-placeholder-desc").textContent =
        descriptionString;
      shadowRoot.getElementById("smartblock-placeholder-button").textContent =
        buttonString;

      // Wait for user to opt-in.
      shadowRoot
        .getElementById("smartblock-placeholder-button")
        .addEventListener("click", () => {
          // Send a message to the addon to allow loading Instagram tracking resources
          // needed by the embed.
          sendMessageToAddon("embedClicked");
        });

      // Replace the embed with the placeholder
      originalEmbedContainer.replaceWith(placeholderDiv);
    });
  }

  createShimPlaceholders();
}
