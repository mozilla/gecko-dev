/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

/* eslint-disable no-undef */
/**
 * Called in the tab content
 */
async function generateAltText(targetElementId) {
  const modal = getModal();
  try {
    const imageUrl = browser.menus.getTargetElement(targetElementId).src;
    modal.updateText("Running inference...");

    const res = await browser.trial.ml.runEngine({
      args: [imageUrl],
    });
    modal.updateText(res[0].generated_text);
  } catch (err) {
    modal.updateText(`${err}`);
  }
}

/**
 * Called in the tab content
 */
async function _displayMessage(message) {
  getModal().updateText(message);
}

/**
 * Calling _displayMessage in the tab content to display the message to the user.
 */
async function displayMessage(tab, message) {
  await browser.scripting.executeScript({
    target: {
      tabId: tab.id,
    },
    func: _displayMessage.bind(null, message),
  });
}

// Initialize the Map to track first run status per tab
const firstRunOnTab = new Map();

/**
 * Sets the first run status for a specific tab.
 *
 * @param {number} tabId - The ID of the tab.
 * @param {boolean} isFirstRun - True if this is the first run on this tab, false otherwise.
 */
function setFirstRun(tabId, isFirstRun) {
  firstRunOnTab.set(tabId, isFirstRun);
}

/**
 * Checks if this is the first run on a specific tab.
 * Defaults to true if the tab has no entry in the map.
 *
 * @param {number} tabId - The ID of the tab.
 * @returns {boolean} - True if this is the first run on this tab, false otherwise.
 */
function isFirstRun(tabId) {
  return firstRunOnTab.get(tabId) !== false;
}

/**
 * Clears the first run status for a tab when it closes or when necessary.
 *
 * @param {number} tabId - The ID of the tab.
 */
function clearFirstRun(tabId) {
  firstRunOnTab.delete(tabId);
}

async function onclick(info, tab) {
  if (isFirstRun(tab.id)) {
    browser.tabs.insertCSS(tab.id, {
      file: "./alt-text-modal.css",
    });
  }

  const listener = progressData => {
    browser.tabs.sendMessage(tab.id, progressData);
  };

  browser.trial.ml.onProgress.addListener(listener);

  try {
    if (isFirstRun(tab.id)) {
      // injecting content-script.js, which creates the AltTextModal instance.
      await browser.scripting.executeScript({
        target: { tabId: tab.id },
        files: ["./content-script.js"],
      });

      await displayMessage(tab, "Initializing...");
      try {
        await browser.trial.ml.createEngine({
          modelHub: "mozilla",
          taskName: "image-to-text",
        });
      } catch (err) {
        await displayMessage(tab, `${err}`);
        return;
      }
    }
    // running generateAltText
    await browser.scripting.executeScript({
      target: {
        tabId: tab.id,
      },
      func: generateAltText,
      args: [info.targetElementId],
    });
  } finally {
    browser.trial.ml.onProgress.removeListener(listener);
    setFirstRun(tab.id, false);
  }
}

browser.menus.create({
  title: "✨ Generate Alt Text",
  documentUrlPatterns: ["*://*/*"],
  contexts: ["image"],
  onclick,
});

browser.permissions.contains({ permissions: ["trialML"] }).then(granted => {
  if (!granted) {
    browser.tabs.create({ url: browser.runtime.getURL("settings.html") });
  }
});
