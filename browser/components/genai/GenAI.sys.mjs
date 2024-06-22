/**
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  ASRouterTargeting: "resource:///modules/asrouter/ASRouterTargeting.sys.mjs",
});
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "chatEnabled",
  "browser.ml.chat.enabled"
);
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "chatHideLocalhost",
  "browser.ml.chat.hideLocalhost"
);
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "chatOpenSidebarOnProviderChange",
  "browser.ml.chat.openSidebarOnProviderChange",
  true
);
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "chatPromptPrefix",
  "browser.ml.chat.prompt.prefix"
);
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "chatProvider",
  "browser.ml.chat.provider",
  null,
  (_pref, _old, val) => onChatProviderChange(val)
);
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "chatSidebar",
  "browser.ml.chat.sidebar"
);

export const GenAI = {
  // Any chat provider can be used and those that match the URLs in this object
  // will allow for additional UI shown such as populating dropdown with a name,
  // showing links, and other special behaviors needed for individual providers.
  // The ordering of this list affects UI and currently alphabetical by name.
  chatProviders: new Map([
    // Until bug 1903900 to better handle max length issues, track in comments
    // 8k max length uri before 414
    [
      "http://localhost:8080",
      {
        get hidden() {
          return lazy.chatHideLocalhost;
        },
        id: "localhost",
        name: "localhost",
      },
    ],
  ]),

  /**
   * Handle startup tasks like telemetry, adding listeners.
   */
  init() {
    Glean.genaiChatbot.enabled.set(lazy.chatEnabled);
    Glean.genaiChatbot.provider.set(this.getProviderId());
    Glean.genaiChatbot.sidebar.set(lazy.chatSidebar);

    // Access this getter for its side effect of observing provider pref change
    lazy.chatProvider;

    // Detect about:preferences to add controls
    Services.obs.addObserver(this, "experimental-pane-loaded");
  },

  /**
   * Convert provider to id.
   *
   * @param {string} provider url defaulting to current pref
   * @returns {string} id or custom or none
   */
  getProviderId(provider = lazy.chatProvider) {
    const { id } = this.chatProviders.get(provider) ?? {};
    return id ?? (provider ? "custom" : "none");
  },

  /**
   * Build prompts menu to ask chat for context menu or popup.
   *
   * @param {MozMenu} menu element to update
   * @param {nsContextMenu} nsContextMenu helpers for context menu
   */
  async buildAskChatMenu(menu, nsContextMenu) {
    nsContextMenu.showItem(menu, false);
    if (!lazy.chatEnabled || lazy.chatProvider == "") {
      return;
    }
    menu.label = `Ask ${
      this.chatProviders.get(lazy.chatProvider)?.name ?? "Your Chatbot"
    }`;
    menu.menupopup?.remove();

    // Prepare context used for both targeting and handling prompts
    const window = menu.ownerGlobal;
    const tab = window.gBrowser.getTabForBrowser(nsContextMenu.browser);
    const context = {
      provider: lazy.chatProvider,
      selection: nsContextMenu.selectionInfo.fullText ?? "",
      tabTitle: (tab._labelIsContentTitle && tab.label) || "",
      window,
    };

    // Add menu items that pass along context for handling
    (await this.getContextualPrompts(context)).forEach(promptObj =>
      menu
        .appendItem(promptObj.label, promptObj.value)
        .addEventListener("command", () =>
          this.handleAskChat(promptObj, context)
        )
    );
    nsContextMenu.showItem(menu, menu.itemCount > 0);
  },

  /**
   * Get prompts from prefs evaluated with context
   *
   * @param {object} context data used for targeting
   * @returns {promise} array of matching prompt objects
   */
  getContextualPrompts(context) {
    // Treat prompt objects as messages to reuse targeting capabilities
    const messages = [];
    Services.prefs.getChildList("browser.ml.chat.prompts.").forEach(pref => {
      try {
        const promptObj = {
          label: Services.prefs.getStringPref(pref),
          targeting: "true",
          value: "",
        };
        try {
          // Prompts can be JSON with label, value, targeting and other keys
          Object.assign(promptObj, JSON.parse(promptObj.label));
        } catch (ex) {}
        messages.push(promptObj);
      } catch (ex) {
        console.error("Failed to get prompt pref " + pref, ex);
      }
    });
    return lazy.ASRouterTargeting.findMatchingMessage({
      messages,
      returnAll: true,
      trigger: { context },
    });
  },

  /**
   * Build a prompt with context.
   *
   * @param {MozMenuItem} item Use value falling back to label
   * @param {object} context Placeholder keys with values to replace
   * @returns {string} Prompt with placeholders replaced
   */
  buildChatPrompt(item, context = {}) {
    // Combine prompt prefix with the item then replace placeholders from the
    // original prompt (and not from context)
    return (lazy.chatPromptPrefix + (item.value || item.label)).replace(
      // Handle %placeholder% as key|options
      /\%(\w+)(?:\|([^%]+))?\%/g,
      (placeholder, key, options) =>
        // Currently only supporting numeric options for slice with `undefined`
        // resulting in whole string
        context[key]?.slice(0, options) ?? placeholder
    );
  },

  /**
   * Handle selected prompt by opening tab or sidebar.
   *
   * @param {object} promptObj to convert to string
   * @param {object} context of how the prompt should be handled
   */
  async handleAskChat(promptObj, context) {
    Glean.genaiChatbot.contextmenuPromptClick.record({
      prompt: promptObj.id ?? "custom",
      provider: this.getProviderId(),
      selection: context.selection?.length ?? 0,
    });

    const prompt = this.buildChatPrompt(promptObj, context);

    // Pass the prompt via GET url ?q= param or request header
    const { header } = this.chatProviders.get(lazy.chatProvider) ?? {};
    const url = new URL(lazy.chatProvider);
    const options = {
      inBackground: false,
      relatedToCurrent: true,
      triggeringPrincipal: Services.scriptSecurityManager.createNullPrincipal(
        {}
      ),
    };
    if (header) {
      options.headers = Cc[
        "@mozilla.org/io/string-input-stream;1"
      ].createInstance(Ci.nsIStringInputStream);
      options.headers.data = `${header}: ${encodeURIComponent(prompt)}\r\n`;
    } else {
      url.searchParams.set("q", prompt);
    }

    // Get the desired browser to handle the prompt url request
    let browser;
    if (lazy.chatSidebar) {
      const { SidebarController } = context.window;
      await SidebarController.show("viewGenaiChatSidebar");
      browser = await SidebarController.browser.contentWindow.browserPromise;
    } else {
      browser = context.window.gBrowser.addTab("", options).linkedBrowser;
    }
    browser.fixupAndLoadURIString(url, options);
  },

  /**
   * Build preferences for chat such as handling providers.
   *
   * @param {Window} window for about:preferences
   */
  buildPreferences({ document, Preferences }) {
    const providerEl = document.getElementById("genai-chat-provider");
    if (!providerEl) {
      return;
    }

    const enabled = Preferences.get("browser.ml.chat.enabled");
    const onEnabledChange = () => {
      providerEl.disabled = !enabled.value;

      // Update enabled telemetry
      Glean.genaiChatbot.enabled.set(enabled.value);
      if (onEnabledChange.canChange) {
        Glean.genaiChatbot.experimentCheckboxClick.record({
          enabled: enabled.value,
        });
      }
      onEnabledChange.canChange = true;
    };
    onEnabledChange();
    enabled.on("change", onEnabledChange);

    // Populate providers and hide from list if necessary
    this.chatProviders.forEach((data, url) => {
      providerEl.appendItem(data.name, url).hidden = data.hidden ?? false;
    });
    const provider = Preferences.add({
      id: "browser.ml.chat.provider",
      type: "string",
    });
    let customItem;
    const onProviderChange = () => {
      // Add/update the Custom entry if it's not a default provider entry
      if (provider.value && !this.chatProviders.has(provider.value)) {
        if (!customItem) {
          customItem = providerEl.appendItem();
        }
        customItem.label = `Custom (${provider.value})`;
        customItem.value = provider.value;

        // Select the item if the preference changed not via menu
        providerEl.selectedItem = customItem;
      }

      // Update potentially multiple links for the provider
      const links = document.getElementById("genai-chat-links");
      const providerData = this.chatProviders.get(provider.value);
      for (let i = 1; i <= 2; i++) {
        const name = `link${i}`;
        let link = links.querySelector(`[data-l10n-name=${name}]`);
        const href = providerData?.[name];
        if (href) {
          if (!link) {
            link = links.appendChild(document.createElement("a"));
            link.dataset.l10nName = name;
            link.target = "_blank";
          }
          link.href = href;
        } else {
          link?.remove();
        }
      }
      document.l10n.setAttributes(
        links,
        providerData?.linksId ?? "genai-settings-chat-links"
      );

      // Update provider telemetry
      const providerId = this.getProviderId(provider.value);
      Glean.genaiChatbot.provider.set(providerId);
      if (onProviderChange.lastId && document.hasFocus()) {
        Glean.genaiChatbot.providerChange.record({
          current: providerId,
          previous: onProviderChange.lastId,
          surface: "settings",
        });
      }
      onProviderChange.lastId = providerId;
    };
    onProviderChange();
    provider.on("change", onProviderChange);
  },

  // nsIObserver
  observe(window) {
    this.buildPreferences(window);
  },
};

/**
 * Ensure the chat sidebar is shown to reflect changed provider.
 *
 * @param {string} value New pref value
 */
function onChatProviderChange(value) {
  if (value && lazy.chatEnabled && lazy.chatOpenSidebarOnProviderChange) {
    Services.wm
      .getMostRecentWindow("navigator:browser")
      ?.SidebarController.show("viewGenaiChatSidebar");
  }
}
