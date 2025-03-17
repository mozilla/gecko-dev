/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

/* eslint-env webextensions */
"use strict";

/**
 * Display a message to the user in the content page.
 */
const displayMessage = async (tabId, message) => {
  await browser.scripting.executeScript({
    target: { tabId },
    func: message => {
      const { altTextModal } = window;
      altTextModal.updateText(message);
    },
    args: [message],
  });
};

/**
 * Ensures the engine is ready. Since there is no way to know whether an engine
 * has been created, and we are limited to just 1 engine per extension, we
 * store a boolean in session storage.
 */
const ensureEngineIsReady = async tabId => {
  const { engineCreated } = await browser.storage.session.get({
    engineCreated: false,
  });

  if (engineCreated) {
    return;
  }

  const listener = progressData => {
    browser.tabs.sendMessage(tabId, progressData);
  };
  browser.trial.ml.onProgress.addListener(listener);

  try {
    await displayMessage(tabId, "Initializing...");
    await browser.trial.ml.createEngine({
      modelHub: "mozilla",
      taskName: "image-to-text",
    });
    browser.storage.session.set({ engineCreated: true });
  } catch (err) {
    await displayMessage(tabId, `${err}`);
  } finally {
    browser.trial.ml.onProgress.removeListener(listener);
  }
};

/**
 * Initializes the ML engine as well as the content CSS/JS in the page, and
 * then run the inference to generate an alt text for a given image (URL).
 */
const generateAltText = async (tabId, imageUrl) => {
  const [{ result: hasAltTextModal }] = await browser.scripting.executeScript({
    target: { tabId },
    func: () => {
      return typeof window.altTextModal !== "undefined";
    },
  });

  if (!hasAltTextModal) {
    // Inject alt-text-modal.*, which creates the AltTextModal instance.
    await browser.scripting.insertCSS({
      target: { tabId },
      files: ["./alt-text-modal.css"],
    });
    await browser.scripting.executeScript({
      target: { tabId },
      files: ["./alt-text-modal.js"],
    });
  }

  try {
    // Make sure the engine is ready.
    await ensureEngineIsReady(tabId);

    // Generate the alt-text for the image.
    await browser.scripting.executeScript({
      target: { tabId },
      func: async imageUrl => {
        const { altTextModal } = window;
        try {
          altTextModal.updateText("Running inference...");

          const res = await browser.trial.ml.runEngine({
            args: [imageUrl],
          });
          altTextModal.updateText(res[0].generated_text);
        } catch (err) {
          altTextModal.updateText(`${err}`);
        }
      },
      args: [imageUrl],
    });
  } catch (err) {
    console.warn(err);
  }
};

// Currently, Firefox for Android does not support the menus API, so we use a
// different trigger to provide the alt text generation. On Firefox for
// desktop, we use the context menu, and on Android, we use a long-press event.
if ("menus" in browser) {
  // When the menus API is available, we create a new item in the context menu.
  // When this item is clicked, we run the alt-text generation.
  browser.menus.onClicked.addListener((info, tab) => {
    if (info.menuItemId !== "generate-alt-text" || !info.srcUrl) {
      return;
    }

    generateAltText(tab.id, info.srcUrl);
  });
} else {
  // When the menus API is not available, we will receive a message from a
  // content script that is injected below. This content script adds a listener
  // to each image in the content page, which will send a message back to the
  // background page when an event is emitted.
  //
  // See also the logic in the `onInstalled` listener.
  browser.runtime.onMessage.addListener((msg, sender) => {
    if (msg.type !== "generate-alt-text") {
      return;
    }

    generateAltText(sender.tab.id, msg.data.url);
  });
}

browser.runtime.onInstalled.addListener(async () => {
  if ("menus" in browser) {
    await browser.menus.create({
      id: "generate-alt-text",
      title: "✨ Generate Alt Text",
      contexts: ["image"],
    });
  } else {
    const scripts = await browser.scripting.getRegisteredContentScripts();
    if (!scripts.some(script => script.id === "contextmenu-shim")) {
      await browser.scripting.registerContentScripts([
        {
          id: "contextmenu-shim",
          js: ["./contextmenu-shim.js"],
          matches: ["<all_urls>"],
        },
      ]);
    }
  }

  const granted = await browser.permissions.contains({
    permissions: ["trialML"],
  });

  if (!granted) {
    // `browser.runtime.openOptionsPage()` is not working well on Firefox for
    // Android, see: https://bugzilla.mozilla.org/show_bug.cgi?id=1795449
    browser.tabs.create({ url: browser.runtime.getURL("settings.html") });
  }
});
