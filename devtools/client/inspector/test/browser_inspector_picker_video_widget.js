/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test that the node picker will not trigger video controls when selecting a
// video element.

const TEST_URI = "data:text/html,<video controls>";

add_task(async function () {
  const { inspector, toolbox } = await openInspectorForURL(TEST_URI);
  const body = await getNodeFront("body", inspector);
  is(
    inspector.selection.nodeFront,
    body,
    "By default the body node is selected"
  );

  info("Start the element picker");
  await startPicker(toolbox);

  info("Listen to 'play' events on the <video> element of the test page");
  await ContentTask.spawn(gBrowser.selectedBrowser, null, () => {
    content.wrappedJSObject.testVideoPlayed = false;
    content.document.querySelector("video").addEventListener("play", () => {
      content.wrappedJSObject.testVideoPlayed = true;
    });
  });

  const onSelectionChanged = inspector.once("inspector-updated");
  await safeSynthesizeMouseEventAtCenterInContentPage("video");
  await onSelectionChanged;

  const videoNodeFront = await getNodeFront("video", inspector);
  is(
    inspector.selection.nodeFront,
    videoNodeFront,
    "The video element is now selected"
  );

  const played = await ContentTask.spawn(
    gBrowser.selectedBrowser,
    null,
    () => content.wrappedJSObject.testVideoPlayed
  );
  ok(
    !(played === true),
    "Selecting the video element with the node picker should not play it"
  );
});
