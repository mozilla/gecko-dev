/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

// Ignore strange errors when shutting down.
PromiseTestUtils.allowMatchingRejectionsGlobally(/No such actor/);

add_task(async function testBasicFrames() {
  const dbg = await initDebugger(
    "doc-script-switching.html",
    "script-switching-01.js",
    "script-switching-02.js"
  );

  const found = findElement(dbg, "callStackBody");
  is(found, null, "Call stack is hidden");

  invokeInTab("firstCall");
  await waitForPaused(dbg);

  const frames = findAllElements(dbg, "frames");
  assertFrameIsSelected(dbg, frames[0], "secondCall");

  info("Assert pause at the correct location");
  await assertPausedAtSourceAndLine(
    dbg,
    findSource(dbg, "script-switching-02.js").id,
    6
  );

  info("Click the second frame and assert the frame and the selected location");
  frames[1].click();
  await assertSelectedLocation(
    dbg,
    findSource(dbg, "script-switching-01.js").id,
    8
  );
  assertFrameIsSelected(dbg, frames[1], "firstCall");

  const button = toggleButton(dbg);
  ok(!button, "toggle button shouldn't be there");

  await resume(dbg);
});

add_task(async function testFrameNavigation() {
  const dbg = await initDebugger("doc-frames.html", "frames.js");

  const source = findSource(dbg, "frames.js");
  invokeInTab("startRecursion");
  await waitForPaused(dbg);

  let frames = findAllElements(dbg, "frames");
  assertFrameIsSelected(dbg, frames[0], "recurseA");
  assertFrameContent(dbg, frames[0], "recurseA", "frames.js:3");

  // check to make sure that the toggle button isn't there
  let button = toggleButton(dbg);

  is(button.innerText, "Expand rows", "toggle button should be 'expand'");
  is(frames.length, 7, "There should be at most seven frames");

  frames[0].focus();

  info("Assert the Home and End Keys on the frame list");
  pressKey(dbg, "End");
  assertFrameContent(
    dbg,
    dbg.win.document.activeElement,
    "recurseA",
    "frames.js:8"
  );

  pressKey(dbg, "Start");
  assertFrameContent(
    dbg,
    dbg.win.document.activeElement,
    "recurseA",
    "frames.js:3"
  );

  info("Assert navigating through the frames");
  pressKey(dbg, "Down");
  is(frames[1], dbg.win.document.activeElement, "The second frame is focused");

  pressKey(dbg, "Down");
  is(frames[2], dbg.win.document.activeElement, "The third frame is focused");

  info(
    "Assert that selecting the third frame jumps to the correct source location"
  );
  pressKey(dbg, "Enter");

  await assertSelectedLocation(dbg, source.id, 8);
  assertFrameIsSelected(dbg, frames[2], "recurseA");
  assertFrameContent(dbg, frames[2], "recurseA", "frames.js:8");

  info("Navigate up and assert the second frame");
  pressKey(dbg, "Up");
  is(
    frames[1],
    dbg.win.document.activeElement,
    "The second frame is now focused"
  );

  info(
    "Assert that selecting the second frame jumps to the correct source location"
  );
  pressKey(dbg, "Enter");

  await assertSelectedLocation(dbg, source.id, 18);
  assertFrameIsSelected(dbg, frames[1], "recurseB");

  button.click();

  button = toggleButton(dbg);
  frames = findAllElements(dbg, "frames");
  is(button.innerText, "Collapse rows", "toggle button should be collapsed");
  is(frames.length, 22, "All of the frames should be shown");
  await waitForSelectedSource(dbg, "frames.js");
});

