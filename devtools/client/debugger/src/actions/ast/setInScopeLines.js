/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import {
  hasInScopeLines,
  getSourceTextContent,
  getVisibleSelectedFrame,
} from "../../selectors/index";

import { isFulfilled } from "../../utils/async-value";

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

    const lines = await editor.getInScopeLines(location);

    dispatch({
      type: "IN_SCOPE_LINES",
      location,
      lines,
    });
  };
}
