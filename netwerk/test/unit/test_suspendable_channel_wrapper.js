/* Any copyright is dedicated to the Public Domain.
https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { sinon } = ChromeUtils.importESModule(
  "resource://testing-common/Sinon.sys.mjs"
);

const { setTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs"
);

/**
 * A very basic nsIChannel implementation in JavaScript that we can spy on
 * and stub methods on using Sinon.
 */
class MockChannel {
  #uri = null;

  constructor(uriString) {
    let uri = Services.io.newURI(uriString);
    this.#uri = uri;
    this.originalURI = uri;
  }

  contentType = "application/x-mock-channel-content";
  loadAttributes = null;
  contentLength = 0;
  owner = null;
  notificationCallbacks = null;
  securityInfo = null;
  originalURI = null;
  status = Cr.NS_OK;

  get name() {
    return this.#uri;
  }

  get URI() {
    return this.#uri;
  }

  get loadGroup() {
    return null;
  }
  set loadGroup(_val) {}

  get loadInfo() {
    return null;
  }
  set loadInfo(_val) {}

  open() {
    throw Components.Exception(
      `${this.constructor.name}.open not implemented`,
      Cr.NS_ERROR_NOT_IMPLEMENTED
    );
  }

  asyncOpen(observer) {
    observer.onStartRequest(this, null);
  }

  asyncRead(listener, ctxt) {
    return listener.onStartRequest(this, ctxt);
  }

  isPending() {
    return false;
  }

  cancel(status) {
    this.status = status;
  }

  suspend() {
    throw Components.Exception(
      `${this.constructor.name}.suspend not implemented`,
      Cr.NS_ERROR_NOT_IMPLEMENTED
    );
  }

  resume() {
    throw Components.Exception(
      `${this.constructor.name}.resume not implemented`,
      Cr.NS_ERROR_NOT_IMPLEMENTED
    );
  }

  QueryInterface = ChromeUtils.generateQI([
    "nsIChannel",
    "nsIRequest",
    // We obviously don't implement nsIRegion here, but we want to test that we
    // can QI down to whatever the inner channel implements.
    "nsIRegion",
  ]);
}

/**
 * A bare-minimum nsIStreamListener that doesn't do anything, useful for
 * passing into methods that expect one of these.
 */
class FakeStreamListener {
  onStartRequest(_request) {}
  onDataAvailable(_request, _stream, _offset, _count) {}
  onStopRequest(_request, _status) {}
  QueryInterface = ChromeUtils.generateQI(["nsIStreamListener"]);
}

/**
 * Test that calling asyncOpen on a nsISuspendedChannel does not call
 * asyncOpen on the inner channel initially if the nsISuspendedChannel had
 * been suspended. Only after calling resume() on the nsISuspendedChannel does
 * the asyncOpen call go through.
 */
add_task(async function test_no_asyncOpen_inner() {
  let innerChannel = new MockChannel("about:newtab");
  Assert.ok(innerChannel.QueryInterface(Ci.nsIChannel));
  let suspendedChannel = Services.io.newSuspendableChannelWrapper(innerChannel);

  let sandbox = sinon.createSandbox();
  sandbox.stub(innerChannel, "asyncOpen");

  suspendedChannel.suspend();

  let fakeStreamListener = new FakeStreamListener();
  suspendedChannel.asyncOpen(fakeStreamListener);
  Assert.ok(innerChannel.asyncOpen.notCalled, "asyncOpen not called on inner");
  Assert.ok(suspendedChannel.isPending(), "suspended channel is pending");
  suspendedChannel.resume();
  Assert.ok(innerChannel.asyncOpen.calledOnce, "asyncOpen called on inner");

  sandbox.restore();
});

/**
 * Tests that nsIChannel and nsIRequest property and method calls are
 * forwarded to the inner channel (except for asyncOpen). This isn't really
 * exhaustive, but checks some fairly important methods and properties.
 */
