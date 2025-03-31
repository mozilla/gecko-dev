/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { AddonTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/AddonTestUtils.sys.mjs"
);

const API_OHTTP_CONFIG = "http://example.com/ohttp-config";
const API_OHTTP_RELAY = "http://example.com/relay/";

AddonTestUtils.maybeInit(this);
const server = AddonTestUtils.createHttpServer({ hosts: ["example.com"] });

const ohttp = Cc["@mozilla.org/network/oblivious-http;1"].getService(
  Ci.nsIObliviousHttp
);
const ohttpServer = ohttp.server();

const serverContext = {
  statusCode: 0,
  configBody: ohttpServer.encodedConfig,
  failure: false,
};

server.registerPathHandler(
  new URL(API_OHTTP_CONFIG).pathname,
  (request, response) => {
    const bstream = Cc["@mozilla.org/binaryoutputstream;1"].createInstance(
      Ci.nsIBinaryOutputStream
    );
    bstream.setOutputStream(response.bodyOutputStream);
    bstream.writeByteArray(serverContext.configBody);
  }
);

let requestPromise, resolveRequest;
let responsePromise, resolveResponse;

server.registerPathHandler(
  new URL(API_OHTTP_RELAY).pathname,
  async (request, response) => {
    const inputStream = Cc["@mozilla.org/binaryinputstream;1"].createInstance(
      Ci.nsIBinaryInputStream
    );
    inputStream.setInputStream(request.bodyInputStream);
    const requestBody = inputStream.readByteArray(inputStream.available());
    const ohttpRequest = ohttpServer.decapsulate(requestBody);
    const bhttp = Cc["@mozilla.org/network/binary-http;1"].getService(
      Ci.nsIBinaryHttp
    );
    const decodedRequest = bhttp.decodeRequest(ohttpRequest.request);

    response.processAsync();
    if (serverContext.failure) {
      response.setStatusLine(request.httpVersion, 500, "Internal Server Error");
      response.finish();
      resolveRequest(decodedRequest);
      return;
    }

    const BinaryHttpResponse = {
      status: serverContext.statusCode,
      headerNames: [],
      headerValues: [],
      content: new TextEncoder().encode(decodedRequest.content),
      QueryInterface: ChromeUtils.generateQI(["nsIBinaryHttpResponse"]),
    };
    const encResponse = ohttpRequest.encapsulate(
      bhttp.encodeResponse(BinaryHttpResponse)
    );
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.setHeader("Content-Type", "message/ohttp-res", false);

    const bstream = Cc["@mozilla.org/binaryoutputstream;1"].createInstance(
      Ci.nsIBinaryOutputStream
    );
    bstream.setOutputStream(response.bodyOutputStream);
    bstream.writeByteArray(encResponse);
    response.finish();

    resolveRequest(decodedRequest);
  }
);

function resetPromises() {
  const tmp1 = Promise.withResolvers();
  requestPromise = tmp1.promise;
  resolveRequest = tmp1.resolve;

  const tmp2 = Promise.withResolvers();
  responsePromise = tmp2.promise;
  resolveResponse = tmp2.resolve;
}

async function test_success() {
  resetPromises();

  const request = {
    method: "POST",
    scheme: "https",
    authority: "example.com",
    path: "/my-path",
    headerNames: ["User-Agent"],
    headerValues: ["Mozilla/5.0"],
    content: "Hello, world!",
  };

  const expectedResponse = {
    url: `${request.scheme}://${request.authority}${request.path}`,
    statusCode: 42,
    error: "",
  };

  serverContext.statusCode = expectedResponse.statusCode;

  const ohttpClientTester = Cc[
    "@mozilla.org/ohttp-client-test;1"
  ].createInstance(Ci.nsIOhttpClientTest);

  ohttpClientTester.fetch(
    `${request.scheme}://${request.authority}${request.path}`,
    request.method,
    request.content,
    request.headerNames,
    request.headerValues,
    (url, statusCode, headerKeys, headerValues, error) => {
      resolveResponse({
        url,
        statusCode,
        headerKeys,
        headerValues,
        error,
      });
    }
  );

  const [ohttpRequest, ohttpResponse] = await Promise.all([
    requestPromise,
    responsePromise,
  ]);

  // Verify request
  Assert.equal(ohttpRequest.method, request.method);
  Assert.equal(ohttpRequest.scheme, request.scheme);
  Assert.equal(ohttpRequest.authority, request.authority);
  Assert.equal(ohttpRequest.path, request.path);
  Assert.deepEqual(ohttpRequest.headerNames, request.headerNames);
  Assert.deepEqual(ohttpRequest.headerValues, request.headerValues);
  Assert.deepEqual(
    ohttpRequest.content,
    request.content.split("").map(s => s.charCodeAt(0))
  );

  // Verify response
  Assert.equal(ohttpResponse.url, expectedResponse.url);
  Assert.equal(ohttpResponse.statusCode, expectedResponse.statusCode);
  Assert.equal(ohttpResponse.error, expectedResponse.error);
}

