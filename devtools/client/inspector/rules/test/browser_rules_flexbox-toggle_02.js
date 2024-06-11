/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test toggling the flexbox highlighter in the rule view from an overridden
// 'display: flex' declaration.

const TEST_URI = `
  <style type='text/css'>
    #flex {
      display: flex;
    }
    div, ul {
      display: flex;
    }
  </style>
  <ul id="flex">
    <li>1</li>
    <li>2</li>
  </ul>
`;

add_task(async function () {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, view } = await openRuleView();
  const HIGHLIGHTER_TYPE = inspector.highlighters.TYPES.FLEXBOX;
  const {
    getActiveHighlighter,
    getNodeForActiveHighlighter,
    waitForHighlighterTypeShown,
    waitForHighlighterTypeHidden,
  } = getHighlighterTestHelpers(inspector);

  await selectNode("#flex", inspector);
  const container = getRuleViewProperty(view, "#flex", "display").valueSpan;
  const flexboxToggle = container.querySelector(
    ".js-toggle-flexbox-highlighter"
  );
  const overriddenContainer = getRuleViewProperty(
    view,
    "div, ul",
    "display"
  ).valueSpan;
  const overriddenFlexboxToggle = overriddenContainer.querySelector(
    ".js-toggle-flexbox-highlighter"
  );

  info("Checking the initial state of the flexbox toggle in the rule-view.");
  ok(
    flexboxToggle && overriddenFlexboxToggle,
    "Flexbox highlighter toggles are visible."
  );
  is(
    flexboxToggle.getAttribute("aria-pressed"),
    "false",
    "Flexbox highlighter toggle buttons is not active…"
  );
  is(
    overriddenFlexboxToggle.getAttribute("aria-pressed"),
    "false",
    "… and overriden Flexbox highlighter toggle buttons isn't active either"
  );

  ok(
    !getActiveHighlighter(HIGHLIGHTER_TYPE),
    "No flexbox highlighter exists in the rule-view."
  );
  ok(
    !getNodeForActiveHighlighter(HIGHLIGHTER_TYPE),
    "No flexbox highlighter is shown."
  );

  info(
    "Toggling ON the flexbox highlighter from the overridden rule in the rule-view."
  );
  const onHighlighterShown = waitForHighlighterTypeShown(HIGHLIGHTER_TYPE);
  overriddenFlexboxToggle.click();
  await onHighlighterShown;

  info(
    "Checking the flexbox highlighter is created and toggle buttons are active in " +
      "the rule-view."
  );
  is(
    flexboxToggle.getAttribute("aria-pressed"),
    "true",
    "Flexbox highlighter toggle is active…"
  );
  is(
    overriddenFlexboxToggle.getAttribute("aria-pressed"),
    "true",
    "… and overriden Flexbox highlighter toggle buttons is active as well"
  );

  ok(
    getActiveHighlighter(HIGHLIGHTER_TYPE),
    "Flexbox highlighter created in the rule-view."
  );
  ok(
    getNodeForActiveHighlighter(HIGHLIGHTER_TYPE),
    "Flexbox highlighter is shown."
  );

  info(
    "Toggling off the flexbox highlighter from the normal flexbox declaration in  " +
      "the rule-view."
  );
  const onHighlighterHidden = waitForHighlighterTypeHidden(HIGHLIGHTER_TYPE);
  flexboxToggle.click();
  await onHighlighterHidden;

  info(
    "Checking the flexbox highlighter is not shown and toggle buttons are not " +
      "active in the rule-view."
  );
  is(
    flexboxToggle.getAttribute("aria-pressed"),
    "false",
    "Flexbox highlighter toggle buttons is not active…"
  );
  is(
    overriddenFlexboxToggle.getAttribute("aria-pressed"),
    "false",
    "… and overriden Flexbox highlighter toggle buttons isn't active either"
  );
  ok(
    !getNodeForActiveHighlighter(HIGHLIGHTER_TYPE),
    "No flexbox highlighter is shown."
  );
});
