/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import {
  hasInScopeLines,
  getSourceTextContent,
  getVisibleSelectedFrame,
} from "../../selectors/index";

import { getSourceLineCount } from "../../utils/source";
import { features } from "../../utils/prefs";

import { isFulfilled } from "../../utils/async-value";

function getOutOfScopeLines(outOfScopeLocations) {
  if (!outOfScopeLocations) {
    return null;
  }

  const uniqueLines = new Set();
  for (const location of outOfScopeLocations) {
    for (let i = location.start.line; i < location.end.line; i++) {
      uniqueLines.add(i);
    }
  }

  return uniqueLines;
}

async function getInScopeLines(location, sourceTextContent, { parserWorker }) {
  let locations = null;
  if (location.line && parserWorker.isLocationSupported(location)) {
    locations = await parserWorker.findOutOfScopeLocations(location);
  }

  const linesOutOfScope = getOutOfScopeLines(locations);
  const sourceNumLines =
    !sourceTextContent || !isFulfilled(sourceTextContent)
      ? 0
      : getSourceLineCount(sourceTextContent.value);

  const noLinesOutOfScope =
    linesOutOfScope == null || linesOutOfScope.size == 0;

  // This operation can be very costly for large files so we sacrifice a bit of readability
  // for performance sake.
  // We initialize an array with a fixed size and we'll directly assign value for lines
  // that are not out of scope. This is much faster than having an empty array and pushing
  // into it.
  const sourceLines = new Array(sourceNumLines);
  for (let i = 0; i < sourceNumLines; i++) {
    const line = i + 1;
    if (noLinesOutOfScope || !linesOutOfScope.has(line)) {
      sourceLines[i] = line;
    }
  }

  // Finally we need to remove any undefined values, i.e. the ones that were matching
  // out of scope lines.
  return sourceLines.filter(i => i != undefined);
}
/**
 * Get and store the in scope lines in the reducer
 * @param {Object} editor - The editor provides an API to retrieve the in scope location
 *                          details based on lezer in CM6.
 * @returns
 */
export function setInScopeLines(editor) {
  return async thunkArgs => {
    const { getState, dispatch } = thunkArgs;
    const visibleFrame = getVisibleSelectedFrame(getState());

    if (!visibleFrame) {
      return;
    }

    const { location } = visibleFrame;
    const sourceTextContent = getSourceTextContent(getState(), location);

    // Ignore if in scope lines have already be computed, or if the selected location
    // doesn't have its content already fully fetched.
    // The ParserWorker will only have the source text content once the source text content is fulfilled.
    if (
      hasInScopeLines(getState(), location) ||
      !sourceTextContent ||
      !isFulfilled(sourceTextContent) ||
      !editor
    ) {
      return;
    }

    const lines =
      features.codemirrorNext && editor
        ? await editor.getInScopeLines(location)
        : await getInScopeLines(location, sourceTextContent, thunkArgs);

    dispatch({
      type: "IN_SCOPE_LINES",
      location,
      lines,
    });
  };
}
