/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var { FileUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/FileUtils.sys.mjs"
);
var Pipe = Components.Constructor("@mozilla.org/pipe;1", "nsIPipe", "init");

add_task(async function () {
  initTestDevToolsServer();
  add_test_bulk_actor();

  dump(" ################# JSON REPLY\n");
  await test_bulk_request_cs(socket_transport, "jsonReply", "json");
  await test_bulk_request_cs(local_transport, "jsonReply", "json");

  dump(" ################# BULK ECHO\n");
  await test_bulk_request_cs(socket_transport, "bulkEcho", "bulk");
  await test_bulk_request_cs(local_transport, "bulkEcho", "bulk");

  dump(" ################# BULK ECHO WITH BUFFER\n");
  await test_bulk_request_cs(socket_transport, "bulkEchoWithBuffer", "bulk");
  await test_bulk_request_cs(local_transport, "bulkEchoWithBuffer", "bulk");

  dump(" ################# BULK REPLY\n");
  await test_json_request_cs(socket_transport, "bulkReply", "bulk");
  await test_json_request_cs(local_transport, "bulkReply", "bulk");

  DevToolsServer.destroy();
});

/** * Sample Bulk Actor ***/
const protocol = require("resource://devtools/shared/protocol.js");
const { Arg, RetVal } = protocol;
const { Actor } = require("resource://devtools/shared/protocol/Actor.js");

const testBulkSpec = protocol.generateActorSpec({
  typeName: "testBulk",

  methods: {
    bulkEcho: {
      request: protocol.BULK_REQUEST,
      response: protocol.BULK_RESPONSE,
    },
    bulkEchoWithBuffer: {
      request: protocol.BULK_REQUEST,
      response: protocol.BULK_RESPONSE,
    },
    bulkReply: {
      request: { foo: Arg(0, "number") },
      response: protocol.BULK_RESPONSE,
    },
    jsonReply: {
      request: protocol.BULK_REQUEST,
      response: { allDone: RetVal("number") },
    },
  },
});

class TestBulkFront extends protocol.FrontClassWithSpec(testBulkSpec) {
  formAttributeName = "testBulk";
  form(form) {
    this.actor = form.actor;
  }
}
protocol.registerFront(TestBulkFront);

class TestBulkActor extends Actor {
  constructor(conn) {
    super(conn, testBulkSpec);
  }

  // Receives data as bulk and respond as bulk
  async bulkEcho({ length, copyTo }, startBulkResponse) {
    Assert.equal(length, really_long().length);

    const { copyFrom } = await startBulkResponse(length);

    // We'll just echo back the same thing
    const pipe = new Pipe(true, true, 0, 0, null);
    copyTo(pipe.outputStream).then(() => {
      pipe.outputStream.close();
    });
    copyFrom(pipe.inputStream).then(() => {
      pipe.inputStream.close();
    });
  }

  async bulkEchoWithBuffer({ length, copyToBuffer }, startBulkResponse) {
    Assert.equal(length, really_long().length);

    const { copyFromBuffer } = await startBulkResponse(length);

    // We'll just echo back the same thing
    const buffer = new ArrayBuffer(length);
    await copyToBuffer(buffer);
    await copyFromBuffer(buffer);
  }

  // Receives data as json and respond as bulk
  async bulkReply({ foo }, startBulkResponse) {
    Assert.equal(foo, 42);

    const { copyFrom } = await startBulkResponse(really_long().length);
    const input = await new Promise(resolve => {
      NetUtil.asyncFetch(
        {
          uri: NetUtil.newURI(getTestTempFile("bulk-input")),
          loadUsingSystemPrincipal: true,
        },
        resolve
      );
    });
    await copyFrom(input);
    input.close();
  }

  // Receives data as bulk and response as json
  async jsonReply({ length, copyTo }) {
    Assert.equal(length, really_long().length);

    const outputFile = getTestTempFile("bulk-output", true);
    outputFile.create(Ci.nsIFile.NORMAL_FILE_TYPE, parseInt("666", 8));

    const output = FileUtils.openSafeFileOutputStream(outputFile);

    await copyTo(output);
    FileUtils.closeSafeFileOutputStream(output);
    await verify_files();

    return 24;
  }
}

function add_test_bulk_actor() {
  ActorRegistry.addGlobalActor(
    {
      constructorName: "TestBulkActor",
      constructorFun: TestBulkActor,
    },
    "testBulk"
  );
}

