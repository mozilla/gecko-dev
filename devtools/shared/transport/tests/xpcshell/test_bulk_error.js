/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function run_test() {
  initTestDevToolsServer();
  add_test_bulk_actor();

  add_task(async function () {
    await test_string_error(socket_transport);
    await test_string_error(local_transport);
    DevToolsServer.destroy();
  });

  run_next_test();
}

/** * Sample Bulk Actor ***/
const protocol = require("resource://devtools/shared/protocol.js");
const { Arg, RetVal } = protocol;
const { Actor } = require("resource://devtools/shared/protocol/Actor.js");

const testBulkSpec = protocol.generateActorSpec({
  typeName: "testBulk",

  methods: {
    jsonReply: {
      request: protocol.BULK_REQUEST,
      response: { allDone: RetVal("number") },
    },
    bulkReply: {
      request: { foo: Arg(0, "number") },
      response: protocol.BULK_RESPONSE,
    },
  },
});

class TestBulkActor extends Actor {
  constructor(conn) {
    super(conn, testBulkSpec);
  }

  jsonReply({ length }) {
    Assert.equal(length, really_long().length);

    return {
      allDone: true,
    };
  }

  async bulkReply({ foo }) {
    Assert.equal(foo, 42, "received the bulk reply request");

    throw new Error("actor exception");
  }
}

class TestBulkFront extends protocol.FrontClassWithSpec(testBulkSpec) {
  formAttributeName = "testBulk";
  form(form) {
    this.actor = form.actor;
  }
}
protocol.registerFront(TestBulkFront);

function add_test_bulk_actor() {
  ActorRegistry.addGlobalActor(
    {
      constructorName: "TestBulkActor",
      constructorFun: TestBulkActor,
    },
    "testBulk"
  );
}

/** * Tests ***/

var test_string_error = async function (transportFactory) {
  const transport = await transportFactory();

  const client = new DevToolsClient(transport);
  await client.connect();
  await client.mainRoot.rootForm;
  const front = await client.mainRoot.getFront("testBulk");

  try {
    await front.bulkReply({ foo: 42 });
    Assert.ok(false, "bulkReply should have thrown");
  } catch (e) {
    Assert.stringContains(e.message, "actor exception");
  }

  const reallyLong = really_long();

  const input = Cc["@mozilla.org/io/string-input-stream;1"].createInstance(
    Ci.nsIStringInputStream
  );
  input.setByteStringData(reallyLong);

  const { promise, resolve } = Promise.withResolvers();
  function bulkSendReadyCallback({ writer, done }) {
    try {
      // Send bulk data to server
      writer.copyFrom(input, () => {
        input.close();
        done();
      });
      do_throw(new Error("Copying should fail, the stream is not async."));
    } catch (e) {
      Assert.ok(true);
      resolve();
    }
  }
  await front.jsonReply({ length: reallyLong.length }, bulkSendReadyCallback);
  await promise;

  client.close();
  transport.close();
};
