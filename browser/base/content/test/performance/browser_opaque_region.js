/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_opaque_region() {
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
});
