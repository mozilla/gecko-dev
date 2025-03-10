/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/browser-window */

// prettier-ignore
// eslint-disable-next-line no-lone-blocks
{
  Services.scriptloader.loadSubScript("chrome://browser/content/browser-init.js", this);
  Services.scriptloader.loadSubScript("chrome://global/content/contentAreaUtils.js", this);
  Services.scriptloader.loadSubScript("chrome://browser/content/browser-captivePortal.js", this);
  if (AppConstants.MOZ_DATA_REPORTING) {
    Services.scriptloader.loadSubScript("chrome://browser/content/browser-data-submission-info-bar.js", this);
  }
  if (!AppConstants.MOZILLA_OFFICIAL) {
    Services.scriptloader.loadSubScript("chrome://browser/content/browser-development-helpers.js", this);
  }
  Services.scriptloader.loadSubScript("chrome://browser/content/browser-pageActions.js", this);
  Services.scriptloader.loadSubScript("chrome://browser/content/sidebar/browser-sidebar.js", this);
  Services.scriptloader.loadSubScript("chrome://browser/content/browser-customtitlebar.js", this);
  Services.scriptloader.loadSubScript("chrome://browser/content/browser-unified-extensions.js", this);
  Services.scriptloader.loadSubScript("chrome://browser/content/tabbrowser/tab.js", this);
  Services.scriptloader.loadSubScript("chrome://browser/content/tabbrowser/tabbrowser.js", this);
  Services.scriptloader.loadSubScript("chrome://browser/content/tabbrowser/tabgroup.js", this);
  Services.scriptloader.loadSubScript("chrome://browser/content/tabbrowser/tabgroup-menu.js", this);
  Services.scriptloader.loadSubScript("chrome://browser/content/tabbrowser/tabs.js", this);
  Services.scriptloader.loadSubScript("chrome://browser/content/places/places-menupopup.js", this);
  Services.scriptloader.loadSubScript("chrome://browser/content/search/autocomplete-popup.js", this);
  Services.scriptloader.loadSubScript("chrome://browser/content/search/searchbar.js", this);
  Services.scriptloader.loadSubScript("chrome://browser/content/shopping/shopping-sidebar.js", this);
}

window.onload = gBrowserInit.onLoad.bind(gBrowserInit);
window.onunload = gBrowserInit.onUnload.bind(gBrowserInit);
window.onclose = WindowIsClosing;

window.addEventListener(
  "MozBeforeInitialXULLayout",
  gBrowserInit.onBeforeInitialXULLayout.bind(gBrowserInit),
  { once: true }
);

// The listener of DOMContentLoaded must be set on window, rather than
// document, because the window can go away before the event is fired.
// In that case, we don't want to initialize anything, otherwise we
// may be leaking things because they will never be destroyed after.
window.addEventListener(
  "DOMContentLoaded",
  gBrowserInit.onDOMContentLoaded.bind(gBrowserInit),
  { once: true }
);
