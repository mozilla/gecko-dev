/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  GenAI: "resource:///modules/GenAI.sys.mjs",
});
const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "providerPref",
  "browser.ml.chat.provider",
  null,
  renderProviders
);

const node = {};

function request(url = lazy.providerPref) {
  try {
    node.chat.fixupAndLoadURIString(url, {
      triggeringPrincipal: Services.scriptSecurityManager.createNullPrincipal(
        {}
      ),
    });
  } catch (ex) {
    console.error("Failed to request chat provider", ex);
  }
}

function renderChat() {
  const browser = document.createXULElement("browser");
  browser.setAttribute("type", "content");
  browser.setAttribute("remote", "true");
  return document.body.appendChild(browser);
}

async function renderProviders() {
  // Skip potential pref change callback when unloading
  if ((await document.visibilityState) == "hidden") {
    return null;
  }

  const select = document.getElementById("provider");
  select.innerHTML = "";
  let selected = false;

  const addOption = (text, val) => {
    const option = select.appendChild(document.createElement("option"));
    option.textContent = text;
    option.value = val;
    return option;
  };

  // Add the known providers in order while looking for current selection
  lazy.GenAI.chatProviders.forEach((data, url) => {
    const option = addOption(data.name, url);
    if (lazy.providerPref == url) {
      option.selected = true;
      selected = true;
    } else if (data.hidden) {
      option.hidden = true;
    }
  });

  // Must be a custom preference if provider wasn't found
  if (!selected) {
    const option = addOption(
      `Custom provider (${lazy.providerPref})`,
      lazy.providerPref
    );
    option.selected = true;
  }

  // Load the requested provider
  request();
  return select;
}

function handleChange({ target }) {
  const { value } = target;
  switch (target) {
    case node.provider:
      Services.prefs.setStringPref("browser.ml.chat.provider", value);
      break;
  }
}
addEventListener("change", handleChange);

// Expose a promise for loading and rendering the chat browser element
var browserPromise = new Promise((resolve, reject) => {
  addEventListener("load", async () => {
    try {
      node.chat = renderChat();
      node.provider = await renderProviders();
      resolve(node.chat);
    } catch (ex) {
      console.error("Failed to render on load", ex);
      reject(ex);
    }
  });
});
