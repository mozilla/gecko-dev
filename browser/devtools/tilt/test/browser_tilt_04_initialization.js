/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

function test() {
  if (!isTiltEnabled()) {
    aborting();
    info("Skipping initialization test because Tilt isn't enabled.");
    return;
  }
  if (!isWebGLSupported()) {
    aborting();
    info("Skipping initialization test because WebGL isn't supported.");
    return;
  }

  waitForExplicitFinish();

  createTab(function() {
    let id = TiltUtils.getWindowId(gBrowser.selectedBrowser.contentWindow);

    is(id, Tilt.currentWindowId,
      "The unique window identifiers should match for the same window.");

    createTilt({
      onTiltOpen: function(instance)
      {
        is(document.activeElement, instance.presenter.canvas,
          "The visualizer canvas should be focused on initialization.");

        ok(Tilt.visualizers[id] instanceof TiltVisualizer,
          "A new instance of the visualizer wasn't created properly.");
        ok(Tilt.visualizers[id].isInitialized(),
          "The new instance of the visualizer wasn't initialized properly.");
      },
      onTiltClose: function()
      {
        is(document.activeElement, gBrowser.selectedBrowser,
          "The focus wasn't correctly given back to the selectedBrowser.");

        is(Tilt.visualizers[id], null,
          "The current instance of the visualizer wasn't destroyed properly.");
      },
      onEnd: function()
      {
        cleanup();
      }
    }, true, function suddenDeath()
    {
      ok(false, "Tilt could not be initialized properly.");
      cleanup();
    });
  });
}

function cleanup() {
  gBrowser.removeCurrentTab();
  finish();
}