/** * Reply Handlers ***/

var replyHandlers = {
  async json(reply) {
    // Receive JSON reply from server
    Assert.equal(reply, 24, "The returned JSON value is correct");
  },

  async bulk({ length, copyTo }) {
    // Receive bulk data reply from server
    Assert.equal(length, really_long().length);

    const outputFile = getTestTempFile("bulk-output", true);
    outputFile.create(Ci.nsIFile.NORMAL_FILE_TYPE, parseInt("666", 8));

    const output = FileUtils.openSafeFileOutputStream(outputFile);

    await copyTo(output);
    FileUtils.closeSafeFileOutputStream(output);

    await verify_files();
  },
};

/** * Tests ***/

var test_bulk_request_cs = async function (
  transportFactory,
  frontMethod,
  replyType
) {
  // Ensure test files are not present from a failed run
  cleanup_files();
  writeTestTempFile("bulk-input", really_long());

  let serverResolve;
  const serverDeferred = new Promise(resolve => {
    serverResolve = resolve;
  });

  let bulkCopyResolve;
  const bulkCopyDeferred = new Promise(resolve => {
    bulkCopyResolve = resolve;
  });

  const transport = await transportFactory();

  const client = new DevToolsClient(transport);
  await client.connect();
  await client.mainRoot.rootForm;

  function bulkSendReadyCallback({ copyFrom }) {
    NetUtil.asyncFetch(
      {
        uri: NetUtil.newURI(getTestTempFile("bulk-input")),
        loadUsingSystemPrincipal: true,
      },
      input => {
        copyFrom(input).then(() => {
          input.close();
          bulkCopyResolve();
        });
      }
    );
  }

  const front = await client.mainRoot.getFront("testBulk");
  const response = await front[frontMethod](
    { length: really_long().length },
    bulkSendReadyCallback
  );

  // Set up reply handling for this type
  await replyHandlers[replyType](response);

  const connectionListener = type => {
    if (type === "closed") {
      DevToolsServer.off("connectionchange", connectionListener);
      serverResolve();
    }
  };
  DevToolsServer.on("connectionchange", connectionListener);

  client.close();
  transport.close();

  await Promise.all([bulkCopyDeferred, serverDeferred]);
};

var test_json_request_cs = async function (
  transportFactory,
  frontMethod,
  replyType
) {
  // Ensure test files are not present from a failed run
  cleanup_files();
  writeTestTempFile("bulk-input", really_long());

  let serverResolve;
  const serverDeferred = new Promise(resolve => {
    serverResolve = resolve;
  });

  const transport = await transportFactory();

  const client = new DevToolsClient(transport);
  await client.connect();
  await client.mainRoot.rootForm;
  const front = await client.mainRoot.getFront("testBulk");
  const response = await front[frontMethod]({ foo: 42 });

  await replyHandlers[replyType](response);

  const connectionListener = type => {
    if (type === "closed") {
      DevToolsServer.off("connectionchange", connectionListener);
      serverResolve();
    }
  };
  DevToolsServer.on("connectionchange", connectionListener);
  client.close();
  transport.close();

  return serverDeferred;
};

/** * Test Utils ***/

function verify_files() {
  const reallyLong = really_long();

  const inputFile = getTestTempFile("bulk-input");
  const outputFile = getTestTempFile("bulk-output");

  Assert.equal(inputFile.fileSize, reallyLong.length);
  Assert.equal(outputFile.fileSize, reallyLong.length);

  // Ensure output file contents actually match
  return new Promise(resolve => {
    NetUtil.asyncFetch(
      {
        uri: NetUtil.newURI(getTestTempFile("bulk-output")),
        loadUsingSystemPrincipal: true,
      },
      input => {
        const outputData = NetUtil.readInputStreamToString(
          input,
          reallyLong.length
        );
        // Avoid Assert.strictEqual here so we don't log the contents
        // eslint-disable-next-line mozilla/no-comparison-or-assignment-inside-ok
        Assert.ok(outputData === reallyLong);
        input.close();
        resolve();
      }
    );
  }).then(cleanup_files);
}

function cleanup_files() {
  const inputFile = getTestTempFile("bulk-input", true);
  if (inputFile.exists()) {
    inputFile.remove(false);
  }

  const outputFile = getTestTempFile("bulk-output", true);
  if (outputFile.exists()) {
    outputFile.remove(false);
  }
}
