/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the Target domain

add_task(
  async function(_, CDP) {
    // Connect to the server
    const { webSocketDebuggerUrl } = await CDP.Version();
    const client = await CDP({ target: webSocketDebuggerUrl });

    const { Target } = client;
    ok("Target" in client, "Target domain is available");

    const onTargetsCreated = new Promise(resolve => {
      let gotTabTarget = false,
        gotMainTarget = false;
      const unsubscribe = Target.targetCreated(event => {
        if (
          event.targetInfo.type == "page" &&
          event.targetInfo.url == gBrowser.selectedBrowser.currentURI.spec
        ) {
          info("Got the current tab target");
          gotTabTarget = true;
        }
        if (event.targetInfo.type == "browser") {
          info("Got the browser target");
          gotMainTarget = true;
        }
        if (gotTabTarget && gotMainTarget) {
          unsubscribe();
          resolve();
        }
      });
    });

    // Instruct the server to fire Target.targetCreated events
    Target.setDiscoverTargets({ discover: true });

    // Calling `setDiscoverTargets` will dispatch `targetCreated` event for all
    // already opened tabs and the browser target.
    await onTargetsCreated;

    // Create a new target so that the test runs against a fresh new tab
    const targetCreated = Target.targetCreated();
    const { targetId } = await Target.createTarget();
    ok(true, `Target created: ${targetId}`);
    ok(!!targetId, "createTarget returns a non-empty target id");
    const { targetInfo } = await targetCreated;
    is(
      targetId,
      targetInfo.targetId,
      "createTarget and targetCreated refers to the same target id"
    );
    is(targetInfo.type, "page", "The target is a page");

    const attachedToTarget = Target.attachedToTarget();
    const { sessionId } = await Target.attachToTarget({ targetId });
    ok(true, "Target attached");

    const attachedEvent = await attachedToTarget;
    ok(true, "Received Target.attachToTarget event");
    is(
      attachedEvent.sessionId,
      sessionId,
      "attachedToTarget and attachToTarget returns the same session id"
    );
    is(
      attachedEvent.targetInfo.type,
      "page",
      "attachedToTarget creates a tab by default"
    );

    info("Calling Target.sendMessageToTarget");
    const onResponse = Target.receivedMessageFromTarget();
    const id = 1;
    const message = JSON.stringify({
      id,
      method: "Page.navigate",
      params: {
        url: toDataURL("new-page"),
      },
    });
    await Target.sendMessageToTarget({ sessionId, message });

    info("Waiting for Target.receivedMessageToTarget");
    const response = await onResponse;
    is(response.sessionId, sessionId, "The response is from the same session");
    const responseMessage = JSON.parse(response.message);
    is(responseMessage.id, id, "The response is from the same session");
    ok(
      !!responseMessage.result.frameId,
      "received the `frameId` out of `Page.navigate` request"
    );

    await client.close();
    ok(true, "The client is closed");
  },
  { createTab: false }
);
