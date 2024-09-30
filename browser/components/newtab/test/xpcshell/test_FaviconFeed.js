/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  FaviconFeed: "resource://activity-stream/lib/FaviconFeed.sys.mjs",
  HttpServer: "resource://testing-common/httpd.sys.mjs",
  NetUtil: "resource://gre/modules/NetUtil.sys.mjs",
});

// Copy from toolkit/components/places/tests/head_common.js
function readInputStreamData(aStream) {
  let bistream = Cc["@mozilla.org/binaryinputstream;1"].createInstance(
    Ci.nsIBinaryInputStream
  );
  try {
    bistream.setInputStream(aStream);
    let expectedData = [];
    let avail;
    while ((avail = bistream.available())) {
      expectedData = expectedData.concat(bistream.readByteArray(avail));
    }
    return expectedData;
  } finally {
    bistream.close();
  }
}

function readFileData(aFile) {
  let inputStream = Cc[
    "@mozilla.org/network/file-input-stream;1"
  ].createInstance(Ci.nsIFileInputStream);
  // init the stream as RD_ONLY, -1 == default permissions.
  inputStream.init(aFile, 0x01, -1, null);

  // Check the returned size versus the expected size.
  let size = inputStream.available();
  let bytes = readInputStreamData(inputStream);
  if (size !== bytes.length) {
    throw new Error("Didn't read expected number of bytes");
  }
  return bytes;
}

async function fileDataToDataURL(data, mimeType) {
  const dataURL = await new Promise(resolve => {
    const buffer = new Uint8ClampedArray(data);
    const blob = new Blob([buffer], { type: mimeType });
    const reader = new FileReader();
    reader.onload = e => {
      resolve(e.target.result);
    };
    reader.readAsDataURL(blob);
  });
  return dataURL;
}

async function readFileDataAsDataURL(file, mimeType) {
  const data = readFileData(file);
  return fileDataToDataURL(data, mimeType);
}

add_task(async function test_getFaviconDataURLFromNetwork() {
  const faviconFile = do_get_file("favicon.png");

  info("Setup http server that returns favicon content");
  const httpServer = new HttpServer();
  httpServer.registerPathHandler("/favicon.png", (request, response) => {
    const inputStream = Cc[
      "@mozilla.org/network/file-input-stream;1"
    ].createInstance(Ci.nsIFileInputStream);
    inputStream.init(faviconFile, 0x01, -1, null);
    const size = inputStream.available();
    const faviconData = NetUtil.readInputStreamToString(inputStream, size);

    response.setStatusLine(request.httpVersion, 200, "Ok");
    response.setHeader("Content-Type", "image/png", false);
    response.bodyOutputStream.write(faviconData, faviconData.length);
  });
  httpServer.start(-1);

  info("Get favicon data via FaviconFeed");
  const feed = new FaviconFeed();
  Assert.ok(feed, "FaviconFeed should be constructed");
  const result = await feed.getFaviconDataURLFromNetwork(
    Services.io.newURI(
      `http://localhost:${httpServer.identity.primaryPort}/favicon.png`
    )
  );

  Assert.equal(
    result.spec,
    await readFileDataAsDataURL(faviconFile, "image/png"),
    "getFaviconDataURLFromNetwork returns correct data url"
  );
});
