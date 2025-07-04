/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/* globals ImageDecoder */
/* import-globals-from clipboard_helper.js */

"use strict";

function getLoadContext() {
  return SpecialPowers.wrap(window).docShell.QueryInterface(Ci.nsILoadContext);
}

// Get clipboard data to paste.
async function getPNGFromClipboard(clipboard) {
  let trans = Cc["@mozilla.org/widget/transferable;1"].createInstance(
    Ci.nsITransferable
  );
  trans.init(getLoadContext());
  trans.addDataFlavor("image/png");
  clipboard.getData(
    trans,
    Ci.nsIClipboard.kGlobalClipboard,
    SpecialPowers.wrap(window).browsingContext.currentWindowContext
  );
  let obj = SpecialPowers.createBlankObject();
  trans.getTransferData("image/png", obj);
  let rawStream = obj.value.QueryInterface(Ci.nsIInputStream);

  let stream = Cc["@mozilla.org/binaryinputstream;1"].createInstance();
  stream.QueryInterface(Ci.nsIBinaryInputStream);

  stream.setInputStream(rawStream);

  let size = stream.available();
  let data = new ArrayBuffer(size);
  stream.readArrayBuffer(size, data);

  let decoder = new ImageDecoder({ type: "image/png", data });
  let { image } = await decoder.decode();
  return image;
}

async function putOnClipboard(expected, operationFn, desc, type) {
  try {
    await SimpleTest.promiseClipboardChange(expected, operationFn, type, 1000);
  } catch (e) {
    throw new Error(`Failed "${desc}" due to "${e.toString()}"`);
  }
}

add_setup(async function init() {
  await cleanupAllClipboard();
});

add_task(async function test_copy() {
  // Copy the image's data to the clipboard
  await SpecialPowers.pushPrefEnv({
    set: [["clipboard.copy_image.as_png", true]],
  });

  await putOnClipboard(
    "",
    () => {
      SpecialPowers.setCommandNode(
        window,
        document.getElementById("pngWithTransparency")
      );
      SpecialPowers.doCommand(window, "cmd_copyImageContents");
    },
    "copy changed clipboard when preference is disabled"
  );

  // Get the data from the clipboard
  let imagePng = await getPNGFromClipboard(clipboard);

  // Make sure the resulting PNG has transparency
  // by drawing on a canvas
  let canvas = document.getElementById("targetCanvas");
  let ctx = canvas.getContext("2d");

  // The image is transparent in the top left corner.
  // Fill the canvas with a color that is not in the image
  ctx.fillStyle = "rgb(10, 30, 230)";
  ctx.fillRect(0, 0, 200, 200);
  ctx.drawImage(imagePng, 0, 0);

  let imageData = ctx.getImageData(0, 0, 1, 1);
  // getImageData() returns in RGBA order
  is(imageData.data[0], 10, "R value should not have changed");
  is(imageData.data[1], 30, "G value should not have changed");
  is(imageData.data[2], 230, "B value should not have changed");

  imageData = ctx.getImageData(100, 100, 1, 1);
  is(imageData.data[0], 255, "R in circle is red");
  // is(imageData.data[1], 0, "G in circle is red");
  is(imageData.data[2], 0, "B in circle is red");

  await cleanupAllClipboard();
  await SpecialPowers.popPrefEnv();
});
