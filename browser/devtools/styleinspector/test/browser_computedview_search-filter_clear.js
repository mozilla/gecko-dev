/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the search filter clear button works properly.

let TEST_URI = [
  '<style type="text/css">',
  '  .matches {',
  '    color: #F00;',
  '  }',
  '</style>',
  '<span id="matches" class="matches">Some styled text</span>'
].join("\n");

add_task(function*() {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {toolbox, inspector, view} = yield openComputedView();
  yield selectNode("#matches", inspector);
  yield testAddTextInFilter(inspector, view);
  yield testClearSearchFilter(inspector, view);
});

function* testAddTextInFilter(inspector, computedView) {
  info("Setting filter text to \"background-color\"");

  let win = computedView.styleWindow;
  let doc = computedView.styleDocument;
  let propertyViews = computedView.propertyViews;
  let searchField = computedView.searchField;
  let checkbox = computedView.includeBrowserStylesCheckbox;

  info("Include browser styles");
  checkbox.click();
  yield inspector.once("computed-view-refreshed");

  searchField.focus();
  synthesizeKeys("background-color", win);
  yield inspector.once("computed-view-refreshed");

  info("Check that the correct properties are visible");

  propertyViews.forEach((propView) => {
    let name = propView.name;
    is(propView.visible, name.indexOf("background-color") > -1,
      "span " + name + " property visibility check");
  });
}

function* testClearSearchFilter(inspector, computedView) {
  info("Clearing the search filter");

  let win = computedView.styleWindow;
  let doc = computedView.styleDocument;
  let propertyViews = computedView.propertyViews;
  let searchField = computedView.searchField;
  let searchClearButton = computedView.searchClearButton;
  let onRefreshed = inspector.once("computed-view-refreshed");

  EventUtils.synthesizeMouseAtCenter(searchClearButton, {}, win);
  yield onRefreshed;

  info("Check that the correct properties are visible");

  ok(!searchField.value, "Search filter is cleared");
  propertyViews.forEach((propView) => {
    let name = propView.name;
    is(propView.visible, true,
      "span " + name + " property is visible");
  });
}
