/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from head_cache.js */
/* import-globals-from head_cookies.js */
/* import-globals-from head_channels.js */
/* import-globals-from head_servers.js */

const certOverrideService = Cc[
  "@mozilla.org/security/certoverride;1"
].getService(Ci.nsICertOverrideService);

add_setup(async function setup() {
  certOverrideService.setDisableAllSecurityChecksAndLetAttackersInterceptMyData(
    true
  );

  registerCleanupFunction(async () => {
    certOverrideService.setDisableAllSecurityChecksAndLetAttackersInterceptMyData(
      false
    );
  });
});

function makeChan(uri) {
  let chan = NetUtil.newChannel({
    uri,
    loadUsingSystemPrincipal: true,
  }).QueryInterface(Ci.nsIHttpChannel);
  chan.loadFlags = Ci.nsIChannel.LOAD_INITIAL_DOCUMENT_URI;
  return chan;
}

function httpChannelOpenPromise(chan, flags) {
  return new Promise(resolve => {
    function finish(req, buffer) {
      resolve([req, buffer]);
    }
    chan.asyncOpen(new ChannelListener(finish, null, flags));
  });
}

async function channelOpenPromise(url, msg) {
  let conn = new WebSocketConnection();
  let statusObj = await Promise.race([conn.open(url), conn.finished()]);
  if (statusObj && statusObj.status != Cr.NS_OK) {
    return [statusObj.status, ""];
  }
  let finalStatusPromise = conn.finished();
  conn.send(msg);
  let res = await conn.receiveMessages();
  conn.close();
  let finalStatus = await finalStatusPromise;
  return [finalStatus.status, res];
}

// h1.1 direct
async function test_h1_websocket_direct() {
  let wss = new NodeWebSocketServer();
  await wss.start();
  registerCleanupFunction(async () => wss.stop());
  Assert.notEqual(wss.port(), null);
  await wss.registerMessageHandler((data, ws) => {
    ws.send(data);
  });
  let url = `wss://localhost:${wss.port()}`;
  const msg = "test websocket";

  let conn = new WebSocketConnection();
  await conn.open(url);
  conn.send(msg);
  let mess1 = await conn.receiveMessages();
  Assert.deepEqual(mess1, [msg]);

  // Now send 3 more, and check that we received all of them
  conn.send(msg);
  conn.send(msg);
  conn.send(msg);
  let mess2 = [];
  while (mess2.length < 3) {
    // receive could return 1, 2 or all 3 replies.
    mess2 = mess2.concat(await conn.receiveMessages());
  }
  Assert.deepEqual(mess2, [msg, msg, msg]);

  conn.close();
  let { status } = await conn.finished();

  Assert.equal(status, Cr.NS_OK);
}

// h1 server with secure h1.1 proxy
async function test_h1_ws_with_secure_h1_proxy() {
  let proxy = new NodeHTTPSProxyServer();
  await proxy.start();

  let wss = new NodeWebSocketServer();
  await wss.start();

  registerCleanupFunction(async () => {
    await wss.stop();
    await proxy.stop();
  });

  Assert.notEqual(wss.port(), null);
  await wss.registerMessageHandler((data, ws) => {
    ws.send(data);
  });

  let url = `wss://localhost:${wss.port()}`;
  const msg = "test h1.1 websocket with h1.1 secure proxy";
  let [status, res] = await channelOpenPromise(url, msg);
  Assert.equal(status, Cr.NS_OK);
  Assert.deepEqual(res, [msg]);

  await proxy.stop();
}

async function test_h2_websocket_direct() {
  Services.prefs.setBoolPref("network.http.http2.websockets", true);
  let wss = new NodeWebSocketHttp2Server();
  await wss.start();
  registerCleanupFunction(async () => wss.stop());

  Assert.notEqual(wss.port(), null);
  await wss.registerMessageHandler((data, ws) => {
    ws.send(data);
  });
  let url = `wss://localhost:${wss.port()}`;
  const msg = "test h2 websocket h2 direct";
  let [status, res] = await channelOpenPromise(url, msg);
  Assert.equal(status, Cr.NS_OK);
  Assert.deepEqual(res, [msg]);
}

