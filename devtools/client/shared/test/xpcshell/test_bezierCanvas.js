/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests the BezierCanvas API in the CubicBezierWidget module

var {
  CubicBezier,
  BezierCanvas,
} = require("resource://devtools/client/shared/widgets/CubicBezierWidget.js");

function run_test() {
  const doc = Services.appShell.createWindowlessBrowser(false).document;
  const canvas = doc.createElement("canvas");
  canvas.setAttribute("width", 200);
  canvas.setAttribute("height", 400);
  doc.body.appendChild(canvas);

  offsetsGetterReturnsData(canvas);
  convertsOffsetsToCoordinates(canvas);
  plotsCanvas(canvas);
}

function offsetsGetterReturnsData(canvas) {
  info("offsets getter returns an array of 2 offset objects");

  let b = new BezierCanvas(canvas, getCubicBezier(), [0.25, 0]);
  let offsets = b.offsets;

  Assert.equal(offsets.length, 2);

  Assert.ok("top" in offsets[0]);
  Assert.ok("left" in offsets[0]);
  Assert.ok("top" in offsets[1]);
  Assert.ok("left" in offsets[1]);

  Assert.equal(offsets[0].top, "300px");
  Assert.equal(offsets[0].left, "0px");
  Assert.equal(offsets[1].top, "100px");
  Assert.equal(offsets[1].left, "200px");

  info("offsets getter returns data according to current padding");

  b = new BezierCanvas(canvas, getCubicBezier(), [0, 0]);
  offsets = b.offsets;

  Assert.equal(offsets[0].top, "400px");
  Assert.equal(offsets[0].left, "0px");
  Assert.equal(offsets[1].top, "0px");
  Assert.equal(offsets[1].left, "200px");
}

function convertsOffsetsToCoordinates(canvas) {
  info("Converts offsets to coordinates");

  const b = new BezierCanvas(canvas, getCubicBezier(), [0.25, 0]);

  let coordinates = b.offsetsToCoordinates({
    style: {
      left: "0px",
      top: "0px",
    },
  });
  Assert.equal(coordinates.length, 2);
  Assert.equal(coordinates[0], 0);
  Assert.equal(coordinates[1], 1.5);

  coordinates = b.offsetsToCoordinates({
    style: {
      left: "0px",
      top: "300px",
    },
  });
  Assert.equal(coordinates[0], 0);
  Assert.equal(coordinates[1], 0);

  coordinates = b.offsetsToCoordinates({
    style: {
      left: "200px",
      top: "100px",
    },
  });
  Assert.equal(coordinates[0], 1);
  Assert.equal(coordinates[1], 1);
}

function plotsCanvas(canvas) {
  info("Plots the curve to the canvas");

  let hasDrawnCurve = false;
  const b = new BezierCanvas(canvas, getCubicBezier(), [0.25, 0]);
  b.ctx.bezierCurveTo = () => {
    hasDrawnCurve = true;
  };
  b.plot();

  Assert.ok(hasDrawnCurve);
}

function getCubicBezier() {
  return new CubicBezier([0, 0, 1, 1]);
}
