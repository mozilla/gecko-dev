/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  AddonManager: "resource://gre/modules/AddonManager.sys.mjs",
});

async function assert_opaque_region() {
  // Ensure we've painted.
  await new Promise(r => requestAnimationFrame(() => requestAnimationFrame(r)));

  let contentRect = document
    .getElementById("tabbrowser-tabbox")
    .getBoundingClientRect();
  let opaqueRegion = window.windowUtils.getWidgetOpaqueRegion();

  info(`Got opaque region: ${JSON.stringify(opaqueRegion)}`);
  isnot(opaqueRegion.length, 0, "Should have some part of the window opaque");

  let anyContainsContentRect = false;
  let containsContentRect = opaqueRect => {
    return (
      opaqueRect.x <= contentRect.x &&
      opaqueRect.y <= contentRect.y &&
      opaqueRect.width >= contentRect.width &&
      opaqueRect.height >= contentRect.height
    );
  };

  for (let opaqueRect of opaqueRegion) {
    anyContainsContentRect |= containsContentRect(opaqueRect);
  }

  ok(
    anyContainsContentRect,
    "The browser area should be considered opaque by widget"
  );
}

registerCleanupFunction(async function () {
  let defaultTheme = await lazy.AddonManager.getAddonByID(
    "default-theme@mozilla.org"
  );
  await defaultTheme.enable();
});

add_task(async function assert_opaque_region_system_theme() {
  return assert_opaque_region();
});

add_task(async function assert_opaque_region_mica() {
  await SpecialPowers.pushPrefEnv({ set: [["widget.windows.mica", true]] });
  return assert_opaque_region();
});

add_task(async function assert_opaque_region_light_theme() {
  let lightTheme = await lazy.AddonManager.getAddonByID(
    "firefox-compact-light@mozilla.org"
  );
  await lightTheme.enable();
  return assert_opaque_region();
});

add_task(async function assert_opaque_region_dark_theme() {
  let lightTheme = await lazy.AddonManager.getAddonByID(
    "firefox-compact-dark@mozilla.org"
  );
  await lightTheme.enable();
  return assert_opaque_region();
});
