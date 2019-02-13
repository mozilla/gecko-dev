/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Check that the box-model highlighter supports configuration options

const TEST_URL = "data:text/html;charset=utf-8," +
                 "<body style='padding:2em;'>" +
                 "<div style='width:100px;height:100px;padding:2em;border:.5em solid black;margin:1em;'>test</div>" +
                 "</body>";

// Test data format:
// - desc: a string that will be output to the console.
// - options: json object to be passed as options to the highlighter.
// - checkHighlighter: a generator (async) function that should check the
//   highlighter is correct.
const TEST_DATA = [
  {
    desc: "Guides and infobar should be shown by default",
    options: {},
    checkHighlighter: function*(toolbox) {
      let hidden = yield getHighlighterNodeAttribute(toolbox.highlighter,
        "box-model-nodeinfobar-container", "hidden");
      ok(!hidden, "Node infobar is visible");

      hidden = yield getHighlighterNodeAttribute(toolbox.highlighter,
        "box-model-elements", "hidden");
      ok(!hidden, "SVG container is visible");

      for (let side of ["top", "right", "bottom", "left"]) {
        hidden = yield getHighlighterNodeAttribute(toolbox.highlighter,
          "box-model-guide-" + side, "hidden");
        ok(!hidden, side + " guide is visible");
      }
    }
  },
  {
    desc: "All regions should be shown by default",
    options: {},
    checkHighlighter: function*(toolbox) {
      for (let region of ["margin", "border", "padding", "content"]) {
        let {d} = yield getHighlighterRegionPath(region, toolbox.highlighter);
        ok(d, "Region " + region + " has set coordinates");
      }
    }
  },
  {
    desc: "Guides can be hidden",
    options: {hideGuides: true},
    checkHighlighter: function*(toolbox) {
      for (let side of ["top", "right", "bottom", "left"]) {
        let hidden = yield getHighlighterNodeAttribute(toolbox.highlighter,
          "box-model-guide-" + side, "hidden");
        is(hidden, "true", side + " guide has been hidden");
      }
    }
  },
  {
    desc: "Infobar can be hidden",
    options: {hideInfoBar: true},
    checkHighlighter: function*(toolbox) {
      let hidden = yield getHighlighterNodeAttribute(toolbox.highlighter,
        "box-model-nodeinfobar-container", "hidden");
      is(hidden, "true", "nodeinfobar has been hidden");
    }
  },
  {
    desc: "One region only can be shown (1)",
    options: {showOnly: "content"},
    checkHighlighter: function*(toolbox) {
      let {d} = yield getHighlighterRegionPath("margin", toolbox.highlighter);
      ok(!d, "margin region is hidden");

      ({d} = yield getHighlighterRegionPath("border", toolbox.highlighter));
      ok(!d, "border region is hidden");

      ({d} = yield getHighlighterRegionPath("padding", toolbox.highlighter));
      ok(!d, "padding region is hidden");

      ({d} = yield getHighlighterRegionPath("content", toolbox.highlighter));
      ok(d, "content region is shown");
    }
  },
  {
    desc: "One region only can be shown (2)",
    options: {showOnly: "margin"},
    checkHighlighter: function*(toolbox) {
      let {d} = yield getHighlighterRegionPath("margin", toolbox.highlighter);
      ok(d, "margin region is shown");

      ({d} = yield getHighlighterRegionPath("border", toolbox.highlighter));
      ok(!d, "border region is hidden");

      ({d} = yield getHighlighterRegionPath("padding", toolbox.highlighter));
      ok(!d, "padding region is hidden");

      ({d} = yield getHighlighterRegionPath("content", toolbox.highlighter));
      ok(!d, "content region is hidden");
    }
  },
  {
    desc: "Guides can be drawn around a given region (1)",
    options: {region: "padding"},
    checkHighlighter: function*(toolbox) {
      let topY1 = yield getHighlighterNodeAttribute(toolbox.highlighter,
        "box-model-guide-top", "y1");
      let rightX1 = yield getHighlighterNodeAttribute(toolbox.highlighter,
        "box-model-guide-right", "x1");
      let bottomY1 = yield getHighlighterNodeAttribute(toolbox.highlighter,
        "box-model-guide-bottom", "y1");
      let leftX1 = yield getHighlighterNodeAttribute(toolbox.highlighter,
        "box-model-guide-left", "x1");

      let {points} = yield getHighlighterRegionPath("padding", toolbox.highlighter);
      points = points[0];

      is(Math.ceil(topY1), points[0][1], "Top guide's y1 is correct");
      is(Math.floor(rightX1), points[1][0], "Right guide's x1 is correct");
      is(Math.floor(bottomY1), points[2][1], "Bottom guide's y1 is correct");
      is(Math.ceil(leftX1), points[3][0], "Left guide's x1 is correct");
    }
  },
  {
    desc: "Guides can be drawn around a given region (2)",
    options: {region: "margin"},
    checkHighlighter: function*(toolbox) {
      let topY1 = yield getHighlighterNodeAttribute(toolbox.highlighter,
        "box-model-guide-top", "y1");
      let rightX1 = yield getHighlighterNodeAttribute(toolbox.highlighter,
        "box-model-guide-right", "x1");
      let bottomY1 = yield getHighlighterNodeAttribute(toolbox.highlighter,
        "box-model-guide-bottom", "y1");
      let leftX1 = yield getHighlighterNodeAttribute(toolbox.highlighter,
        "box-model-guide-left", "x1");

      let {points} = yield getHighlighterRegionPath("margin", toolbox.highlighter);
      points = points[0];

      is(Math.ceil(topY1), points[0][1], "Top guide's y1 is correct");
      is(Math.floor(rightX1), points[1][0], "Right guide's x1 is correct");
      is(Math.floor(bottomY1), points[2][1], "Bottom guide's y1 is correct");
      is(Math.ceil(leftX1), points[3][0], "Left guide's x1 is correct");
    }
  },
  {
    desc: "When showOnly is used, other regions can be faded",
    options: {showOnly: "margin", onlyRegionArea: true},
    checkHighlighter: function*(toolbox) {
      let h = toolbox.highlighter;

      for (let region of ["margin", "border", "padding", "content"]) {
        let {d} = yield getHighlighterRegionPath(region, h);
        ok(d, "Region " + region + " is shown (it has a d attribute)");

        let faded = yield getHighlighterNodeAttribute(h,
                          "box-model-" + region, "faded");
        if (region === "margin") {
          ok(!faded, "The margin region is not faded");
        } else {
          is(faded, "true", "Region " + region + " is faded");
        }
      }
    }
  },
  {
    desc: "When showOnly is used, other regions can be faded (2)",
    options: {showOnly: "padding", onlyRegionArea: true},
    checkHighlighter: function*(toolbox) {
      let h = toolbox.highlighter;

      for (let region of ["margin", "border", "padding", "content"]) {
        let {d} = yield getHighlighterRegionPath(region, h);
        ok(d, "Region " + region + " is shown (it has a d attribute)");

        let faded = yield getHighlighterNodeAttribute(h,
                          "box-model-" + region, "faded");
        if (region === "padding") {
          ok(!faded, "The padding region is not faded");
        } else {
          is(faded, "true", "Region " + region + " is faded");
        }
      }
    }
  }
];

add_task(function*() {
  let {inspector, toolbox} = yield openInspectorForURL(TEST_URL);

  let divFront = yield getNodeFront("div", inspector);

  for (let {desc, options, checkHighlighter} of TEST_DATA) {
    info("Running test: " + desc);

    info("Show the box-model highlighter with options " + options);
    yield toolbox.highlighter.showBoxModel(divFront, options);

    yield checkHighlighter(toolbox);

    info("Hide the box-model highlighter");
    yield toolbox.highlighter.hideBoxModel();
  }
});
