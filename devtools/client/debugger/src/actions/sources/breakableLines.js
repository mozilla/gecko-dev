/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import {
  getBreakableLines,
  getSourceActorBreakableLines,
  getSourceActorsForSource,
} from "../../selectors/index";
import { setBreakpointPositions } from "../breakpoints/breakpointPositions";

function calculateBreakableLines(positions) {
  const lines = [];
  for (const line in positions) {
    if (positions[line].length) {
      lines.push(Number(line));
    }
  }

  return lines;
}

/**
 * Ensure that breakable lines for a given source are fetched.
 *
 * This method is memoized, so that if concurrent calls are dispatched,
 * it will return the first async promise processing the breakable lines.
 *
 * @param Object location
 */
export function setBreakableLines(location) {
  return async ({ getState, dispatch, client }) => {
    if (location.source.isOriginal) {
      // Original sources have a dedicated codepath to fetch locations
      // from the generated source actor and then map them to "positions"
      // in the original source.
      let promise = getBreakableLines(getState(), location.source.id);
      if (promise) {
        return promise;
      }
      promise = (async () => {
        const positions = await dispatch(setBreakpointPositions(location));
        return calculateBreakableLines(positions);
      })();
      dispatch({
        type: "SET_ORIGINAL_BREAKABLE_LINES",
        source: location.source,
        promise,
      });
      const breakableLines = await promise;
      dispatch({
        type: "SET_ORIGINAL_BREAKABLE_LINES",
        source: location.source,
        breakableLines,
      });
    } else if (location.source.isHTML) {
      // For a given HTML page, we get a unique Source (for the html page),
      // but many Source Actors (one per inline <script> tag and inlined event listeners).
      // So that we have to get the breakable lines for each of them.
      //
      // Whereas, for regular source (with a url), when we have more than one source actor,
      // it means that we evaled the source more than once.
      const sourceActors = getSourceActorsForSource(
        getState(),
        location.source.id
      );
      if (!sourceActors) {
        return null;
      }
      for (const sourceActor of sourceActors) {
        let promise = getSourceActorBreakableLines(getState(), sourceActor.id);
        if (promise) {
          await promise;
        } else {
          promise = client.getSourceActorBreakableLines(sourceActor);
          dispatch({
            type: "SET_SOURCE_ACTOR_BREAKABLE_LINES",
            sourceActor,
            promise,
          });
          const breakableLines = await promise;
          dispatch({
            type: "SET_SOURCE_ACTOR_BREAKABLE_LINES",
            sourceActor,
            breakableLines,
          });
        }
      }
    } else {
      // Here is the handling of regular non-source-mapped non-html sources.
      // We do fetch the breakable position for the specific source actor passed as argument.
      // In case there is many source actors for the same URL, it will only affect one instance.
      let promise = getSourceActorBreakableLines(
        getState(),
        location.sourceActor.id
      );
      if (promise) {
        return promise;
      }
      promise = client.getSourceActorBreakableLines(location.sourceActor);
      dispatch({
        type: "SET_SOURCE_ACTOR_BREAKABLE_LINES",
        sourceActor: location.sourceActor,
        promise,
      });
      const breakableLines = await promise;
      dispatch({
        type: "SET_SOURCE_ACTOR_BREAKABLE_LINES",
        sourceActor: location.sourceActor,
        breakableLines,
      });
    }
    return null;
  };
}
