/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import type { ActorId, ExecutionPoint } from "../../types";
import type { ThunkArgs } from "../types";

import { getSourceByActorId, getCurrentThread, getSelectedFrame } from "../../selectors";
import { zip } from "lodash";

export function setFramePositions() {
  return async ({ dispatch, getState, sourceMaps, client }: ThunkArgs) => {
    const thread = getCurrentThread(getState());
    const frame = getSelectedFrame(getState(), thread);
    if (!frame) {
      return;
    }

    const { positions, unexecuted } = await client.fetchAncestorFramePositions(frame.index);

    if (frame != getSelectedFrame(getState(), thread)) {
      return;
    }

    const sourceId = getSourceByActorId(getState(), positions[0].location.actor).id;

    const executionPoints = positions.map(({ point }) => point);
    const generatedLocations = positions.map(({ location }) => {
      const { line, column } = location;
      return { line, column, sourceId };
    });
    const originalLocations = await sourceMaps.getOriginalLocations(
      generatedLocations
    );

    const combinedPositions = zip(executionPoints, originalLocations, generatedLocations).map(
      ([point, location, generatedLocation]) => ({ point, location, generatedLocation })
    );

    const generatedUnexecuted = unexecuted.map(({ line, column }) => {
      return { line, column, sourceId };
    });
    const originalUnexecuted = await sourceMaps.getOriginalLocations(
      generatedUnexecuted
    );

    const combinedUnexecuted = zip(originalUnexecuted, generatedUnexecuted).map(
      ([location, generatedLocation]) => ({ location, generatedLocation })
    );

    if (frame != getSelectedFrame(getState(), thread)) {
      return;
    }

    dispatch({
      type: "SET_FRAME_POSITIONS",
      positions: combinedPositions,
      unexecuted: combinedUnexecuted,
    });
  };
}