add_task(async function test_forwarding() {
  let innerChannel = new MockChannel("about:newtab");
  let suspendedChannel = Services.io.newSuspendableChannelWrapper(innerChannel);

  let sandbox = sinon.createSandbox();
  sandbox.stub(innerChannel, "asyncOpen");

  let nameSpy = sandbox.spy(innerChannel, "name", ["get"]);
  suspendedChannel.name;
  Assert.ok(nameSpy.get.calledOnce, "name was retreived from inner");

  sandbox.stub(innerChannel, "suspend");
  suspendedChannel.suspend();
  Assert.ok(
    innerChannel.suspend.notCalled,
    "suspend not called on inner (since not yet opened)"
  );

  sandbox.stub(innerChannel, "resume");
  suspendedChannel.resume();
  Assert.ok(
    innerChannel.resume.notCalled,
    "resume not called on inner (since not yet opened)"
  );

  let loadGroupSpy = sandbox.spy(innerChannel, "loadGroup", ["get", "set"]);
  suspendedChannel.loadGroup;
  Assert.ok(loadGroupSpy.get.calledOnce, "loadGroup was retreived from inner");
  suspendedChannel.loadGroup = null;
  Assert.ok(loadGroupSpy.set.calledOnce, "loadGroup was set on inner");

  let loadInfoSpy = sandbox.spy(innerChannel, "loadInfo", ["get", "set"]);
  suspendedChannel.loadInfo;
  Assert.ok(loadInfoSpy.get.calledOnce, "loadInfo was retreived from inner");
  suspendedChannel.loadInfo = null;
  Assert.ok(loadInfoSpy.set.calledOnce, "loadInfo was set on inner");

  let URISpy = sandbox.spy(innerChannel, "URI", ["get"]);
  suspendedChannel.URI;
  Assert.ok(URISpy.get.calledOnce, "URI was retreived from inner");

  Assert.ok(
    innerChannel.asyncOpen.notCalled,
    "asyncOpen never called on the inner channel"
  );

  // Now check that QI forwarding works for the underlying channel.
  Assert.ok(
    innerChannel.QueryInterface(Ci.nsIRegion),
    "Inner QIs to nsIRegion"
  );

  Assert.ok(
    suspendedChannel.QueryInterface(Ci.nsIRegion),
    "Can QI to something the inner channel implements"
  );

  sandbox.restore();
});

/**
 * Test that calling resume on an nsISuspendedChannel does not call
 * asyncOpen on the inner channel until asyncOpen is called on the
 * nsISuspendedChannel.
 */
add_task(async function test_no_asyncOpen_on_resume() {
  let innerChannel = new MockChannel("about:newtab");
  let suspendedChannel = Services.io.newSuspendableChannelWrapper(innerChannel);
  suspendedChannel.suspend();

  let sandbox = sinon.createSandbox();
  sandbox.stub(innerChannel, "asyncOpen");

  Assert.ok(innerChannel.asyncOpen.notCalled, "asyncOpen not called on inner");
  Assert.ok(suspendedChannel.isPending(), "suspended channel is pending");
  suspendedChannel.resume();
  Assert.ok(innerChannel.asyncOpen.notCalled, "asyncOpen not called on inner");

  let fakeStreamListener = new FakeStreamListener();
  suspendedChannel.asyncOpen(fakeStreamListener);
  Assert.ok(innerChannel.asyncOpen.calledOnce, "asyncOpen called on inner");

  sandbox.restore();
});

/**
 * Test that we can get access to the data provided by the inner channel through
 * an nsISuspendedChannel that has been resumed after being suspended.
 */
add_task(async function test_allow_data() {
  let innerChannel = Cc["@mozilla.org/network/input-stream-channel;1"]
    .createInstance(Ci.nsIInputStreamChannel)
    .QueryInterface(Ci.nsIChannel);
  let suspendedChannel = Services.io.newSuspendableChannelWrapper(innerChannel);

  const TEST_STRING = "This is a test string!";
  let stringStream = Cc["@mozilla.org/io/string-input-stream;1"].createInstance(
    Ci.nsIStringInputStream
  );
  stringStream.setByteStringData(TEST_STRING);

  // Let's just make up some HTTPChannel to steal some properties from to
  // make things easier.
  let httpChan = NetUtil.newChannel({
    uri: "http://localhost",
    loadUsingSystemPrincipal: true,
  });
  innerChannel.contentStream = stringStream;
  innerChannel.contentType = "text/plain";
  innerChannel.setURI(httpChan.URI);
  innerChannel.loadInfo = httpChan.loadInfo;

  suspendedChannel.suspend();

  let completedFetch = false;
  let fetchPromise = new Promise((resolve, reject) => {
    NetUtil.asyncFetch(suspendedChannel, (stream, result) => {
      if (!Components.isSuccessCode(result)) {
        reject(new Error(`Failed to fetch stream`));
        return;
      }
      completedFetch = true;

      resolve(stream);
    });
  });

  // Wait for 1 second to make sure that the fetch didn't occur.
  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  await new Promise(resolve => setTimeout(resolve, 1000));
  Assert.ok(!completedFetch, "Should not have completed the fetch.");

  suspendedChannel.resume();
  let resultStream = await fetchPromise;
  Assert.ok(completedFetch, "Should have completed the fetch.");

  let resultString = NetUtil.readInputStreamToString(
    resultStream,
    resultStream.available()
  );

  Assert.equal(TEST_STRING, resultString, "Got back the expected string.");
});
