/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { loadSourceText } from "./loadSourceText";
import { getEditor } from "../../utils/editor/index";

export function getOriginalFunctionDisplayName(location) {
  return async ({ dispatch }) => {
    // Make sure the source for the symbols exist.
    await dispatch(loadSourceText(location.source, location.sourceActor));
    const editor = getEditor();
    return editor.getClosestFunctionName(location);
  };
}

export function getFunctionSymbols(location, maxResults) {
  return async ({ dispatch }) => {
    // Make sure the source for the symbols exist.
    await dispatch(loadSourceText(location.source, location.sourceActor));
    const editor = getEditor();
    return editor?.getFunctionSymbols(maxResults);
  };
}

export function getClassSymbols(location) {
  return async ({ dispatch }) => {
    // See  comment in getFunctionSymbols
    await dispatch(loadSourceText(location.source, location.sourceActor));

    const editor = getEditor();
    return editor?.getClassSymbols();
  };
}
