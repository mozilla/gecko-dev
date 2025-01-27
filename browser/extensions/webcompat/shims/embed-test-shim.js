/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

/* globals browser */

if (!window.smartblockTestShimInitialized) {
  // Guard against this script running multiple times
  window.smartblockTestShimInitialized = true;

  const SHIM_ID = "EmbedTestShim";

  // Original URL of the test embed script.
  const ORIGINAL_URL =
    "https://itisatracker.org/browser/browser/extensions/webcompat/tests/browser/embed_test.js";

  // Use instagram logo as a test logo
  const LOGO_URL = "https://smartblock.firefox.etp/instagram.svg";

  let originalEmbedContainers = document.querySelectorAll(
    ".broken-embed-content"
  );
  let embedPlaceholders = [];

  // Bug 1925582: this should be a common snippet for use in multiple shims.
  function sendMessageToAddon(message) {
    return browser.runtime.sendMessage({ message, shimId: SHIM_ID });
  }

  function addonMessageHandler(message) {
    let { topic, shimId } = message;
    // Only react to messages which are targeting this shim.
    if (shimId != SHIM_ID) {
      return;
    }

    if (topic === "smartblock:unblock-embed") {
      // remove embed placeholders
      embedPlaceholders.forEach((p, idx) => {
        p.replaceWith(originalEmbedContainers[idx]);
      });

      // recreate scripts
      let scriptElement = document.createElement("script");

      // Set the script element's src with the website's principal instead of
      // the content script principal to ensure the tracker script is not loaded
      // via the content script's expanded principal.
      scriptElement.wrappedJSObject.src = ORIGINAL_URL;
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

      // Tag the div with a class to make it easily detectable FOR THE TEST SHIM ONLY
      placeholderDiv.classList.add("shimmed-embedded-content");

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
        .addEventListener("click", ({ isTrusted }) => {
          if (!isTrusted) {
            return;
          }
          // Send a message to the addon to allow loading TikTok tracking resources
          // needed by the embed.
          sendMessageToAddon("embedClicked");
        });

      // Replace the embed with the placeholder
      originalEmbedContainer.replaceWith(placeholderDiv);

      sendMessageToAddon("smartblockEmbedReplaced");
    });

    // Dispatch event to signal that the script is done replacing FOR TEST SHIM ONLY
    const finishedEvent = new CustomEvent("smartblockEmbedScriptFinished", {
      bubbles: true,
      composed: true,
    });
    window.dispatchEvent(finishedEvent);
  }

  // Listen for messages from the background script.
  browser.runtime.onMessage.addListener(request => {
    addonMessageHandler(request);
  });

  createShimPlaceholders();
}
