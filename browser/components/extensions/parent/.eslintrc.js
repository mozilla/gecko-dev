/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

module.exports = {
  extends: "../../../../toolkit/components/extensions/parent/.eslintrc.js",

  globals: {
    // These are defined in browser/components/extensions/parent/ext-browser.js
    Tab: true,
    TabContext: true,
    Window: true,
    clickModifiersFromEvent: true,
    makeWidgetId: true,
    openOptionsPage: true,
    replaceUrlInTab: true,
    tabTracker: true,
    waitForTabLoaded: true,
    windowTracker: true,

    // NOTE: Unlike ext-browser.js (and ext-toolkit.js, ext-tabs-base.js), the
    // files mentioned below are not loaded unconditionally. In practice,
    // because all ext-*.js files share the same global scope, they are likely
    // available when a dependent API is available. Before using these globals,
    // make sure that the dependent module (API) has been loaded, e.g. by only
    // using these globals when you know that an extension is using one of these
    // APIs.

    // This is defined in browser/components/extensions/parent/ext-browserAction.js
    browserActionFor: true,
    // This is defined in browser/components/extensions/parent/ext-menus.js
    actionContextMenu: true,
    // This is defined in browser/components/extensions/parent/ext-devtools.js
    getTargetTabIdForToolbox: true,
    getToolboxEvalOptions: true,
    // This is defined in browser/components/extensions/parent/ext-pageAction.js
    pageActionFor: true,
    // This is defined in browser/components/extensions/parent/ext-sidebarAction.js
    sidebarActionFor: true,
  },
};
