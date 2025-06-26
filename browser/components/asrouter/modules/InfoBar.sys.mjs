/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-disable no-use-before-define */
const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.sys.mjs",
  RemoteL10n: "resource:///modules/asrouter/RemoteL10n.sys.mjs",
  SpecialMessageActions:
    "resource://messaging-system/lib/SpecialMessageActions.sys.mjs",
});

const TYPES = {
  UNIVERSAL: "universal",
  GLOBAL: "global",
};

const FTL_FILES = [
  "browser/newtab/asrouter.ftl",
  "browser/defaultBrowserNotification.ftl",
  "preview/termsOfUse.ftl",
];

class InfoBarNotification {
  constructor(message, dispatch) {
    this._dispatch = dispatch;
    this.dispatchUserAction = this.dispatchUserAction.bind(this);
    this.buttonCallback = this.buttonCallback.bind(this);
    this.infobarCallback = this.infobarCallback.bind(this);
    this.message = message;
    this.notification = null;
  }

  /**
   * Ensure a hidden container of <a data-l10n-name> templates exists, and
   * inject the request links using hrefs from message.content.linkUrls.
   */
  _ensureLinkTemplatesFor(doc, names) {
    let container = doc.getElementById("infobar-link-templates");
    // We inject a hidden <div> of <a data-l10n-name> templates into the
    // document because Fluent’s DOM-overlay scans the page for those
    // placeholders.
    if (!container) {
      container = doc.createElement("div");
      container.id = "infobar-link-templates";
      container.hidden = true;
      doc.body.appendChild(container);
    }

    const linkUrls = this.message.content.linkUrls || {};
    for (let name of names) {
      if (!container.querySelector(`a[data-l10n-name="${name}"]`)) {
        const a = doc.createElement("a");
        a.dataset.l10nName = name;
        a.href = linkUrls[name];
        container.appendChild(a);
      }
    }
  }

  /**
   * Async helper to render a Fluent string. If the translation contains `<a
   * data-l10n-name>`, it will parse and inject the associated link contained
   * in the message.
   */
  async _buildMessageFragment(doc, browser, stringId, args) {
    // Get the raw HTML translation
    const html = await lazy.RemoteL10n.formatLocalizableText({
      string_id: stringId,
      ...(args && { args }),
    });

    // If no inline anchors, just return a span
    if (!html.includes('data-l10n-name="')) {
      return lazy.RemoteL10n.createElement(doc, "span", {
        content: { string_id: stringId, ...(args && { args }) },
      });
    }

    // Otherwise parse it and set up a fragment
    const temp = new DOMParser().parseFromString(html, "text/html").body;
    const frag = doc.createDocumentFragment();

    // Prepare <a data-l10n-name> templates
    const names = [...temp.querySelectorAll("a[data-l10n-name]")].map(
      a => a.dataset.l10nName
    );
    this._ensureLinkTemplatesFor(doc, names);

    // Import each node and wire up any anchors it contains
    for (const node of temp.childNodes) {
      // Nodes from DOMParser belong to a different document, so importNode()
      // clones them into our target doc
      const importedNode = doc.importNode(node, true);

      if (importedNode.nodeType === Node.ELEMENT_NODE) {
        // collect this node if it's an anchor, and all child anchors
        const anchors = [];
        if (importedNode.matches("a[data-l10n-name]")) {
          anchors.push(importedNode);
        }
        anchors.push(...importedNode.querySelectorAll("a[data-l10n-name]"));

        for (const a of anchors) {
          const name = a.dataset.l10nName;
          const template = doc
            .getElementById("infobar-link-templates")
            .querySelector(`a[data-l10n-name="${name}"]`);
          if (!template) {
            continue;
          }
          a.href = template.href;
          a.addEventListener("click", e => {
            e.preventDefault();
            lazy.SpecialMessageActions.handleAction(
              { type: "OPEN_URL", data: { args: a.href, where: "tab" } },
              browser
            );
          });
        }
      }

      frag.appendChild(importedNode);
    }

    return frag;
  }

