/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { makeBreakpointId } from "../utils/breakpoint/index";

export function getInScopeLines(state, location) {
  return state.ast.mutableInScopeLines[makeBreakpointId(location)]?.lines;
}

export function hasInScopeLines(state, location) {
  return !!getInScopeLines(state, location);
}
