/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

function colorAt(image, x, y) {
  // Create a test canvas element.
  const HTML_NS = "http://www.w3.org/1999/xhtml";
  const canvas = document.createElementNS(HTML_NS, "canvas");
  canvas.width = image.width;
  canvas.height = image.height;

  // Draw the image in the canvas
  const context = canvas.getContext("2d");
  context.drawImage(image, 0, 0, image.width, image.height);

  // Return the color found at the provided x,y coordinates as a "r, g, b" string.
  const [r, g, b] = context.getImageData(x, y, 1, 1).data;
  return { r, g, b };
}

function waitUntilScreenshot() {
  return new Promise(async function(resolve) {
    const { Downloads } = require("resource://gre/modules/Downloads.jsm");
    const list = await Downloads.getList(Downloads.ALL);

    const view = {
      onDownloadAdded: download => {
        download.whenSucceeded().then(() => {
          resolve(download.target.path);
          list.removeView(view);
        });
      },
    };

    await list.addView(view);
  });
}

async function resetDownloads() {
  info("Reset downloads");
  const { Downloads } = require("resource://gre/modules/Downloads.jsm");
  const publicList = await Downloads.getList(Downloads.PUBLIC);
  const downloads = await publicList.getAll();
  for (const download of downloads) {
    publicList.remove(download);
    await download.finalize(true);
  }
}

async function takeNodeScreenshot(inspector) {
  // Cleanup all downloads at the end of the test.
  registerCleanupFunction(resetDownloads);

  info("Call screenshotNode() and wait until the screenshot is found in the Downloads");
  const whenScreenshotSucceeded = waitUntilScreenshot();
  inspector.screenshotNode();
  const filePath = await whenScreenshotSucceeded;

  info("Create an image using the downloaded fileas source");
  const image = new Image();
  image.src = OS.Path.toFileURI(filePath);
  await once(image, "load");

  return image;
}
/* exported takeNodeScreenshot */

/**
 * Check that the provided image has the expected width, height, and color.
 * NOTE: This test assumes that the image is only made of a single color and will only
 * check one pixel.
 */
async function assertSingleColorScreenshotImage(image, width, height, { r, g, b }) {
  is(image.width, width, "node screenshot has the expected width");
  is(image.height, height, "node screenshot has the expected height");

  const color = colorAt(image, 0, 0);
  is(color.r, r, "node screenshot has the expected red component");
  is(color.g, g, "node screenshot has the expected green component");
  is(color.b, b, "node screenshot has the expected blue component");
}
/* exported assertSingleColorScreenshotImage */