  /**
   * Show the infobar notification and send an impression ping
   *
   * @param {object} browser Browser reference for the currently selected tab
   */
  async showNotification(browser) {
    let { content } = this.message;
    let { gBrowser } = browser.ownerGlobal;
    let doc = gBrowser.ownerDocument;
    let notificationContainer;
    if ([TYPES.GLOBAL, TYPES.UNIVERSAL].includes(content.type)) {
      notificationContainer = browser.ownerGlobal.gNotificationBox;
    } else {
      notificationContainer = gBrowser.getNotificationBox(browser);
    }

    let priority = content.priority || notificationContainer.PRIORITY_SYSTEM;

    let labelNode = await this.formatMessageConfig(doc, browser, content.text);

    this.notification = await notificationContainer.appendNotification(
      this.message.id,
      {
        label: labelNode,
        image: content.icon || "chrome://branding/content/icon64.png",
        priority,
        eventCallback: this.infobarCallback,
      },
      content.buttons.map(b => this.formatButtonConfig(b)),
      false,
      content.dismissable
    );
    // If InfoBar is universal, only record an impression for the first
    // instance.
    if (
      content.type !== TYPES.UNIVERSAL ||
      !InfoBar._universalInfobars.length
    ) {
      this.addImpression();
    }

    if (content.type === TYPES.UNIVERSAL) {
      InfoBar._universalInfobars.push({
        box: browser.ownerGlobal.gNotificationBox,
        notification: this.notification,
      });
    }
  }

  _createLinkNode(doc, browser, { href, where = "tab", string_id, args, raw }) {
    const a = doc.createElement("a");
    a.href = href;
    a.addEventListener("click", e => {
      e.preventDefault();
      lazy.SpecialMessageActions.handleAction(
        { type: "OPEN_URL", data: { args: a.href, where } },
        browser
      );
    });

    if (string_id) {
      // wrap a localized span inside
      const span = lazy.RemoteL10n.createElement(doc, "span", {
        content: { string_id, ...(args && { args }) },
      });
      a.appendChild(span);
    } else {
      a.textContent = raw || "";
    }

    return a;
  }

  async formatMessageConfig(doc, browser, content) {
    const frag = doc.createDocumentFragment();
    const parts = Array.isArray(content) ? content : [content];

    for (const part of parts) {
      if (!part) {
        continue;
      }
      if (part.href) {
        frag.appendChild(this._createLinkNode(doc, browser, part));
        continue;
      }

      if (part.string_id) {
        const subFrag = await this._buildMessageFragment(
          doc,
          browser,
          part.string_id,
          part.args
        );
        frag.appendChild(subFrag);
        continue;
      }

      if (typeof part === "string") {
        frag.appendChild(doc.createTextNode(part));
        continue;
      }

      if (part.raw && typeof part.raw === "string") {
        frag.appendChild(doc.createTextNode(part.raw));
      }
    }

    return frag;
  }

  formatButtonConfig(button) {
    let btnConfig = { callback: this.buttonCallback, ...button };
    // notificationbox will set correct data-l10n-id attributes if passed in
    // using the l10n-id key. Otherwise the `button.label` text is used.
    if (button.label.string_id) {
      btnConfig["l10n-id"] = button.label.string_id;
    }

    return btnConfig;
  }

  addImpression() {
    // Record an impression in ASRouter for frequency capping
    this._dispatch({ type: "IMPRESSION", data: this.message });
    // Send a user impression telemetry ping
    this.sendUserEventTelemetry("IMPRESSION");
  }

  /**
   * Callback fired when a button in the infobar is clicked.
   *
   * @param {Element} notificationBox - The `<notification-message>` element representing the infobar.
   * @param {Object} btnDescription - An object describing the button, includes the label, the action with an optional dismiss property, and primary button styling.
   * @param {Element} target - The <button> DOM element that was clicked.
   * @returns {boolean} Returns `false` to dismiss the infobar or `true` to keep it open.
   */
  buttonCallback(notificationBox, btnDescription, target) {
    this.dispatchUserAction(
      btnDescription.action,
      target.ownerGlobal.gBrowser.selectedBrowser
    );
    let isPrimary = target.classList.contains("primary");
    let eventName = isPrimary
      ? "CLICK_PRIMARY_BUTTON"
      : "CLICK_SECONDARY_BUTTON";
    this.sendUserEventTelemetry(eventName);

    // Prevent dismissal if dismiss property is set to 'false'
    if (btnDescription.action?.dismiss === false) {
      return true;
    }

    // Default, dismisses the Infobar
    return false;
  }

