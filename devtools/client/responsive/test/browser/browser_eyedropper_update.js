/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that the eyedropper's underlying screenshot is updated
// after switchig to RDM and resizing the viewport. See Bug 1295057.

const TEST_URL =
  "data:text/html;charset=utf-8," +
  `
  <head>
    <meta name='viewport' content='width=device-width' />
    <style>
      div {
        width: 200px;
        height: 100px;
        position: absolute;
      }
    </style>
  </head>
  <body>
    <div style='right: 200px; background-color: green;'></div>
    <div style='right: 0px; background-color: blue;'></div>
  </body>
  `;

add_task(async function () {
  info(
    "Test that the eyedropper works after switching to RDM and resizing the viewport."
  );

  const { inspector, highlighterTestFront } =
    await openInspectorForURL(TEST_URL);

  await openEyeDropper(inspector, highlighterTestFront);
  await moveMouse(50, 50);
  await waitForEyedropperColor(highlighterTestFront, "#ffffff");

  info("Switch to RDM");
  const { ui } = await openRDM(gBrowser.selectedTab);
  await waitForEyedropperColor(highlighterTestFront, "#008000");

  info("Resize the viewport");
  await changeViewportWidth(200, ui);
  await waitForEyedropperColor(highlighterTestFront, "#0000ff");

  info("Check that the picked color is copied to the clipboard");
  ui.getViewportBrowser().focus();
  await waitForClipboardPromise(
    () => EventUtils.synthesizeKey("KEY_Enter"),
    "#0000ff"
  );
});

async function openEyeDropper(inspector, highlighterTestFront) {
  info("Opening the eyedropper");
  const toggleButton = inspector.panelDoc.querySelector(
    "#inspector-eyedropper-toggle"
  );
  toggleButton.click();
  await TestUtils.waitForCondition(() =>
    highlighterTestFront.isEyeDropperVisible()
  );
}

async function moveMouse(x, y) {
  info(`Moving mouse to (${x}, ${y})`);
  await BrowserTestUtils.synthesizeMouse(
    "html",
    x,
    y,
    { type: "mousemove", isSynthesized: false },
    gBrowser.selectedBrowser
  );
}

async function waitForEyedropperColor(highlighterTestFront, expectedColor) {
  await waitFor(async () => {
    const color = await highlighterTestFront.getEyeDropperColorValue();
    return color === expectedColor;
  }, `Wait for the eyedropper color to be ${expectedColor}`);
}

async function changeViewportWidth(width, ui) {
  info(`Changing viewport width to ${width}`);

  const { Simulate } = ui.toolWindow.require(
    "resource://devtools/client/shared/vendor/react-dom-test-utils.js"
  );

  const widthInput = ui.toolWindow.document.querySelector(
    ".text-input.viewport-dimension-input"
  );
  widthInput.focus();
  widthInput.value = width;
  Simulate.change(widthInput);
  EventUtils.synthesizeKey("KEY_Enter");
}
