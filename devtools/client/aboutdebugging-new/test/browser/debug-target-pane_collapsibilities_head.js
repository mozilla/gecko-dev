/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// eslint-disable-next-line no-unused-vars
const TARGET_PANES = [
  {
    title: "Temporary Extensions",
    pref: "devtools.aboutdebugging.collapsibilities.temporaryExtension",
  },
  {
    title: "Extensions",
    pref: "devtools.aboutdebugging.collapsibilities.installedExtension",
  },
  {
    title: "Tabs",
    pref: "devtools.aboutdebugging.collapsibilities.tab",
  },
  {
    title: "Service Workers",
    pref: "devtools.aboutdebugging.collapsibilities.serviceWorker",
  },
  {
    title: "Shared Workers",
    pref: "devtools.aboutdebugging.collapsibilities.sharedWorker",
  },
  {
    title: "Other Workers",
    pref: "devtools.aboutdebugging.collapsibilities.otherWorker",
  },
];

// eslint-disable-next-line no-unused-vars
function getDebugTargetPane(title, document) {
  // removes the suffix "(<NUMBER>)" in debug target pane's title, if needed
  const sanitizeTitle = (x) => {
    return x.replace(/\s+\(\d+\)$/, "");
  };

  const targetTitle = sanitizeTitle(title);
  for (const titleEl of document.querySelectorAll(".js-debug-target-pane-title")) {
    if (sanitizeTitle(titleEl.textContent) !== targetTitle) {
      continue;
    }

    return titleEl.closest(".js-debug-target-pane");
  }

  return null;
}

// eslint-disable-next-line no-unused-vars
function prepareCollapsibilitiesTest() {
  // Make all collapsibilities to be expanded.
  for (const { pref } of TARGET_PANES) {
    Services.prefs.setBoolPref(pref, false);
  }
}

// eslint-disable-next-line no-unused-vars
async function toggleCollapsibility(debugTargetPane) {
  debugTargetPane.querySelector(".js-debug-target-pane-title").click();
  // Wait for animation of collapse/expand.
  const animations = debugTargetPane.ownerDocument.getAnimations();
  await Promise.all(animations.map(animation => animation.finished));
}

registerCleanupFunction(() => {
  for (const { pref } of TARGET_PANES) {
    Services.prefs.clearUserPref(pref);
  }
});