add_task(async function testGroupFrames() {
  const url = createMockAngularPage();
  const tab = await addTab(url);
  info("Open debugger");
  const toolbox = await openToolboxForTab(tab, "jsdebugger");
  const dbg = createDebuggerContext(toolbox);

  const found = findElement(dbg, "callStackBody");
  is(found, null, "Call stack is hidden");

  const pausedContent = SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [],
    function () {
      content.document.querySelector("button.pause").click();
    }
  );

  await waitForPaused(dbg);
  const group = findElementWithSelector(dbg, ".frames .frames-group");
  is(
    group.querySelector(".badge").textContent,
    "2",
    "Group has expected badge"
  );
  is(
    group.querySelector(".group-description-name").textContent,
    "Angular",
    "Group has expected location"
  );

  info("Expand the frame group");
  group.focus();
  pressKey(dbg, "Enter");

  info("Press arrow to select first frame element");
  pressKey(dbg, "Down");

  info("Assert the Home and End Keys in the Frame Group");
  pressKey(dbg, "End");
  info("The last frame item in the group is not selected");
  assertFrameIsNotSelected(dbg, dbg.win.document.activeElement, "<anonymous>");
  assertFrameContent(dbg, dbg.win.document.activeElement, "<anonymous>");

  pressKey(dbg, "Start");
  info("The group header is focused");
  is(
    dbg.win.document.activeElement.querySelector(".group-description-name")
      .innerText,
    "Angular",
    "The group is correct"
  );

  const frameElements = findAllElements(dbg, "frames");
  is(
    frameElements[0],
    dbg.win.document.activeElement,
    "The first frame is now focused"
  );

  const source = findSource(dbg, "angular.js");
  await assertPausedAtSourceAndLine(dbg, source.id, 4);
  await assertSelectedLocation(dbg, source.id, 4);
  assertFrameIsSelected(dbg, frameElements[1], "<anonymous>");

  info("Select the frame group");
  frameElements[1].focus();
  pressKey(dbg, "Up");

  info(
    "Assert that the frame group does not collapse when a frame group item is selected"
  );
  pressKey(dbg, "Enter");
  const frameGroupHeader = frameElements[1].parentNode.previousElementSibling;
  ok(
    frameGroupHeader.classList.contains("expanded"),
    "The frame group is still expanded"
  );
  is(
    frameGroupHeader.querySelector(".group-description-name").innerText,
    "Angular",
    "The group is correct"
  );
  is(
    frameGroupHeader.title,
    "Select a non-group frame to collapse Angular frames",
    "The group title is correct"
  );

  await resume(dbg);

  info("Wait for content to be resumed");
  await pausedContent;
});

function toggleButton(dbg) {
  const callStackBody = findElement(dbg, "callStackBody");
  return callStackBody.querySelector(".show-more");
}

function assertFrameContent(dbg, element, expectedTitle, expectedLocation) {
  is(
    element.querySelector(".title").innerText,
    expectedTitle,
    "The frame title is correct"
  );

  if (expectedLocation) {
    is(
      element.querySelector(".location").innerText,
      expectedLocation,
      "The location is correct"
    );
  }
}

async function assertSelectedLocation(dbg, sourceId, line) {
  await waitFor(() => {
    const selectedLocation = dbg.selectors.getSelectedLocation();
    return (
      selectedLocation.source.id == sourceId && selectedLocation.line == line
    );
  });
  ok(true, "The correct location is selected");
}

// Create an HTTP server to simulate an angular app with anonymous functions
// and return the URL.
function createMockAngularPage() {
  const httpServer = createTestHTTPServer();

  httpServer.registerContentType("html", "text/html");
  httpServer.registerContentType("js", "application/javascript");

  const htmlFilename = "angular-mock.html";
  httpServer.registerPathHandler(
    `/${htmlFilename}`,
    function (request, response) {
      response.setStatusLine(request.httpVersion, 200, "OK");
      response.write(`
        <html>
            <button class="pause">Click me</button>
            <script type="text/javascript" src="angular.js"></script>
        </html>`);
    }
  );

  // Register an angular.js file in order to create a Group with anonymous functions in
  // the callstack panel.
  httpServer.registerPathHandler("/angular.js", function (request, response) {
    response.setHeader("Content-Type", "application/javascript");
    response.write(`
      document.querySelector("button.pause").addEventListener("click", () => {
        (function() {
          debugger;
        })();
      })
    `);
  });

  const port = httpServer.identity.primaryPort;
  return `http://localhost:${port}/${htmlFilename}`;
}
