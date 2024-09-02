/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test toggling the grid highlighter in the rule view from an overridden 'display: grid'
// declaration.

const TEST_URI = `
  <style type='text/css'>
    #grid {
      display: grid;
    }
    div, ul {
      display: grid;
    }
  </style>
  <ul id="grid">
    <li id="cell1">cell1</li>
    <li id="cell2">cell2</li>
  </ul>
`;

add_task(async function () {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, view } = await openRuleView();
  const highlighters = view.highlighters;
  const HIGHLIGHTER_TYPE = inspector.highlighters.TYPES.GRID;
  const { waitForHighlighterTypeShown, waitForHighlighterTypeHidden } =
    getHighlighterTestHelpers(inspector);

  await selectNode("#grid", inspector);
  const container = getRuleViewProperty(view, "#grid", "display").valueSpan;
  const gridToggle = container.querySelector(".js-toggle-grid-highlighter");
  const overriddenContainer = getRuleViewProperty(
    view,
    "div, ul",
    "display"
  ).valueSpan;
  const overriddenGridToggle =
    overriddenContainer.querySelector(".inspector-grid");

  info("Checking the initial state of the CSS grid toggle in the rule-view.");
  ok(
    !gridToggle.hasAttribute("disabled"),
    "Grid highlighter toggle is not disabled."
  );
  ok(
    !overriddenGridToggle.hasAttribute("disabled"),
    "Grid highlighter toggle is not disabled."
  );
  is(
    gridToggle.getAttribute("aria-pressed"),
    "false",
    "Grid highlighter toggle buttons is not active…"
  );
  is(
    overriddenGridToggle.getAttribute("aria-pressed"),
    "false",
    "…and overridden Grid highlighter toggle buttons is not active either"
  );
  ok(!highlighters.gridHighlighters.size, "No CSS grid highlighter is shown.");

  info(
    "Toggling ON the CSS grid highlighter from the overridden rule in the rule-view."
  );
  const onHighlighterShown = waitForHighlighterTypeShown(HIGHLIGHTER_TYPE);
  overriddenGridToggle.click();
  await onHighlighterShown;

  info(
    "Checking the CSS grid highlighter is created and toggle buttons are active in " +
      "the rule-view."
  );
  is(
    gridToggle.getAttribute("aria-pressed"),
    "true",
    "Grid highlighter toggle buttons is active…"
  );
  is(
    overriddenGridToggle.getAttribute("aria-pressed"),
    "true",
    "…and overridden Grid highlighter toggle buttons is also active"
  );
  is(highlighters.gridHighlighters.size, 1, "CSS grid highlighter is shown.");

  info(
    "Toggling off the CSS grid highlighter from the normal grid declaration in the " +
      "rule-view."
  );
  const onHighlighterHidden = waitForHighlighterTypeHidden(HIGHLIGHTER_TYPE);
  gridToggle.click();
  await onHighlighterHidden;

  info(
    "Checking the CSS grid highlighter is not shown and toggle buttons are not " +
      "active in the rule-view."
  );
  is(
    gridToggle.getAttribute("aria-pressed"),
    "false",
    "Grid highlighter toggle buttons is not active…"
  );
  is(
    overriddenGridToggle.getAttribute("aria-pressed"),
    "false",
    "…and overridden Grid highlighter toggle buttons is not active either"
  );
  ok(!highlighters.gridHighlighters.size, "No CSS grid highlighter is shown.");
});
