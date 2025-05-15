/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { getSelectedLocation } from "./selected-location";
import { getSource } from "../selectors/index";

/**
 * Note that arguments can be created via `createLocation`.
 * But they can also be created via `createPendingLocation` in reducer/pending-breakpoints.js.
 * Both will have similar line and column attributes.
 */
export function comparePosition(a, b) {
  return a && b && a.line == b.line && a.column == b.column;
}

export function createLocation({
  source,
  sourceActor = null,

  // Line 0 represents no specific line chosen for action
  line = 0,
  column,
}) {
  return {
    source,
    sourceActor,
    sourceActorId: sourceActor?.id,

    // # Quick overview of 1-based versus 0-based lines and columns #
    //
    // In the Debugger frontend, we use these location objects to refer to a precise line and column.
    // Locations objects use 1-based `line` and 0-based `column`.
    //
    // In the frontend, the Source Map library, as well as CodeMirror 6, both match the location objects convention
    // and use 1-based for lines and 0-based for columns.
    // CodeMirror 5 uses 0-based lines and 0-based columns.
    //
    // This also matches RDP conventions.
    // Breakpoints sent to the RDP server and breakable positions fetched from the RDP server
    // are using 1-based lines and 0-based columns.
    //
    // But within the RDP server, there is a mapping between RDP packets and Spidermonkey
    // as Spidermonkey use 1-based lines **and** columns.
    // This data is mostly coming from and driven by
    // JSScript::lineno and JSScript::column
    // https://searchfox.org/mozilla-central/rev/4c065f1df299065c305fb48b36cdae571a43d97c/js/src/vm/JSScript.h#1567-1570
    //
    // Spidermonkey also matches the lines and columns mentioned in tests to assert the (selected) locations.
    // We are using human readeable numbers and both lines and columns are 1-based.
    // This actually matches the numbers displayed in the UI to the user as we always display 1-based numbers.
    line,
    column,
  };
}

/**
 * Convert location objects created via `createLocation` into
 * the format used by the Source Map Loader/Worker.
 * It only needs sourceId, line and column attributes.
 */
export function debuggerToSourceMapLocation(location) {
  return {
    sourceId: location.source.id,
    // In case of errors loading the source, we might not have a precise location.
    // Defaults to first line and column.
    line: location.line || 1,
    column: location.column || 0,
  };
}

/**
 * Pending location only need these three attributes,
 * and especially doesn't need the large source and sourceActor objects of the regular location objects.
 *
 * @param {Object} location
 */
export function createPendingSelectedLocation(location) {
  return {
    url: location.source.url,

    line: location.line,
    column: location.column,
  };
}

export function sortSelectedLocations(locations, selectedSource) {
  return Array.from(locations).sort((locationA, locationB) => {
    const aSelected = getSelectedLocation(locationA, selectedSource);
    const bSelected = getSelectedLocation(locationB, selectedSource);

    // Order the locations by line number…
    if (aSelected.line < bSelected.line) {
      return -1;
    }

    if (aSelected.line > bSelected.line) {
      return 1;
    }

    // … and if we have the same line, we want to return location with undefined columns
    // first, and then order them by column
    if (aSelected.column == bSelected.column) {
      return 0;
    }

    if (aSelected.column === undefined) {
      return -1;
    }

    if (bSelected.column === undefined) {
      return 1;
    }

    return aSelected.column < bSelected.column ? -1 : 1;
  });
}

/**
 * Source map Loader/Worker and debugger frontend don't use the same objects for locations.
 * Worker uses 'sourceId' attributes whereas the frontend has 'source' attribute.
 */
export function sourceMapToDebuggerLocation(state, location) {
  // From MapScopes modules, we might re-process the exact same location objects
  // for which we would already have computed the source object,
  // and which would lack sourceId attribute.
  if (location.source) {
    return location;
  }

  // SourceMapLoader doesn't known about debugger's source objects
  // so that we have to fetch it from here
  const source = getSource(state, location.sourceId);
  if (!source) {
    throw new Error(`Could not find source-map source ${location.sourceId}`);
  }

  return createLocation({
    ...location,
    source,
  });
}
