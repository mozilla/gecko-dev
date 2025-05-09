/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.sys.mjs",
  RemoteL10n: "resource:///modules/asrouter/RemoteL10n.sys.mjs",
  SpecialMessageActions:
    "resource://messaging-system/lib/SpecialMessageActions.sys.mjs",
});

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
   * Show the infobar notification and send an impression ping
   *
   * @param {object} browser Browser reference for the currently selected tab
   */
  async showNotification(browser) {
    let { content } = this.message;
    let { gBrowser } = browser.ownerGlobal;
    let doc = gBrowser.ownerDocument;
    let notificationContainer;
    if (content.type === "global") {
      notificationContainer = browser.ownerGlobal.gNotificationBox;
    } else {
      notificationContainer = gBrowser.getNotificationBox(browser);
    }

    let priority = content.priority || notificationContainer.PRIORITY_SYSTEM;

    this.notification = await notificationContainer.appendNotification(
      this.message.id,
      {
        label: this.formatMessageConfig(doc, browser, content.text),
        image: content.icon || "chrome://branding/content/icon64.png",
        priority,
        eventCallback: this.infobarCallback,
      },
      content.buttons.map(b => this.formatButtonConfig(b)),
      false,
      content.dismissable
    );

    this.addImpression();
  }

  formatMessageConfig(doc, browser, content) {
    const frag = doc.createDocumentFragment();
    const parts = Array.isArray(content) ? content : [content];
    for (const part of parts) {
      let node;
      if (typeof part === "string") {
        node = doc.createTextNode(part);
        // Handle embedded link
      } else if (part.href) {
        const a = doc.createElement("a");
        a.href = part.href;
        a.addEventListener("click", e => {
          e.preventDefault();
          lazy.SpecialMessageActions.handleAction(
            { type: "OPEN_URL", data: { args: a.href, where: part.where } },
            browser
          );
        });

        if (part.string_id) {
          const l10n = lazy.RemoteL10n.createElement(doc, "span", {
            content: {
              string_id: part.string_id,
              ...(part.args && { args: part.args }),
            },
          });
          a.appendChild(l10n);
        } else {
          a.textContent = part.raw || "";
        }
        node = a;
      } else if (part.string_id) {
        node = lazy.RemoteL10n.createElement(doc, "span", {
          content: {
            string_id: part.string_id,
            ...(part.args && { args: part.args }),
          },
        });
      } else {
        const text = part.raw !== null ? part.raw : String(part);
        node = doc.createTextNode(text);
      }

      frag.appendChild(node);
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
   * Called when one of the infobar buttons is clicked
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
  }

  dispatchUserAction(action, selectedBrowser) {
    this._dispatch({ type: "USER_ACTION", data: action }, selectedBrowser);
  }

  /**
   * Called when interacting with the toolbar (but not through the buttons)
   */
  infobarCallback(eventType) {
    if (eventType === "removed") {
      this.notification = null;
      // eslint-disable-next-line no-use-before-define
      InfoBar._activeInfobar = null;
    } else if (this.notification) {
      this.sendUserEventTelemetry("DISMISSED");
      this.notification = null;
      // eslint-disable-next-line no-use-before-define
      InfoBar._activeInfobar = null;
    }
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

  maybeLoadCustomElement(win) {
    if (!win.customElements.get("remote-text")) {
      Services.scriptloader.loadSubScript(
        "chrome://browser/content/asrouter/components/remote-text.js",
        win
      );
    }
  },

  maybeInsertFTL(win) {
    win.MozXULElement.insertFTLIfNeeded("browser/newtab/asrouter.ftl");
    win.MozXULElement.insertFTLIfNeeded(
      "browser/defaultBrowserNotification.ftl"
    );
  },

  async showInfoBarMessage(browser, message, dispatch) {
    // Prevent stacking multiple infobars
    if (this._activeInfobar) {
      return null;
    }

    const win = browser?.ownerGlobal;

    if (!win || lazy.PrivateBrowsingUtils.isWindowPrivate(win)) {
      return null;
    }

    this.maybeLoadCustomElement(win);
    this.maybeInsertFTL(win);

    let notification = new InfoBarNotification(message, dispatch);
    await notification.showNotification(browser);
    this._activeInfobar = true;

    return notification;
  },
};
