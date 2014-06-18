/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

module.metadata = {
  'stability': 'unstable'
};


// NOTE: This file should only deal with xul/native tabs


const { Ci } = require('chrome');
const { defer } = require("../lang/functional");
const { windows, isBrowser } = require('../window/utils');
const { isPrivateBrowsingSupported } = require('../self');
const { isGlobalPBSupported } = require('../private-browsing/utils');

// Bug 834961: ignore private windows when they are not supported
function getWindows() windows(null, { includePrivate: isPrivateBrowsingSupported || isGlobalPBSupported });

const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

// Define predicate functions that can be used to detech weather
// we deal with fennec tabs or firefox tabs.

// Predicate to detect whether tab is XUL "Tab" node.
const isXULTab = tab =>
  tab instanceof Ci.nsIDOMNode &&
  tab.nodeName === "tab" &&
  tab.namespaceURI === XUL_NS;
exports.isXULTab = isXULTab;

// Predicate to detecet whether given tab is a fettec tab.
// Unfortunately we have to guess via duck typinng of:
// http://mxr.mozilla.org/mozilla-central/source/mobile/android/chrome/content/browser.js#2583
const isFennecTab = tab =>
  tab &&
  tab.QueryInterface &&
  Ci.nsIBrowserTab &&
  tab.QueryInterface(Ci.nsIBrowserTab) === tab;
exports.isFennecTab = isFennecTab;

const isTab = x => isXULTab(x) || isFennecTab(x);
exports.isTab = isTab;

function activateTab(tab, window) {
  let gBrowser = getTabBrowserForTab(tab);

  // normal case
  if (gBrowser) {
    gBrowser.selectedTab = tab;
  }
  // fennec ?
  else if (window && window.BrowserApp) {
    window.BrowserApp.selectTab(tab);
  }
  return null;
}
exports.activateTab = activateTab;

function getTabBrowser(window) {
  // bug 1009938 - may be null in SeaMonkey
  return window.gBrowser || window.getBrowser();
}
exports.getTabBrowser = getTabBrowser;

function getTabContainer(window) {
  return getTabBrowser(window).tabContainer;
}
exports.getTabContainer = getTabContainer;

/**
 * Returns the tabs for the `window` if given, or the tabs
 * across all the browser's windows otherwise.
 *
 * @param {nsIWindow} [window]
 *    A reference to a window
 *
 * @returns {Array} an array of Tab objects
 */
function getTabs(window) {
  if (arguments.length === 0) {
    return getWindows().
               filter(isBrowser).
               reduce((tabs, window) => tabs.concat(getTabs(window)), []);
  }

  // fennec
  if (window.BrowserApp)
    return window.BrowserApp.tabs;

  // firefox - default
  return Array.filter(getTabContainer(window).children, function(t) !t.closing);
}
exports.getTabs = getTabs;

function getActiveTab(window) {
  return getSelectedTab(window);
}
exports.getActiveTab = getActiveTab;

function getOwnerWindow(tab) {
  // normal case
  if (tab.ownerDocument)
    return tab.ownerDocument.defaultView;

  // try fennec case
  return getWindowHoldingTab(tab);
}
exports.getOwnerWindow = getOwnerWindow;

// fennec
function getWindowHoldingTab(rawTab) {
  for each (let window in getWindows()) {
    // this function may be called when not using fennec,
    // but BrowserApp is only defined on Fennec
    if (!window.BrowserApp)
      continue;

    for each (let tab in window.BrowserApp.tabs) {
      if (tab === rawTab)
        return window;
    }
  }

  return null;
}

function openTab(window, url, options) {
  options = options || {};

  // fennec?
  if (window.BrowserApp) {
    return window.BrowserApp.addTab(url, {
      selected: options.inBackground ? false : true,
      pinned: options.isPinned || false,
      isPrivate: options.isPrivate || false
    });
  }

  // firefox
  let newTab = window.gBrowser.addTab(url);
  if (!options.inBackground) {
    activateTab(newTab);
  }
  return newTab;
};
exports.openTab = openTab;

function isTabOpen(tab) {
  // try normal case then fennec case
  return !!((tab.linkedBrowser) || getWindowHoldingTab(tab));
}
exports.isTabOpen = isTabOpen;

function closeTab(tab) {
  let gBrowser = getTabBrowserForTab(tab);
  // normal case?
  if (gBrowser) {
    // Bug 699450: the tab may already have been detached
    if (!tab.parentNode)
      return;
    return gBrowser.removeTab(tab);
  }

  let window = getWindowHoldingTab(tab);
  // fennec?
  if (window && window.BrowserApp) {
    // Bug 699450: the tab may already have been detached
    if (!tab.browser)
      return;
    return window.BrowserApp.closeTab(tab);
  }
  return null;
}
exports.closeTab = closeTab;

