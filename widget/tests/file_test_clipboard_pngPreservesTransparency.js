/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/* import-globals-from clipboard_helper.js */

"use strict";

function getLoadContext() {
  return SpecialPowers.wrap(window).docShell.QueryInterface(Ci.nsILoadContext);
}

/* toBase64 copied from extensions/xml-rpc/src/nsXmlRpcClient.js */

/* Convert data (an array of integers) to a Base64 string. */
const toBase64Table =
  // eslint-disable-next-line no-useless-concat
  "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz" + "0123456789+/";
const base64Pad = "=";
function toBase64(data) {
  var result = "";
  var length = data.length;
  var i;
  // Convert every three bytes to 4 ascii characters.
  for (i = 0; i < length - 2; i += 3) {
    result += toBase64Table[data[i] >> 2];
    result += toBase64Table[((data[i] & 0x03) << 4) + (data[i + 1] >> 4)];
    result += toBase64Table[((data[i + 1] & 0x0f) << 2) + (data[i + 2] >> 6)];
    result += toBase64Table[data[i + 2] & 0x3f];
  }

  // Convert the remaining 1 or 2 bytes, pad out to 4 characters.
  if (length % 3) {
    i = length - (length % 3);
    result += toBase64Table[data[i] >> 2];
    if (length % 3 == 2) {
      result += toBase64Table[((data[i] & 0x03) << 4) + (data[i + 1] >> 4)];
      result += toBase64Table[(data[i + 1] & 0x0f) << 2];
      result += base64Pad;
    } else {
      result += toBase64Table[(data[i] & 0x03) << 4];
      result += base64Pad + base64Pad;
    }
  }

  return result;
}

// Get clipboard data to paste.
function getPNGFromClipboard(clipboard) {
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

  var bytes = stream.readByteArray(stream.available()); // returns int[]

  var base64String = toBase64(bytes);

  return "data:image/png;base64," + base64String;
}

async function putOnClipboard(expected, operationFn, desc, type) {
  try {
    await SimpleTest.promiseClipboardChange(expected, operationFn, type, 1000);
  } catch (e) {
    throw new Error(`Failed "${desc}" due to "${e.toString()}"`);
  }
}

add_setup(function init() {
  cleanupAllClipboard();
});

add_task(async function test_copy() {
  // Copy the image's data to the clipboard
  await SpecialPowers.pushPrefEnv({
    set: [["clipboard.copy_image.as_png", true]],
  });
  try {
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
  } catch (e) {
    ok(false, e.toString());
  }

  // Get the data from the clipboard
  let dataURLString = getPNGFromClipboard(clipboard);
  let targetPng = document.getElementById("targetPng");
  targetPng.src = dataURLString;
  await new Promise(resolve => {
    targetPng.addEventListener(
      "load",
      () => {
        resolve();
      },
      { once: true }
    );
  });

  // Make sure the resulting PNG has transparency
  // by drawing on a canvas
  let canvas = document.getElementById("targetCanvas");
  let ctx = canvas.getContext("2d");
  // The image is transparent in the top left corner.
  // Fill the canvas with a color that is not in the image
  ctx.fillStyle = "rgb(10, 30, 230)";
  ctx.fillRect(0, 0, 200, 200);
  ctx.drawImage(targetPng, 0, 0);
  let imageData = ctx.getImageData(0, 0, 1, 1);
  // getImageData() returns in RGBA order
  is(imageData.data[0], 10, "R value should not have changed");
  is(imageData.data[1], 30, "G value should not have changed");
  is(imageData.data[2], 230, "B value should not have changed");

  cleanupAllClipboard();
  await SpecialPowers.popPrefEnv();
});