// ws h1.1 with insecure h1.1 proxy
async function test_h1_ws_with_h1_insecure_proxy() {
  Services.prefs.setBoolPref("network.http.http2.websockets", false);
  let proxy = new NodeHTTPProxyServer();
  await proxy.start();

  let wss = new NodeWebSocketServer();
  await wss.start();

  registerCleanupFunction(async () => {
    await wss.stop();
    await proxy.stop();
  });

  Assert.notEqual(wss.port(), null);

  await wss.registerMessageHandler((data, ws) => {
    ws.send(data);
  });
  let url = `wss://localhost:${wss.port()}`;
  const msg = "test h1 websocket with h1 insecure proxy";
  let [status, res] = await channelOpenPromise(url, msg);
  Assert.equal(status, Cr.NS_OK);
  Assert.deepEqual(res, [msg]);

  await proxy.stop();
}

// ws h1.1 with h2 proxy
async function test_h1_ws_with_h2_proxy() {
  Services.prefs.setBoolPref("network.http.http2.websockets", false);

  let proxy = new NodeHTTP2ProxyServer();
  await proxy.start();

  let wss = new NodeWebSocketServer();
  await wss.start();

  registerCleanupFunction(async () => {
    await wss.stop();
    await proxy.stop();
  });

  Assert.notEqual(wss.port(), null);
  await wss.registerMessageHandler((data, ws) => {
    ws.send(data);
  });

  let url = `wss://localhost:${wss.port()}`;
  const msg = "test h1 websocket with h2 proxy";
  let [status, res] = await channelOpenPromise(url, msg);
  Assert.equal(status, Cr.NS_OK);
  Assert.deepEqual(res, [msg]);

  await proxy.stop();
}

// ws h2 with insecure h1.1 proxy
async function test_h2_ws_with_h1_insecure_proxy() {
  Services.prefs.setBoolPref("network.http.http2.websockets", true);

  let proxy = new NodeHTTPProxyServer();
  await proxy.start();

  registerCleanupFunction(async () => {
    await wss.stop();
    await proxy.stop();
  });

  let wss = new NodeWebSocketHttp2Server();
  await wss.start();
  Assert.notEqual(wss.port(), null);
  await wss.registerMessageHandler((data, ws) => {
    ws.send(data);
  });

  let url = `wss://localhost:${wss.port()}`;
  const msg = "test h2 websocket with h1 insecure proxy";
  let [status, res] = await channelOpenPromise(url, msg);
  Assert.equal(status, Cr.NS_OK);
  Assert.deepEqual(res, [msg]);

  await proxy.stop();
}

// ws h2 with secure h1 proxy
async function test_h2_ws_with_h1_secure_proxy() {
  Services.prefs.setBoolPref("network.http.http2.websockets", true);

  let proxy = new NodeHTTPSProxyServer();
  await proxy.start();

  let wss = new NodeWebSocketHttp2Server();
  await wss.start();

  registerCleanupFunction(async () => {
    await wss.stop();
    await proxy.stop();
  });

  Assert.notEqual(wss.port(), null);
  await wss.registerMessageHandler((data, ws) => {
    ws.send(data);
  });

  let url = `wss://localhost:${wss.port()}`;
  const msg = "test h2 websocket with h1 secure proxy";
  let [status, res] = await channelOpenPromise(url, msg);
  Assert.equal(status, Cr.NS_OK);
  Assert.deepEqual(res, [msg]);

  await proxy.stop();
}

// ws h2 with secure h2 proxy
async function test_h2_ws_with_h2_proxy() {
  Services.prefs.setBoolPref("network.http.http2.websockets", true);

  let proxy = new NodeHTTP2ProxyServer();
  await proxy.start(); // start and register proxy "filter"

  let wss = new NodeWebSocketHttp2Server();
  await wss.start(); // init port

  registerCleanupFunction(async () => {
    await wss.stop();
    await proxy.stop();
  });

  Assert.notEqual(wss.port(), null);
  await wss.registerMessageHandler((data, ws) => {
    ws.send(data);
  });

  let url = `wss://localhost:${wss.port()}`;
  const msg = "test h2 websocket with h2 proxy";
  let [status, res] = await channelOpenPromise(url, msg);
  Assert.equal(status, Cr.NS_OK);
  Assert.deepEqual(res, [msg]);

  await proxy.stop();
}

