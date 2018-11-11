/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test global exit button

const TEST_URL = "data:text/html;charset=utf-8,";

const { OS } = require("resource://gre/modules/osfile.jsm");

function* waitUntilScreenshot() {
  return new Promise(Task.async(function* (resolve) {
    let { Downloads } = require("resource://gre/modules/Downloads.jsm");
    let list = yield Downloads.getList(Downloads.ALL);

    let view = {
      onDownloadAdded: download => {
        download.whenSucceeded().then(() => {
          resolve(download.target.path);
          list.removeView(view);
        });
      }
    };

    yield list.addView(view);
  }));
}

addRDMTask(TEST_URL, function* ({ ui: {toolWindow} }) {
  let { store, document } = toolWindow;

  // Wait until the viewport has been added
  yield waitUntilState(store, state => state.viewports.length == 1);

  info("Click the screenshot button");
  let screenshotButton = document.getElementById("global-screenshot-button");
  screenshotButton.click();

  let whenScreenshotSucceeded = waitUntilScreenshot();

  let filePath = yield whenScreenshotSucceeded;
  let image = new Image();
  image.src = OS.Path.toFileURI(filePath);

  yield once(image, "load");

  // We have only one viewport at the moment
  let viewport = store.getState().viewports[0];
  let ratio = window.devicePixelRatio;

  is(image.width, viewport.width * ratio,
    "screenshot width has the expected width");

  is(image.height, viewport.height * ratio,
    "screenshot width has the expected height");

  yield OS.File.remove(filePath);
});
