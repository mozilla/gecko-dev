/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import {
  getGeneratedFrameScope,
  getOriginalFrameScope,
  getSelectedFrame,
} from "../../selectors/index";
import { mapScopes } from "./mapScopes";
import { generateInlinePreview } from "./inlinePreview";
import { PROMISE } from "../utils/middleware/promise";
import { validateSelectedFrame } from "../../utils/context";

/**
 * Retrieve the scopes and map them for the currently selected frame.
 * Once this is done, update the inline previews.
 */
export function fetchScopes() {
  return async function ({ dispatch, getState, client }) {
    const selectedFrame = getSelectedFrame(getState());
    // See if we already fetched the scopes.
    // We may have pause on multiple thread and re-select a paused thread
    // for which we already fetched the scopes.
    // Ignore pending scopes as the previous action may have been cancelled
    // by context assertions.
    let scopes = getGeneratedFrameScope(getState(), selectedFrame);
    if (!scopes?.scope) {
      scopes = dispatch({
        type: "ADD_SCOPES",
        selectedFrame,
        [PROMISE]: client.getFrameScopes(selectedFrame),
      });

      scopes.then(() => {
        // Avoid generating previews, if we resumed or switched to another frame while retrieving scopes
        validateSelectedFrame(getState(), selectedFrame);

        dispatch(generateInlinePreview());
      });
    }

    if (!getOriginalFrameScope(getState(), selectedFrame)) {
      await dispatch(mapScopes(selectedFrame, scopes));
    }
  };
}
