/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/remote-page */

import { html } from "chrome://global/content/vendor/lit.all.mjs";
import { MozLitElement } from "chrome://global/content/lit-utils.mjs";

// eslint-disable-next-line import/no-unassigned-import
import "chrome://browser/content/shopping/shopping-card.mjs";
// eslint-disable-next-line import/no-unassigned-import
import "chrome://global/content/elements/moz-button-group.mjs";
// eslint-disable-next-line import/no-unassigned-import
import "chrome://global/content/elements/moz-button.mjs";

/* Until caching is implemented (see Bug 1927956), any location change will force another
 * update and refresh the UI. Furthermore, we probably want to keep the card visible for the
 * same tab until the user takes action. To prevent the card from being hidden unnecessarily
 * on location change, and to ensure users have a chance to read the message,
 * change the pref once the user dismisses the card or opens the sidebar settings panel. */
const HAS_SEEN_NEW_POSITION_NOTIFICATION_CARD_PREF =
  "browser.shopping.experience2023.newPositionCard.hasSeen";

class NewPositionNotificationCard extends MozLitElement {
  static properties = {
    isSidebarStartPosition: { type: Boolean },
  };

  static get queries() {
    return {
      imgEl: "#notification-card-img",
      settingsLinkEl: "#notification-card-settings-link",
      moveLeftButtonEl: "#notification-card-move-left-button",
      moveRightButtonEl: "#notification-card-move-right-button",
      dismissButtonEl: "#notification-card-dismiss-button",
    };
  }

  firstUpdated() {
    super.firstUpdated();
    Glean.shopping.surfaceNotificationCardImpression.record();
  }

  handleClickSettingsLink(event) {
    event.preventDefault();
    // Click event listener references get lost if attached to the settings link since it is slotted into the shopping-card.
    // As a workaround, attach the listener to its parent element and only dispatch events if the target is the settings link.
    if (event.target == this.settingsLinkEl) {
      Glean.shopping.surfaceNotificationCardSidebarSettingsClicked.record();
      window.dispatchEvent(
        new CustomEvent("ShowSidebarSettings", {
          bubbles: true,
          composed: true,
        })
      );
      RPMSetPref(HAS_SEEN_NEW_POSITION_NOTIFICATION_CARD_PREF, true);
      window.dispatchEvent(
        new CustomEvent("HideNewPositionCard", {
          bubbles: true,
          composed: true,
        })
      );
    }
  }

  handleClickPositionButton() {
    if (this.isSidebarStartPosition) {
      Glean.shopping.surfaceNotificationCardMoveRightClicked.record();
      this.isSidebarStartPosition = false;
      window.dispatchEvent(
        new CustomEvent("MoveSidebarToRight", {
          bubbles: true,
          composed: true,
        })
      );
    } else {
      Glean.shopping.surfaceNotificationCardMoveLeftClicked.record();
      this.isSidebarStartPosition = true;
      window.dispatchEvent(
        new CustomEvent("MoveSidebarToLeft", {
          bubbles: true,
          composed: true,
        })
      );
    }
  }

  handleClickDismissButton() {
    Glean.shopping.surfaceNotificationCardDismissClicked.record();
    this.dispatchEvent(
      new CustomEvent("HideNewPositionCard", {
        bubbles: true,
        composed: true,
      })
    );
    RPMSetPref(HAS_SEEN_NEW_POSITION_NOTIFICATION_CARD_PREF, true);
    window.dispatchEvent(
      new CustomEvent("HideNewPositionCard", { bubbles: true, composed: true })
    );
  }

  render() {
    return html`
      <link
        rel="stylesheet"
        href="chrome://browser/content/shopping/new-position-notification-card.css"
      />
      <shopping-card>
        <div id="notification-card-wrapper" slot="content">
          <img
            id="notification-card-img"
            src="chrome://browser/content/shopping/assets/emptyStateB.svg"
            alt=""
            role="presentation"
          />
          <h2
            id="notification-card-header"
            data-l10n-id="shopping-integrated-new-position-notification-title"
          ></h2>
          <p
            id="notification-card-body"
            data-l10n-id="shopping-integrated-new-position-notification-subtitle"
            @click=${this.handleClickSettingsLink}
          >
            <a
              id="notification-card-settings-link"
              data-l10n-name="sidebar_settings"
              href="#"
            ></a>
          </p>
          <div id="notification-card-button-group">
            <moz-button
              id=${this.isSidebarStartPosition
                ? "notification-card-move-right-button"
                : "notification-card-move-left-button"}
              data-l10n-id=${this.isSidebarStartPosition
                ? "shopping-integrated-new-position-notification-move-right-button"
                : "shopping-integrated-new-position-notification-move-left-button"}
              type="primary"
              size="small"
              @click=${this.handleClickPositionButton}
            >
            </moz-button>
            <moz-button
              id="notification-card-dismiss-button"
              data-l10n-id="shopping-integrated-new-position-notification-dismiss-button"
              type="ghost"
              size="small"
              @click=${this.handleClickDismissButton}
            >
            </moz-button>
          </div>
        </div>
      </shopping-card>
    `;
  }
}

customElements.define(
  "new-position-notification-card",
  NewPositionNotificationCard
);
