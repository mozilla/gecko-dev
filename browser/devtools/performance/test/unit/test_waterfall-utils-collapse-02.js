/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the waterfall collapsing logic works properly for console.time/console.timeEnd
 * markers, as they should ignore any sort of collapsing.
 */

function test() {
  const WaterfallUtils = devtools.require("devtools/performance/waterfall-utils");

  let rootMarkerNode = WaterfallUtils.makeParentMarkerNode({ name: "(root)" });

  WaterfallUtils.collapseMarkersIntoNode({
    markerNode: rootMarkerNode,
    markersList: gTestMarkers
  });

  function compare (marker, expected) {
    for (let prop in expected) {
      if (prop === "submarkers") {
        for (let i = 0; i < expected.submarkers.length; i++) {
          compare(marker.submarkers[i], expected.submarkers[i]);
        }
      } else if (prop !== "uid") {
        is(marker[prop], expected[prop], `${expected.name} matches ${prop}`);
      }
    }
  }

  compare(rootMarkerNode, gExpectedOutput);
  finish();
}

const gTestMarkers = [
  { start: 2, end: 9, name: "Javascript" },
    { start: 3, end: 4, name: "Paint" },
  // Time range starting in nest, ending outside
  { start: 5, end: 12, name: "ConsoleTime", causeName: "1" },

  // Time range starting outside of nest, ending inside
  { start: 15, end: 21, name: "ConsoleTime", causeName: "2" },
  { start: 18, end: 22, name: "Javascript" },
    { start: 19, end: 20, name: "Paint" },

  // Time range completely eclipsing nest
  { start: 30, end: 40, name: "ConsoleTime", causeName: "3" },
  { start: 34, end: 39, name: "Javascript" },
    { start: 35, end: 36, name: "Paint" },

  // Time range completely eclipsed by nest
  { start: 50, end: 60, name: "Javascript" },
  { start: 54, end: 59, name: "ConsoleTime", causeName: "4" },
    { start: 56, end: 57, name: "Paint" },
];

const gExpectedOutput = {
  name: "(root)", submarkers: [
    { start: 2, end: 9, name: "Javascript", submarkers: [
      { start: 3, end: 4, name: "Paint" }
    ]},
    { start: 5, end: 12, name: "ConsoleTime", causeName: "1" },

    { start: 15, end: 21, name: "ConsoleTime", causeName: "2" },
    { start: 18, end: 22, name: "Javascript", submarkers: [
      { start: 19, end: 20, name: "Paint" }
    ]},
    
    { start: 30, end: 40, name: "ConsoleTime", causeName: "3" },
    { start: 34, end: 39, name: "Javascript", submarkers: [
      { start: 35, end: 36, name: "Paint" },
    ]},

    { start: 50, end: 60, name: "Javascript", submarkers: [
      { start: 56, end: 57, name: "Paint" },
    ]},
    { start: 54, end: 59, name: "ConsoleTime", causeName: "4" },
]};
