"use strict";

const { HttpServer } = ChromeUtils.importESModule(
  "resource://testing-common/httpd.sys.mjs"
);

function makeChannel(url) {
  return NetUtil.newChannel({
    uri: url,
    loadUsingSystemPrincipal: true,
  }).QueryInterface(Ci.nsIHttpChannel);
}

ChromeUtils.defineLazyGetter(this, "URL", function () {
  return "http://localhost:" + httpServer.identity.primaryPort;
});

let httpServer = null;

function pathHandler(metadata, response) {
  var body;
  if (metadata.hasHeader("Idempotency-Key")) {
    response.setStatusLine(metadata.httpVersion, 200, "OK");
    // echo back the header for further validation
    let IDK = metadata.getHeader("Idempotency-Key");
    response.setHeader("Idempotency-Key", IDK, false);
    body = "success";
  } else {
    response.setStatusLine(
      metadata.httpVersion,
      500,
      "missing Idempotency-Key"
    );
    body = "failed";
  }
  response.bodyOutputStream.write(body, body.length);
}

// Helper-method to validate that the "Idempotency-Key" value that we
// generate matches the expected format and is the same in the response.
// Meant to be called inside of a test that's been added via add_task.
function validateAutomaticallyAddedHeaderVal(chan) {
  let valueFromRequest = chan.getRequestHeader("Idempotency-Key");
  let valueFromResponse = chan.getResponseHeader("Idempotency-Key");

  // Validate that the header's got a nonempty value, and that the
  // response matches the request:
  Assert.notEqual(valueFromRequest, "");
  Assert.notEqual(valueFromResponse, "");
  Assert.equal(valueFromRequest, valueFromResponse);
}

add_setup(async () => {
  httpServer = new HttpServer();
  httpServer.registerPathHandler("/test_bug1830022", pathHandler);
  httpServer.start(-1);
  registerCleanupFunction(async () => await httpServer.stop());
});

// tests if we add the header for the POST request
add_task(async function idempotency_key_addition_for_post() {
  let chan = makeChannel(URL + "/test_bug1830022");
  chan.requestMethod = "POST";
  await new Promise(resolve => {
    chan.asyncOpen(new ChannelListener(resolve));
  });
  validateAutomaticallyAddedHeaderVal(chan);
});

// tests if we add the header for the PATCH request
add_task(async function idempotency_key_addition_for_patch() {
  let chan = makeChannel(URL + "/test_bug1830022");
  chan.requestMethod = "PATCH";
  await new Promise(resolve => {
    chan.asyncOpen(new ChannelListener(resolve));
  });
  validateAutomaticallyAddedHeaderVal(chan);
});

// tests Idempotency key's uniqueness
add_task(async function idempotency_key_uniqueness() {
  let chan = makeChannel(URL + "/test_bug1830022");
  chan.requestMethod = "POST";
  await new Promise(resolve => {
    chan.asyncOpen(new ChannelListener(resolve));
  });

  let chan2 = makeChannel(URL + "/test_bug1830022");
  chan2.requestMethod = "POST";
  await new Promise(resolve => {
    chan2.asyncOpen(new ChannelListener(resolve));
  });

  Assert.notEqual(
    chan.getRequestHeader("Idempotency-Key"),
    chan2.getRequestHeader("Idempotency-Key")
  );

  // tests if the Idempotency key is same for reposts
  let chan3 = makeChannel(URL + "/test_bug1830022");
  chan3.requestMethod = "POST";
  await new Promise(resolve => {
    chan3.asyncOpen(new ChannelListener(resolve));
  });

  let cachekey = chan3.QueryInterface(Ci.nsICacheInfoChannel).cacheKey;

  let chan4 = makeChannel(URL + "/test_bug1830022");
  chan4.requestMethod = "POST";
  chan4.QueryInterface(Ci.nsICacheInfoChannel).cacheKey = cachekey;
  await new Promise(resolve => {
    chan4.asyncOpen(new ChannelListener(resolve));
  });

  Assert.equal(
    chan3.getRequestHeader("Idempotency-Key"),
    chan4.getRequestHeader("Idempotency-Key")
  );
});

// tests if we do not overwrite the header that is set before opening the channel
add_task(async function idempotency_key_addition_for_post() {
  // construct the channel
  let chan = makeChannel(URL + "/test_bug1830022");
  chan.setRequestHeader("Idempotency-Key", "U-V-W-X-Y-Z", false);
  chan.requestMethod = "POST";
  await new Promise(resolve => {
    chan.asyncOpen(new ChannelListener(resolve));
  });
  Assert.equal(chan.getRequestHeader("Idempotency-Key"), "U-V-W-X-Y-Z");
  Assert.equal(chan.getResponseHeader("Idempotency-Key"), "U-V-W-X-Y-Z");
});