function getURI(tab) {
  if (tab.browser) // fennec
    return tab.browser.currentURI.spec;
  return tab.linkedBrowser.currentURI.spec;
}
exports.getURI = getURI;

function getTabBrowserForTab(tab) {
  let outerWin = getOwnerWindow(tab);
  if (outerWin)
    return getOwnerWindow(tab).gBrowser;
  return null;
}
exports.getTabBrowserForTab = getTabBrowserForTab;

function getBrowserForTab(tab) {
  if (tab.browser) // fennec
    return tab.browser;

  return tab.linkedBrowser;
}
exports.getBrowserForTab = getBrowserForTab;

function getTabId(tab) {
  if (tab.browser) // fennec
    return tab.id

  return String.split(tab.linkedPanel, 'panel').pop();
}
exports.getTabId = getTabId;

function getTabForId(id) {
  return getTabs().find(tab => getTabId(tab) === id) || null;
}
exports.getTabForId = getTabForId;

function getTabTitle(tab) {
  return getBrowserForTab(tab).contentDocument.title || tab.label || "";
}
exports.getTabTitle = getTabTitle;

function setTabTitle(tab, title) {
  title = String(title);
  if (tab.browser)
    tab.browser.contentDocument.title = title;
  tab.label = String(title);
}
exports.setTabTitle = setTabTitle;

function getTabContentWindow(tab) {
  return getBrowserForTab(tab).contentWindow;
}
exports.getTabContentWindow = getTabContentWindow;

/**
 * Returns all tabs' content windows across all the browsers' windows
 */
function getAllTabContentWindows() {
  return getTabs().map(getTabContentWindow);
}
exports.getAllTabContentWindows = getAllTabContentWindows;

// gets the tab containing the provided window
function getTabForContentWindow(window) {
  return getTabs().find(tab => getTabContentWindow(tab) === window.top) || null;
}
exports.getTabForContentWindow = getTabForContentWindow;

function getTabURL(tab) {
  if (tab.browser) // fennec
    return String(tab.browser.currentURI.spec);
  return String(getBrowserForTab(tab).currentURI.spec);
}
exports.getTabURL = getTabURL;

function setTabURL(tab, url) {
  url = String(url);
  if (tab.browser)
    return tab.browser.loadURI(url);
  return getBrowserForTab(tab).loadURI(url);
}
// "TabOpen" event is fired when it's still "about:blank" is loaded in the
// changing `location` property of the `contentDocument` has no effect since
// seems to be either ignored or overridden by internal listener, there for
// location change is enqueued for the next turn of event loop.
exports.setTabURL = defer(setTabURL);

function getTabContentType(tab) {
  return getBrowserForTab(tab).contentDocument.contentType;
}
exports.getTabContentType = getTabContentType;

function getSelectedTab(window) {
  if (window.BrowserApp) // fennec?
    return window.BrowserApp.selectedTab;
  if (window.gBrowser)
    return window.gBrowser.selectedTab;
  return null;
}
exports.getSelectedTab = getSelectedTab;


function getTabForBrowser(browser) {
  for each (let window in getWindows()) {
    // this function may be called when not using fennec
    if (!window.BrowserApp)
      continue;

    for each (let tab in window.BrowserApp.tabs) {
      if (tab.browser === browser)
        return tab;
    }
  }
  return null;
}
exports.getTabForBrowser = getTabForBrowser;

function pin(tab) {
  let gBrowser = getTabBrowserForTab(tab);
  // TODO: Implement Fennec support
  if (gBrowser) gBrowser.pinTab(tab);
}
exports.pin = pin;

function unpin(tab) {
  let gBrowser = getTabBrowserForTab(tab);
  // TODO: Implement Fennec support
  if (gBrowser) gBrowser.unpinTab(tab);
}
exports.unpin = unpin;

function isPinned(tab) !!tab.pinned
exports.isPinned = isPinned;

function reload(tab) {
  let gBrowser = getTabBrowserForTab(tab);
  // Firefox
  if (gBrowser) gBrowser.unpinTab(tab);
  // Fennec
  else if (tab.browser) tab.browser.reload();
}
exports.reload = reload

function getIndex(tab) {
  let gBrowser = getTabBrowserForTab(tab);
  // Firefox
  if (gBrowser) {
    let document = getBrowserForTab(tab).contentDocument;
    return gBrowser.getBrowserIndexForDocument(document);
  }
  // Fennec
  else {
    let window = getWindowHoldingTab(tab)
    let tabs = window.BrowserApp.tabs;
    for (let i = tabs.length; i >= 0; i--)
      if (tabs[i] === tab) return i;
  }
}
exports.getIndex = getIndex;

function move(tab, index) {
  let gBrowser = getTabBrowserForTab(tab);
  // Firefox
  if (gBrowser) gBrowser.moveTabTo(tab, index);
  // TODO: Implement fennec support
}
exports.move = move;
