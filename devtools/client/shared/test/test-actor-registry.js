/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

(function (exports) {
  const Cu = Components.utils;
  const CC = Components.Constructor;

  const { require } = Cu.import("resource://devtools/shared/Loader.jsm", {});
  const { fetch } = require("devtools/shared/DevToolsUtils");
  const defer = require("devtools/shared/defer");
  const { Task } = require("devtools/shared/task");

  const TEST_URL_ROOT = "http://example.com/browser/devtools/client/shared/test/";
  const ACTOR_URL = TEST_URL_ROOT + "test-actor.js";

  // Register a test actor that can operate on the remote document
  exports.registerTestActor = Task.async(function* (client) {
    // First, instanciate ActorRegistryFront to be able to dynamically register an actor
    let deferred = defer();
    client.listTabs(deferred.resolve);
    let response = yield deferred.promise;
    let { ActorRegistryFront } = require("devtools/shared/fronts/actor-registry");
    let registryFront = ActorRegistryFront(client, response);

    // Then ask to register our test-actor to retrieve its front
    let options = {
      type: { tab: true },
      constructor: "TestActor",
      prefix: "testActor"
    };
    let testActorFront = yield registryFront.registerActor(ACTOR_URL, options);
    return testActorFront;
  });

  // Load the test actor in a custom sandbox as we can't use SDK module loader with URIs
  let loadFront = Task.async(function* () {
    let sourceText = yield request(ACTOR_URL);
    const principal = CC("@mozilla.org/systemprincipal;1", "nsIPrincipal")();
    const sandbox = Cu.Sandbox(principal);
    sandbox.exports = {};
    sandbox.require = require;
    Cu.evalInSandbox(sourceText, sandbox, "1.8", ACTOR_URL, 1);
    return sandbox.exports;
  });

  // Ensure fetching a live TabActor form for the targeted app
  // (helps fetching the test actor registered dynamically)
  let getUpdatedForm = function (client, tab) {
    return client.getTab({tab: tab})
                 .then(response => response.tab);
  };

  // Spawn an instance of the test actor for the given toolbox
  exports.getTestActor = Task.async(function* (toolbox) {
    let client = toolbox.target.client;
    return getTestActor(client, toolbox.target.tab, toolbox);
  });

  // Sometimes, we need the test actor before opening or without a toolbox then just
  // create a front for the given `tab`
  exports.getTestActorWithoutToolbox = Task.async(function* (tab) {
    let { DebuggerServer } = require("devtools/server/main");
    let { DebuggerClient } = require("devtools/shared/client/main");

    // We need to spawn a client instance,
    // but for that we have to first ensure a server is running
    if (!DebuggerServer.initialized) {
      DebuggerServer.init();
      DebuggerServer.addBrowserActors();
    }
    let client = new DebuggerClient(DebuggerServer.connectPipe());

    yield client.connect();

    // We also need to make sure the test actor is registered on the server.
    yield exports.registerTestActor(client);

    return getTestActor(client, tab);
  });

  // Fetch the content of a URI
  let request = function (uri) {
    return fetch(uri).then(({ content }) => content);
  };

  let getTestActor = Task.async(function* (client, tab, toolbox) {
    // We may have to update the form in order to get the dynamically registered
    // test actor.
    let form = yield getUpdatedForm(client, tab);

    let { TestActorFront } = yield loadFront();

    return new TestActorFront(client, form, toolbox);
  });
})(this);
