/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var { Toolbox } = require("resource://devtools/client/framework/toolbox.js");

// Test that a fullscreen page allows DevTools to be seen.

const URL = "data:text/html;charset=utf-8,Fullscreen me";

add_task(async function test_fullscreen_docked_toolbox() {
  const tab = await addTab(URL);

  ok(!window.fullScreen, "Should not be fullscreen");

  await new Promise(r => {
    window.addEventListener("fullscreenchange", r, { once: true });
    SpecialPowers.spawn(tab.linkedBrowser, [], () => {
      content.document.documentElement.requestFullscreen();
    });
  });

  ok(window.fullScreen, "Should be fullscreen");

  const toolbox = await gDevTools.showToolboxForTab(tab);
  isnot(
    toolbox.hostType,
    Toolbox.HostType.WINDOW,
    "Toolbox is docked in the main window"
  );

  const tabRect = tab.linkedBrowser.getBoundingClientRect();
  const devToolsRect =
    toolbox.win.browsingContext.embedderElement.getBoundingClientRect();

  Assert.lessOrEqual(
    tabRect.bottom,
    devToolsRect.top,
    "DevTools shouldn't intersect the browser"
  );

  await toolbox.destroy();

  gBrowser.removeCurrentTab();
});
