/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the color in the colorpicker tooltip can be changed several times
// without causing error in various cases:
// - simple single-color property (color)
// - color and image property (background-image)
// - overridden property
// See bug 979292 and bug 980225

const PAGE_CONTENT = [
  '<style type="text/css">',
  '  body {',
  '    color: green;',
  '    background: red url("chrome://global/skin/icons/warning-64.png") no-repeat center center;',
  '  }',
  '  p {',
  '    color: blue;',
  '  }',
  '</style>',
  '<p>Testing the color picker tooltip!</p>'
].join("\n");

add_task(function*() {
  yield addTab("data:text/html;charset=utf-8,rule view color picker tooltip test");
  content.document.body.innerHTML = PAGE_CONTENT;
  let {toolbox, inspector, view} = yield openRuleView();

  yield testSimpleMultipleColorChanges(inspector, view);
  yield testComplexMultipleColorChanges(inspector, view);
  yield testOverriddenMultipleColorChanges(inspector, view);
});

function* testSimpleMultipleColorChanges(inspector, ruleView) {
  yield selectNode("p", inspector);

  info("Getting the <p> tag's color property");
  let swatch = getRuleViewProperty(ruleView, "p", "color").valueSpan
    .querySelector(".ruleview-colorswatch");

  info("Opening the color picker");
  let picker = ruleView.tooltips.colorPicker;
  let onShown = picker.tooltip.once("shown");
  swatch.click();
  yield onShown;

  info("Changing the color several times");
  let colors = [
    {rgba: [0, 0, 0, 1], computed: "rgb(0, 0, 0)"},
    {rgba: [100, 100, 100, 1], computed: "rgb(100, 100, 100)"},
    {rgba: [200, 200, 200, 1], computed: "rgb(200, 200, 200)"}
  ];
  for (let {rgba, computed} of colors) {
    yield simulateColorPickerChange(picker, rgba, {
      element: content.document.querySelector("p"),
      name: "color",
      value: computed
    });
  }
}

function* testComplexMultipleColorChanges(inspector, ruleView) {
  yield selectNode("body", inspector);

  info("Getting the <body> tag's color property");
  let swatch = getRuleViewProperty(ruleView, "body", "background").valueSpan
    .querySelector(".ruleview-colorswatch");

  info("Opening the color picker");
  let picker = ruleView.tooltips.colorPicker;
  let onShown = picker.tooltip.once("shown");
  swatch.click();
  yield onShown;

  info("Changing the color several times");
  let colors = [
    {rgba: [0, 0, 0, 1], computed: "rgb(0, 0, 0)"},
    {rgba: [100, 100, 100, 1], computed: "rgb(100, 100, 100)"},
    {rgba: [200, 200, 200, 1], computed: "rgb(200, 200, 200)"}
  ];
  for (let {rgba, computed} of colors) {
    yield simulateColorPickerChange(picker, rgba, {
      element: content.document.body,
      name: "backgroundColor",
      value: computed
    });
  }

  info("Closing the color picker");
  let onHidden = picker.tooltip.once("hidden");
  picker.tooltip.hide();
  yield onHidden;
}

function* testOverriddenMultipleColorChanges(inspector, ruleView) {
  yield selectNode("p", inspector);

  info("Getting the <body> tag's color property");
  let swatch = getRuleViewProperty(ruleView, "body", "color").valueSpan
    .querySelector(".ruleview-colorswatch");

  info("Opening the color picker");
  let picker = ruleView.tooltips.colorPicker;
  let onShown = picker.tooltip.once("shown");
  swatch.click();
  yield onShown;

  info("Changing the color several times");
  let colors = [
    {rgba: [0, 0, 0, 1], computed: "rgb(0, 0, 0)"},
    {rgba: [100, 100, 100, 1], computed: "rgb(100, 100, 100)"},
    {rgba: [200, 200, 200, 1], computed: "rgb(200, 200, 200)"}
  ];
  for (let {rgba, computed} of colors) {
    yield simulateColorPickerChange(picker, rgba, {
      element: content.document.body,
      name: "color",
      value: computed
    });
    is(content.getComputedStyle(content.document.querySelector("p")).color,
      "rgb(200, 200, 200)", "The color of the P tag is still correct");
  }
}
