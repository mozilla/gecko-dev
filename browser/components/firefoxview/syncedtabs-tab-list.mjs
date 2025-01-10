/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import {
  classMap,
  html,
  ifDefined,
  when,
} from "chrome://global/content/vendor/lit.all.mjs";
import {
  FxviewTabListBase,
  FxviewTabRowBase,
} from "chrome://browser/content/firefoxview/fxview-tab-list.mjs";
// eslint-disable-next-line import/no-unassigned-import
import "chrome://global/content/elements/moz-button.mjs";

const lazy = {};
let XPCOMUtils;

XPCOMUtils = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
).XPCOMUtils;
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "virtualListEnabledPref",
  "browser.firefox-view.virtual-list.enabled"
);

/**
 * A list of synced tabs that are clickable and able to be remotely closed
 */

export class SyncedTabsTabList extends FxviewTabListBase {
  constructor() {
    super();
  }

  static queries = {
    ...FxviewTabListBase.queries,
    rowEls: {
      all: "syncedtabs-tab-row",
    },
  };

  itemTemplate = (tabItem, i) => {
    return html`
      <syncedtabs-tab-row
        ?active=${i == this.activeIndex}
        .canClose=${ifDefined(tabItem.canClose)}
        .closeRequested=${ifDefined(tabItem.closeRequested)}
        ?compact=${this.compactRows}
        .currentActiveElementId=${this.currentActiveElementId}
        .favicon=${tabItem.icon}
        .fxaDeviceId=${tabItem.fxaDeviceId}
        .primaryL10nId=${tabItem.primaryL10nId}
        .primaryL10nArgs=${ifDefined(tabItem.primaryL10nArgs)}
        .secondaryL10nId=${tabItem.secondaryL10nId}
        .secondaryL10nArgs=${ifDefined(tabItem.secondaryL10nArgs)}
        .tertiaryL10nId=${ifDefined(tabItem.tertiaryL10nId)}
        .tertiaryL10nArgs=${ifDefined(tabItem.tertiaryL10nArgs)}
        .secondaryActionClass=${this.secondaryActionClass}
        .tertiaryActionClass=${ifDefined(tabItem.tertiaryActionClass)}
        .sourceClosedId=${ifDefined(tabItem.sourceClosedId)}
        .sourceWindowId=${ifDefined(tabItem.sourceWindowId)}
        .closedId=${ifDefined(tabItem.closedId || tabItem.closedId)}
        role="listitem"
        .tabElement=${ifDefined(tabItem.tabElement)}
        .title=${tabItem.title}
        .url=${tabItem.url}
        .searchQuery=${ifDefined(this.searchQuery)}
        .hasPopup=${this.hasPopup}
      ></fxview-tab-row>
    `;
  };

  stylesheets() {
    return [
      super.stylesheets(),
      html`<link
        rel="stylesheet"
        href="chrome://browser/content/firefoxview/syncedtabs-tab-list.css"
      />`,
    ];
  }

  render() {
    if (this.searchQuery && !this.tabItems.length) {
      return this.emptySearchResultsTemplate();
    }
    return html`
      ${this.stylesheets()}
      <div
        id="fxview-tab-list"
        class="fxview-tab-list"
        data-l10n-id="firefoxview-tabs"
        role="list"
        @keydown=${this.handleFocusElementInRow}
      >
        ${when(
          lazy.virtualListEnabledPref,
          () => html`
            <virtual-list
              .activeIndex=${this.activeIndex}
              .items=${this.tabItems}
              .template=${this.itemTemplate}
            ></virtual-list>
          `,
          () =>
            html`${this.tabItems.map((tabItem, i) =>
              this.itemTemplate(tabItem, i)
            )}`
        )}
      </div>
      <slot name="menu"></slot>
    `;
  }
}

customElements.define("syncedtabs-tab-list", SyncedTabsTabList);

/**
 * A tab item that displays favicon, title, url, and time of last access
 *
 * @property {boolean} canClose - Whether the tab item has the ability to be closed remotely
 * @property {boolean} closeRequested - Whether the tab has been requested closed but not removed from the list
 * @property {string} fxaDeviceId - The device Id the tab item belongs to, for closing tabs remotely
 */

export class SyncedTabsTabRow extends FxviewTabRowBase {
  constructor() {
    super();
  }

  static properties = {
    ...FxviewTabRowBase.properties,
    canClose: { type: Boolean },
    closeRequested: { type: Boolean },
    fxaDeviceId: { type: String },
  };

  secondaryButtonTemplate() {
    return html`${when(
      this.secondaryL10nId && this.secondaryActionHandler,
      () =>
        html`<moz-button
          type="icon ghost"
          class=${classMap({
            "fxview-tab-row-button": true,
            [this.secondaryActionClass]: this.secondaryActionClass,
          })}
          ?disabled=${this.closeRequested}
          id="fxview-tab-row-secondary-button"
          data-l10n-id=${this.secondaryL10nId}
          data-l10n-args=${ifDefined(this.secondaryL10nArgs)}
          aria-haspopup=${ifDefined(this.hasPopup)}
          @click=${this.secondaryActionHandler}
          tabindex="${this.active &&
          this.currentActiveElementId === "fxview-tab-row-secondary-button"
            ? "0"
            : "-1"}"
        ></moz-button>`
    )}`;
  }

  render() {
    return html`
      ${this.stylesheets()}
      <a
        href=${ifDefined(this.url)}
        class="fxview-tab-row-main"
        id="fxview-tab-row-main"
        disabled=${this.closeRequested}
        tabindex=${this.active &&
        this.currentActiveElementId === "fxview-tab-row-main"
          ? "0"
          : "-1"}
        data-l10n-id=${ifDefined(this.primaryL10nId)}
        data-l10n-args=${ifDefined(this.primaryL10nArgs)}
        @click=${this.primaryActionHandler}
        @keydown=${this.primaryActionHandler}
        title=${!this.primaryL10nId ? this.url : null}
      >
        ${this.faviconTemplate()} ${this.titleTemplate()}
        ${when(
          !this.compact,
          () =>
            html`${this.urlTemplate()} ${this.dateTemplate()}
            ${this.timeTemplate()}`
        )}
      </a>
      ${this.secondaryButtonTemplate()} ${this.tertiaryButtonTemplate()}
    `;
  }
}

customElements.define("syncedtabs-tab-row", SyncedTabsTabRow);