async function test_bug_1848013() {
  Services.prefs.setBoolPref("network.http.http2.websockets", true);

  let proxy = new NodeHTTPProxyServer();
  await proxy.start();

  registerCleanupFunction(async () => {
    await wss.stop();
    await proxy.stop();
  });

  let wss = new NodeWebSocketHttp2Server();
  await wss.start();
  Assert.notEqual(wss.port(), null);
  await wss.registerMessageHandler((data, ws) => {
    ws.send(data);
  });

  // To create a h2 connection before the websocket one.
  let chan = makeChan(`https://localhost:${wss.port()}/`);
  await httpChannelOpenPromise(chan, CL_ALLOW_UNKNOWN_CL);

  let url = `wss://localhost:${wss.port()}`;
  const msg = "test h2 websocket with h1 insecure proxy";
  let [status, res] = await channelOpenPromise(url, msg);
  Assert.equal(status, Cr.NS_OK);
  Assert.deepEqual(res, [msg]);

  await proxy.stop();
}

function ActivityObserver() {}

ActivityObserver.prototype = {
  activites: [],
  observeConnectionActivity(
    aHost,
    aPort,
    aSSL,
    aHasECH,
    aIsHttp3,
    aActivityType,
    aActivitySubtype,
    aTimestamp,
    aExtraStringData
  ) {
    info(
      "*** Connection Activity 0x" +
        aActivityType.toString(16) +
        " 0x" +
        aActivitySubtype.toString(16) +
        " " +
        aExtraStringData +
        "\n"
    );
    this.activites.push({
      host: aHost,
      subType: aActivitySubtype,
      connInfo: aExtraStringData,
    });
  },
};

function checkConnectionActivities(activites, host, port) {
  let connections = [];
  for (let activity of activites) {
    if (
      activity.host != host ||
      activity.subType !=
        Ci.nsIHttpActivityDistributor.ACTIVITY_SUBTYPE_CONNECTION_CREATED
    ) {
      continue;
    }
    connections.push(activity.connInfo);
  }

  function parseConnInfoHash(str) {
    if (str.length < 6) {
      throw new Error("Invalid input string");
    }
    // Extract the 6th character (index 5)
    const h2Flag = str[5];
    // Regular expression to extract hostname and port
    const regex = /\[.*?\](.*?):(\d+)/;
    const match = str.match(regex);
    if (!match) {
      throw new Error("Can not extract hostname and port");
    }
    return {
      h2Flag,
      host: match[1],
      port: match[2],
    };
  }

  Assert.equal(connections.length, 2);

  const firstConn = parseConnInfoHash(connections[0]);
  Assert.equal(firstConn.h2Flag, ".");
  Assert.equal(firstConn.host, host);
  Assert.equal(firstConn.port, port);
  const fallbackConn = parseConnInfoHash(connections[1]);
  Assert.equal(fallbackConn.h2Flag, "X");
  Assert.equal(fallbackConn.host, host);
  Assert.equal(fallbackConn.port, port);
}

async function test_websocket_fallback() {
  let observerService = Cc[
    "@mozilla.org/network/http-activity-distributor;1"
  ].getService(Ci.nsIHttpActivityDistributor);
  let observer = new ActivityObserver();
  observerService.addObserver(observer);
  observerService.observeConnection = true;

  Services.prefs.setBoolPref("network.http.http2.websockets", true);
  let wss = new NodeWebSocketHttp2Server();
  await wss.start(0, true);
  registerCleanupFunction(async () => wss.stop());

  Assert.notEqual(wss.port(), null);
  await wss.registerMessageHandler((data, ws) => {
    ws.send(data);
  });
  let url = `wss://localhost:${wss.port()}`;
  await channelOpenPromise(url, "");
  checkConnectionActivities(observer.activites, "localhost", wss.port());
}

add_task(test_h1_websocket_direct);
add_task(test_h2_websocket_direct);
add_task(test_h1_ws_with_secure_h1_proxy);
add_task(test_h1_ws_with_h1_insecure_proxy);
add_task(test_h1_ws_with_h2_proxy);
add_task(test_h2_ws_with_h1_insecure_proxy);
add_task(test_h2_ws_with_h1_secure_proxy);
add_task(test_h2_ws_with_h2_proxy);
add_task(test_bug_1848013);
add_task(test_websocket_fallback);