  dispatchUserAction(action, selectedBrowser) {
    this._dispatch({ type: "USER_ACTION", data: action }, selectedBrowser);
  }

  /**
   * Called when interacting with the toolbar (but not through the buttons)
   */
  infobarCallback(eventType) {
    const wasUniversal =
      InfoBar._activeInfobar?.message.content.type === TYPES.UNIVERSAL;
    if (eventType === "removed") {
      this.notification = null;
      InfoBar._activeInfobar = null;
    } else if (this.notification) {
      this.sendUserEventTelemetry("DISMISSED");
      this.notification = null;
      InfoBar._activeInfobar = null;
    }
    // If one instance of universal infobar is removed, remove all instances and
    // the new window observer
    if (wasUniversal) {
      this.removeUniversalInfobars();
    }
  }

  removeUniversalInfobars() {
    try {
      Services.obs.removeObserver(InfoBar, "domwindowopened");
    } catch (error) {
      console.error(
        "Error removing domwindowopened observer on InfoBar:",
        error
      );
    }
    InfoBar._universalInfobars.forEach(({ box, notification }) => {
      box.removeNotification(notification);
    });
    InfoBar._universalInfobars = [];
  }

  sendUserEventTelemetry(event) {
    const ping = {
      message_id: this.message.id,
      event,
    };
    this._dispatch({
      type: "INFOBAR_TELEMETRY",
      data: { action: "infobar_user_event", ...ping },
    });
  }
}

export const InfoBar = {
  _activeInfobar: null,
  _universalInfobars: [],

  maybeLoadCustomElement(win) {
    if (!win.customElements.get("remote-text")) {
      Services.scriptloader.loadSubScript(
        "chrome://browser/content/asrouter/components/remote-text.js",
        win
      );
    }
  },

  maybeInsertFTL(win) {
    FTL_FILES.forEach(path => win.MozXULElement.insertFTLIfNeeded(path));
  },

  async showNotificationAllWindows(notification) {
    for (let win of Services.wm.getEnumerator(null)) {
      this.maybeLoadCustomElement(win);
      this.maybeInsertFTL(win);
      const browser = win.gBrowser.selectedBrowser;
      await notification.showNotification(browser);
    }
  },

  async showInfoBarMessage(browser, message, dispatch, universalInNewWin) {
    // Prevent stacking multiple infobars
    if (this._activeInfobar && !universalInNewWin) {
      return null;
    }

    // Check if this is the first instance of a universal infobar
    const isFirstUniversal =
      !universalInNewWin && message.content.type === TYPES.UNIVERSAL;
    const win = browser?.ownerGlobal;

    if (!win || lazy.PrivateBrowsingUtils.isWindowPrivate(win)) {
      return null;
    }

    this.maybeLoadCustomElement(win);
    this.maybeInsertFTL(win);

    let notification = new InfoBarNotification(message, dispatch);
    if (isFirstUniversal) {
      await this.showNotificationAllWindows(notification);
      Services.obs.addObserver(this, "domwindowopened");
    } else {
      await notification.showNotification(browser);
    }
    if (!universalInNewWin) {
      this._activeInfobar = { message, dispatch };
      // If the window closes before the user interacts with the active infobar,
      // clear it
      win.addEventListener(
        "unload",
        () => {
          if (InfoBar._activeInfobar?.message === message) {
            InfoBar._activeInfobar = null;
          }
        },
        { once: true }
      );
    }

    return notification;
  },

  observe(aSubject, aTopic) {
    const { message, dispatch } = this._activeInfobar;
    if (
      aTopic !== "domwindowopened" ||
      message?.content.type !== TYPES.UNIVERSAL
    ) {
      return;
    }
    if (aSubject.document.readyState === "complete") {
      let browser = aSubject.gBrowser.selectedBrowser;
      this.showInfoBarMessage(browser, message, dispatch, true);
    } else {
      aSubject.addEventListener(
        "load",
        () => {
          let browser = aSubject.gBrowser.selectedBrowser;
          this.showInfoBarMessage(browser, message, dispatch, true);
        },
        { once: true }
      );
    }
  },
};