async function test_invalid_config() {
  resetPromises();

  const request = {
    method: "POST",
    scheme: "https",
    authority: "example.com",
    path: "/my-path",
    headerNames: ["User-Agent"],
    headerValues: ["Mozilla/5.0"],
    content: "Hello, world!",
  };

  const expectedResponse = {
    url: "",
    statusCode: 0,
    error: "Request failed, error=0x11, category=0x1",
  };

  // Provide invalid config
  // We still set status code to verify that we don't
  // get back 42 as the status.
  serverContext.configBody = [0, 0, 0, 0];
  serverContext.statusCode = 42;

  const ohttpClientTester = Cc[
    "@mozilla.org/ohttp-client-test;1"
  ].createInstance(Ci.nsIOhttpClientTest);

  ohttpClientTester.fetch(
    `${request.scheme}://${request.authority}${request.path}`,
    request.method,
    request.content,
    request.headerNames,
    request.headerValues,
    (url, statusCode, headerKeys, headerValues, error) => {
      resolveResponse({
        url,
        statusCode,
        headerKeys,
        headerValues,
        error,
      });
    }
  );

  // Request promise never resolves as getting config fails
  const ohttpResponse = await responsePromise;

  // Verify response
  Assert.equal(ohttpResponse.url, expectedResponse.url);
  Assert.equal(ohttpResponse.statusCode, expectedResponse.statusCode);
  Assert.equal(ohttpResponse.error, expectedResponse.error);
}

async function test_ohttp_failure() {
  resetPromises();

  const request = {
    method: "POST",
    scheme: "https",
    authority: "example.com",
    path: "/my-path",
    headerNames: ["User-Agent"],
    headerValues: ["Mozilla/5.0"],
    content: "Hello, world!",
  };

  const expectedResponse = {
    url: "",
    statusCode: 0,
    error: "Request failed, error=0x11, category=0x1",
  };

  // Provide a valid config, but make the server fail.
  // We still set status code to verify that we don't
  // get back 42 as the status.
  serverContext.configBody = ohttpServer.encodedConfig;
  serverContext.statusCode = 42;
  serverContext.failure = true;

  const ohttpClientTester = Cc[
    "@mozilla.org/ohttp-client-test;1"
  ].createInstance(Ci.nsIOhttpClientTest);

  ohttpClientTester.fetch(
    `${request.scheme}://${request.authority}${request.path}`,
    request.method,
    request.content,
    request.headerNames,
    request.headerValues,
    (url, statusCode, headerKeys, headerValues, error) => {
      resolveResponse({
        url,
        statusCode,
        headerKeys,
        headerValues,
        error,
      });
    }
  );

  const [ohttpRequest, ohttpResponse] = await Promise.all([
    requestPromise,
    responsePromise,
  ]);

  // Verify request
  Assert.equal(ohttpRequest.method, request.method);
  Assert.equal(ohttpRequest.scheme, request.scheme);
  Assert.equal(ohttpRequest.authority, request.authority);
  Assert.equal(ohttpRequest.path, request.path);
  Assert.deepEqual(ohttpRequest.headerNames, request.headerNames);
  Assert.deepEqual(ohttpRequest.headerValues, request.headerValues);
  Assert.deepEqual(
    ohttpRequest.content,
    request.content.split("").map(s => s.charCodeAt(0))
  );

  // Verify response
  Assert.equal(ohttpResponse.url, expectedResponse.url);
  Assert.equal(ohttpResponse.statusCode, expectedResponse.statusCode);
  Assert.equal(ohttpResponse.error, expectedResponse.error);
}

add_task(async function run_tests() {
  await test_success();
  await test_invalid_config();
  await test_ohttp_failure();
});
