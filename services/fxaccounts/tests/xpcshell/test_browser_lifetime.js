/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { XPCShellContentUtils } = ChromeUtils.importESModule(
  "resource://testing-common/XPCShellContentUtils.sys.mjs"
);
XPCShellContentUtils.ensureInitialized(this);
const server = XPCShellContentUtils.createHttpServer({
  hosts: ["example.com"],
});
server.registerPathHandler("/dummy", (request, response) => {
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.write("ok");
});

add_task(async function () {
  const { FxAccountsPairingChannel } = ChromeUtils.importESModule(
    "resource://gre/modules/FxAccountsPairingChannel.sys.mjs"
  );

  // Collect the module top-level variables and the pointed objects.
  for (let i = 0; i < 100; i++) {
    Cu.forceGC();
    Cu.forceCC();
    await new Promise(resolve => executeSoon(resolve));
  }

  let caught = false;
  try {
    // The windowless browser in FxAccountsPairingChannel.sys.mjs should still
    // be alive, and the `new WebSocket(...)` call inside it shouldn't hit the
    // dead object error.
    //
    // NOTE: The connection itself will hit error.
    await FxAccountsPairingChannel._makePairingChannel(
      "ws://example.com/dummy"
    );
  } catch (e) {
    caught = true;
    Assert.notEqual(
      e.message,
      "can't access dead object",
      "Touching the windowless browser after GC/CC should not hit dead object"
    );
  }
  Assert.ok(caught, "Exception should be caught for connection");
});
