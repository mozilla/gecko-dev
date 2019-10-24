/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the Page navigation events

const INITIAL_DOC = toDataURL("default-test-page");
const RANDOM_ID_DOC = toDataURL(
  `<script>window.randomId = Math.random() + "-" + Date.now();</script>`
);

const promises = new Set();
const resolutions = new Map();

add_task(async function(client) {
  await loadURL(INITIAL_DOC);

  const { Page } = client;

  // turn on navigation related events, such as DOMContentLoaded et al.
  await Page.enable();
  ok(true, "Page domain has been enabled");

  const { frameTree } = await Page.getFrameTree();
  ok(!!frameTree.frame, "getFrameTree exposes one frame");
  is(frameTree.childFrames.length, 0, "getFrameTree reports no child frame");
  ok(frameTree.frame.id, "getFrameTree's frame has an id");
  is(
    frameTree.frame.url,
    INITIAL_DOC,
    "getFrameTree's frame has the right url"
  );

  // Save the given `promise` resolution into the `promises` global Set
  function recordPromise(name, promise) {
    promise.then(event => {
      ok(true, `Received Page.${name}`);
      resolutions.set(name, event);
    });
    promises.add(promise);
  }
  // Record all Page events that we assert in this test
  function recordPromises() {
    recordPromise("frameStoppedLoading", Page.frameStoppedLoading());
    recordPromise("navigatedWithinDocument", Page.navigatedWithinDocument());
    recordPromise("domContentEventFired", Page.domContentEventFired());
    recordPromise("loadEventFired", Page.loadEventFired());
    recordPromise("frameNavigated", Page.frameNavigated());
  }

  info("Test Page.navigate");
  recordPromises();

  const url = RANDOM_ID_DOC;
  const { frameId } = await Page.navigate({ url });
  ok(true, "A new page has been loaded");

  ok(frameId, "Page.navigate returned a frameId");
  is(
    frameId,
    frameTree.frame.id,
    "The Page.navigate's frameId is the same than getFrameTree's one"
  );

  await assertNavigationEvents({ url, frameId });

  const randomId1 = await getTestTabRandomId();
  ok(!!randomId1, "Test tab has a valid randomId");

  info("Test Page.reload");
  recordPromises();

  await Page.reload();
  ok(true, "The page has been reloaded");

  await assertNavigationEvents({ url, frameId });

  const randomId2 = await getTestTabRandomId();
  ok(!!randomId2, "Test tab has a valid randomId");
  isnot(
    randomId2,
    randomId1,
    "Test tab randomId has been updated after reload"
  );

  info("Test Page.navigate with the same URL still reloads the current page");
  recordPromises();

  await Page.navigate({ url });
  ok(true, "The page has been reloaded");

  await assertNavigationEvents({ url, frameId });

  const randomId3 = await getTestTabRandomId();
  ok(!!randomId3, "Test tab has a valid randomId");
  isnot(
    randomId3,
    randomId2,
    "Test tab randomId has been updated after reload"
  );
});

async function assertNavigationEvents({ url, frameId }) {
  // Wait for all the promises to resolve
  await Promise.all(promises);

  // Assert the order in which they resolved
  const expectedResolutions = [
    "frameNavigated",
    "domContentEventFired",
    "loadEventFired",
    "navigatedWithinDocument",
    "frameStoppedLoading",
  ];
  Assert.deepEqual(
    [...resolutions.keys()],
    expectedResolutions,
    "Received various Page navigation events in the expected order"
  );

  // Now assert the data exposed by each of these events
  const frameNavigated = resolutions.get("frameNavigated");
  ok(
    !frameNavigated.frame.parentId,
    "frameNavigated is for the top level document and has a null parentId"
  );
  is(frameNavigated.frame.id, frameId, "frameNavigated id is the right one");
  is(
    frameNavigated.frame.name,
    undefined,
    "frameNavigated name isn't implemented yet"
  );
  is(frameNavigated.frame.url, url, "frameNavigated url is the right one");

  const navigatedWithinDocument = resolutions.get("navigatedWithinDocument");
  is(
    navigatedWithinDocument.frameId,
    frameId,
    "navigatedWithinDocument frameId is the same one"
  );
  is(
    navigatedWithinDocument.url,
    url,
    "navigatedWithinDocument url is the same one"
  );

  const frameStoppedLoading = resolutions.get("frameStoppedLoading");
  is(
    frameStoppedLoading.frameId,
    frameId,
    "frameStoppedLoading frameId is the same one"
  );

  promises.clear();
  resolutions.clear();
}

async function getTestTabRandomId() {
  return ContentTask.spawn(gBrowser.selectedBrowser, {}, function() {
    return content.wrappedJSObject.randomId;
  });
}
