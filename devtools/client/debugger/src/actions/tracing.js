/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * Called when tracing is toggled ON/OFF on a particular thread.
 */
export function tracingToggled(thread, enabled) {
  return ({ dispatch }) => {
    dispatch({
      type: "TRACING_TOGGLED",
      thread,
      enabled,
    });
  };
}
