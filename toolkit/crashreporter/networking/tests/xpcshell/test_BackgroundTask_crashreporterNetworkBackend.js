/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * vim: sw=4 ts=4 sts=4 et
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { BackgroundTasksTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/BackgroundTasksTestUtils.sys.mjs"
);
const { HttpServer } = ChromeUtils.importESModule(
  "resource://testing-common/httpd.sys.mjs"
);
const { CommonUtils } = ChromeUtils.importESModule(
  "resource://services-common/utils.sys.mjs"
);

BackgroundTasksTestUtils.init(this);

const server = new HttpServer();

// Setup temporary file creation.
const tempFilePaths = [];
registerCleanupFunction(() =>
  Promise.all(tempFilePaths.map(p => IOUtils.remove(p)))
);
async function tempTestFile(prefix) {
  const file = await IOUtils.createUniqueFile(PathUtils.tempDir, prefix);
  tempFilePaths.push(file);
  return file;
}

// Send a request to the given path using the background task.
async function sendRequest(path, request) {
  return await BackgroundTasksTestUtils.do_backgroundtask(
    "crashreporterNetworkBackend",
    {
      extraArgs: [
        `http://localhost:${server.identity.primaryPort}${path}`,
        "testUserAgent/1",
        request,
      ],
    }
  );
}

const EXTRA_DATA = JSON.stringify({
  Vendor: "FooCorp",
  ProductName: "Bar",
  ReleaseChannel: "release",
  BuildID: "1234",
  Version: "100.0",
});

server.registerPathHandler("/mime_post", (request, response) => {
  response.processAsync();
  (async () => {
    Assert.equal(request.getHeader("User-Agent"), "testUserAgent/1");
    Assert.equal(request.method, "POST");
    const body = CommonUtils.readBytesFromInputStream(request.bodyInputStream);
    const data = await new Response(body, {
      headers: { "Content-Type": request.getHeader("Content-Type") },
    }).formData();

    const extra = data.get("extra");
    Assert.equal(extra.name, "extra.json");
    Assert.equal(extra.type, "application/json");
    Assert.equal(await extra.text(), EXTRA_DATA);
    data.delete("extra");

    const minidump = data.get("upload_file_minidump");
    Assert.equal(await minidump.text(), "MINIDUMPDATA");
    data.delete("upload_file_minidump");

    Assert.equal(Array.from(data.entries()).length, 0);
  })().then(() => {
    response.write("CrashID=abcdef");
    response.finish();
  });
});

add_task(async function test_mime_post() {
  const minidumpFile = await tempTestFile("mime-post-request-minidump");
  await IOUtils.writeUTF8(minidumpFile, "MINIDUMPDATA");
  const requestFile = await tempTestFile("mime-post-request");
  await IOUtils.writeJSON(requestFile, {
    type: "MimePost",
    parts: [
      {
        name: "extra",
        content: {
          type: "String",
          value: EXTRA_DATA,
        },
        filename: "extra.json",
        mime_type: "application/json",
      },
      {
        name: "upload_file_minidump",
        content: {
          type: "File",
          value: minidumpFile,
        },
      },
    ],
  });

  const exitCode = await sendRequest("/mime_post", requestFile);
  Assert.equal(exitCode, 0);
  const content = await IOUtils.readUTF8(requestFile);
  Assert.equal(content, "CrashID=abcdef");
});

server.registerPathHandler("/post", (request, response) => {
  response.processAsync();
  Assert.equal(request.method, "POST");
  Assert.equal(request.getHeader("User-Agent"), "testUserAgent/1");
  Assert.equal(request.getHeader("Foo"), "Bar");
  const body = CommonUtils.readBytesFromInputStream(request.bodyInputStream);
  Assert.deepEqual(Array.from(new TextEncoder().encode(body)), [1, 2, 3, 4, 5]);
  response.write("\x06\x07\x08\x09\x0a");
  response.finish();
});

add_task(async function test_post() {
  const requestFile = await tempTestFile("post-request");
  await IOUtils.writeJSON(requestFile, {
    type: "Post",
    headers: [["Foo", "Bar"]],
    body: [1, 2, 3, 4, 5],
  });
  let exitCode = await sendRequest("/post", requestFile);
  Assert.equal(exitCode, 0);
  const content = await IOUtils.read(requestFile);
  Assert.deepEqual(Array.from(content), [6, 7, 8, 9, 10]);
});

server.registerPathHandler("/fail", (request, response) => {
  response.processAsync();
  response.setStatusLine("1.1", 500, "Server Error");
  response.finish();
});

add_task(async function test_fail() {
  const requestFile = await tempTestFile("fail-request");
  await IOUtils.writeJSON(requestFile, {
    type: "Post",
    headers: [],
    body: [],
  });
  let exitCode = await sendRequest("/fail", requestFile);
  Assert.equal(exitCode, 1);
});

// Start http server
server.start(-1);
registerCleanupFunction(
  () => new Promise((resolve, _reject) => server.stop(resolve))
);
