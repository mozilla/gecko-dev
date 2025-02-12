/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Check if the position is within this function location
 *
 * @param {Object} functionLocation
 * @param {Object} position
 * @returns {Boolean}
 */
function containsPosition(functionLocation, position) {
  return (
    functionLocation.startLine <= position.line &&
    functionLocation.endLine >= position.line
  );
}

function containsLocation(parentLocation, childLocation) {
  return (
    parentLocation.startLine <= childLocation.startLine &&
    parentLocation.endLine >= childLocation.endLine
  );
}

function getInnerLocations(locations, position) {
  // First, find the function which  directly contains the specified position (line / column)
  let parentIndex;
  for (let i = locations.length - 1; i >= 0; i--) {
    if (containsPosition(locations[i], position)) {
      parentIndex = i;
      break;
    }
  }

  if (parentIndex == undefined) {
    return [];
  }

  const parentLoc = locations[parentIndex];

  // Then, from the nearest location, loop locations again and put locations into
  // the innerLocations array until we get to a location not enclosed by the nearest location.
  const innerLocations = [];
  for (let i = parentIndex + 1; i < locations.length; i++) {
    const loc = locations[i];
    if (!containsLocation(parentLoc, loc)) {
      break;
    }
    innerLocations.push(loc);
  }

  return innerLocations;
}

/**
 * Sort based on the start line
 *
 * @param {Array} locations
 * @returns
 */
function sortByStart(locations) {
  return locations.sort((a, b) => {
    if (a.startLine < b.startLine) {
      return -1;
    } else if (a.startLine === b.startLine) {
      return b.endLine - a.endLine;
    }
    return 1;
  });
}

/**
 * Return a new locations array which excludes
 * items that are completely enclosed by another location in the input locations
 *
 * @param locations Notice! The locations MUST be sorted by `sortByStart`
 *                  so that we can do linear time complexity operation.
 */
function removeOverlapLocations(locations) {
  if (!locations.length) {
    return [];
  }
  const firstParent = locations[0];
  return locations.reduce(deduplicateNode, [firstParent]);
}

function deduplicateNode(nodes, location) {
  const parent = nodes[nodes.length - 1];
  if (!containsLocation(parent, location)) {
    nodes.push(location);
  }
  return nodes;
}

function getOutOfScopeLines(outOfScopeLocations) {
  if (!outOfScopeLocations) {
    return new Set();
  }

  const uniqueLines = new Set();
  for (const location of outOfScopeLocations) {
    for (let i = location.startLine; i < location.endLine; i++) {
      uniqueLines.add(i);
    }
  }

  return uniqueLines;
}

module.exports = {
  containsPosition,
  containsLocation,
  getInnerLocations,
  removeOverlapLocations,
  getOutOfScopeLines,
  sortByStart,
};
