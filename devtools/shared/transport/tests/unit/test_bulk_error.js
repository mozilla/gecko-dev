/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function run_test() {
  initTestDebuggerServer();
  add_test_bulk_actor();

  add_task(function* () {
    yield test_string_error(socket_transport, json_reply);
    yield test_string_error(local_transport, json_reply);
    DebuggerServer.destroy();
  });

  run_next_test();
}

/** * Sample Bulk Actor ***/

function TestBulkActor() {}

TestBulkActor.prototype = {

  actorPrefix: "testBulk",

  jsonReply: function ({length, reader, reply, done}) {
    do_check_eq(length, really_long().length);

    return {
      allDone: true
    };
  }

};

TestBulkActor.prototype.requestTypes = {
  "jsonReply": TestBulkActor.prototype.jsonReply
};

function add_test_bulk_actor() {
  DebuggerServer.addGlobalActor(TestBulkActor);
}

/** * Tests ***/

var test_string_error = Task.async(function* (transportFactory, onReady) {
  let transport = yield transportFactory();

  let client = new DebuggerClient(transport);
  return client.connect().then(([app, traits]) => {
    do_check_eq(traits.bulk, true);
    return client.listTabs();
  }).then(response => {
    return onReady(client, response);
  }).then(() => {
    client.close();
    transport.close();
  });
});

/** * Reply Types ***/

function json_reply(client, response) {
  let reallyLong = really_long();

  let request = client.startBulkRequest({
    actor: response.testBulk,
    type: "jsonReply",
    length: reallyLong.length
  });

  // Send bulk data to server
  let copyDeferred = defer();
  request.on("bulk-send-ready", ({writer, done}) => {
    let input = Cc["@mozilla.org/io/string-input-stream;1"].
                  createInstance(Ci.nsIStringInputStream);
    input.setData(reallyLong, reallyLong.length);
    try {
      writer.copyFrom(input, () => {
        input.close();
        done();
      });
      do_throw(new Error("Copying should fail, the stream is not async."));
    } catch (e) {
      do_check_true(true);
      copyDeferred.resolve();
    }
  });

  return copyDeferred.promise;
}
