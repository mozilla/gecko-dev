/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test the invocation of viewport infobar on window resize with inspector open, and make sure if resizes correctly

const TEST_URL =
  "data:text/html;charset=utf-8," +
  "<div style='position:absolute;left: 0; top: 0; " +
  "width: 20px; height: 50px'></div>";

const ID = "viewport-size-on-resize-highlighter-";
const { TYPES } = ChromeUtils.importESModule(
  "resource://devtools/shared/highlighters.mjs"
);

requestLongerTimeout(2);

add_task(async function () {
  // On some slow build (e.g. tsan), the time we actually check for highlighter attributes,
  // the highlighter is already hidden because of the default 1s timeout.
  // Bump the timeout a bit so we have more time to check for things
  await pushPref("devtools.highlighter-viewport-size-timeout", 2500);
  await pushPref("devtools.command-button-rulers.enabled", true);

  const {
    Toolbox,
  } = require("resource://devtools/client/framework/toolbox.js");
  const { inspector, highlighterTestFront } = await openInspectorForURL(
    TEST_URL,
    Toolbox.HostType.BOTTOM
  );
  const front = inspector.inspectorFront;
  const highlighterFront = await waitFor(() =>
    front.getKnownHighlighter(TYPES.VIEWPORT_SIZE_ON_RESIZE)
  );

  // check that right after opening inspector the viewport infobar is not visible
  await isInfobarVisible(highlighterFront, highlighterTestFront, {
    expectVisible: false,
  });

  // cause resize event to show viewport infobar
  await resizeInspector(inspector, Toolbox.HostType.RIGHT);
  await waitForVisible(highlighterFront, highlighterTestFront);
  await hasRightLabelsContent(highlighterFront, highlighterTestFront);

  // wait for the viewport infobar to hide
  await waitForHidden(highlighterFront, highlighterTestFront);
  ok(true, "Highlighter gets hidden");

  await resizeInspector(inspector, Toolbox.HostType.BOTTOM);
  await waitForVisible(highlighterFront, highlighterTestFront);
  await hasRightLabelsContent(highlighterFront, highlighterTestFront);

  // wait for the viewport infobar to fade after 1 second
  await waitForHidden(highlighterFront, highlighterTestFront);
  ok(true, "Highlighter gets hidden");

  info(
    "Show rulers highlighter and check resizing doesn't show VIEWPORT_SIZE_ON_RESIZE"
  );
  const rulersToggleButton = inspector.toolbox.doc.getElementById(
    "command-button-rulers"
  );
  rulersToggleButton.click();
  // wait until we get the highlighter
  const rulersHighlighterFront = await waitFor(() =>
    front.getKnownHighlighter(TYPES.RULERS)
  );
  await waitFor(async () => {
    const hidden = await isRulersHighlighterHidden(
      rulersHighlighterFront,
      highlighterTestFront
    );
    return !hidden;
  });
  ok(true, "Rulers highlighter is visible");

  // resize the inspector and wait for 2s to the VIEWPORT_SIZE_ON_RESIZE highlighter
  // to be displayed.
  await resizeInspector(inspector, Toolbox.HostType.RIGHT);
  try {
    await waitFor(
      async () => {
        const hidden = await isViewportInfobarHidden(
          highlighterFront,
          highlighterTestFront
        );
        return !hidden;
      },
      "",
      // interval
      200,
      // max tries
      10
    );
    ok(
      false,
      "The VIEWPORT_SIZE_ON_RESIZE highlighter shouldn't have been displayed"
    );
  } catch (e) {
    ok(true, "The VIEWPORT_SIZE_ON_RESIZE highlighter wasn't displayed");
  }

  info("Check that once rulers are hidden, VIEWPORT_SIZE_ON_RESIZE works fine");
  rulersToggleButton.click();
  // wait until the rulers are hidden
  await waitFor(async () => {
    const hidden = await isRulersHighlighterHidden(
      rulersHighlighterFront,
      highlighterTestFront
    );
    return hidden;
  });
  ok(true, "Rulers highlighter is hidden");

  await resizeInspector(inspector, Toolbox.HostType.BOTTOM);
  await waitForVisible(highlighterFront, highlighterTestFront);
  await hasRightLabelsContent(highlighterFront, highlighterTestFront);

  // wait for the viewport infobar to fade after 1 second
  await waitForHidden(highlighterFront, highlighterTestFront);
  ok(true, "Highlighter gets hidden");

  await highlighterFront.finalize();
});

async function isInfobarVisible(
  highlighterFront,
  highlighterTestFront,
  { expectVisible }
) {
  info(
    `Checking visibility of the viewport infobar, expected visibility: ${expectVisible}`
  );
  const hidden = await isViewportInfobarHidden(
    highlighterFront,
    highlighterTestFront
  );

  if (expectVisible) {
    ok(!hidden, "viewport infobar is visible");
  } else {
    ok(hidden, "viewport infobar is hidden");
  }
}

async function hasRightLabelsContent(highlighterFront, highlighterTestFront) {
  info(`Wait until the viewport infobar has the proper text`);
  await waitFor(async () => {
    // Let compute the viewport size
    const windowDimensions = await SpecialPowers.spawn(
      gBrowser.selectedBrowser,
      [],
      () => {
        const { require } = ChromeUtils.importESModule(
          "resource://devtools/shared/loader/Loader.sys.mjs"
        );
        const {
          getWindowDimensions,
        } = require("resource://devtools/shared/layout/utils.js");
        return getWindowDimensions(content);
      }
    );
    const windowHeight = Math.round(windowDimensions.height);
    const windowWidth = Math.round(windowDimensions.width);
    const windowText = `${windowWidth}px \u00D7 ${windowHeight}px`;

    const dimensionText =
      await highlighterTestFront.getHighlighterNodeTextContent(
        `${ID}viewport-infobar-container`,
        highlighterFront
      );
    return dimensionText == windowText;
  }, 100);
}

async function resizeInspector(inspector, orientation) {
  info(
    `Docking the toolbox to the ${orientation} of the browser to change the window size`
  );
  const toolbox = inspector.toolbox;
  await toolbox.switchHost(orientation);
}

async function waitForVisible(highlighterFront, highlighterTestFront) {
  await waitFor(async () => {
    const hidden = await isViewportInfobarHidden(
      highlighterFront,
      highlighterTestFront
    );
    return !hidden;
  });
  ok(true, "Highlighter is visible after resizing the inspector");
}

async function waitForHidden(highlighterFront, highlighterTestFront) {
  await waitFor(async () => {
    const hidden = await isViewportInfobarHidden(
      highlighterFront,
      highlighterTestFront
    );
    return hidden;
  });
}

async function isViewportInfobarHidden(highlighterFront, highlighterTestFront) {
  const hidden = await highlighterTestFront.getHighlighterNodeAttribute(
    `${ID}viewport-infobar-container`,
    "hidden",
    highlighterFront
  );
  return hidden === "true";
}

async function isRulersHighlighterHidden(
  rulersHighlighterFront,
  highlighterTestFront
) {
  const hidden = await highlighterTestFront.getHighlighterNodeAttribute(
    "rulers-highlighter-elements",
    "hidden",
    rulersHighlighterFront
  );
  return hidden === "true";
}
