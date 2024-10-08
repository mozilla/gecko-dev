/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export const TelemetryEvents = {
  sendEvent(method, object, value, extra) {
    for (const val of Object.values(extra)) {
      if (val == null) {
        throw new Error(
          "Extra parameters in telemetry events must not be null"
        );
      }
    }
    let words = (method + "_" + object).split("_");
    let name =
      words[0] +
      words
        .slice(1)
        .map(word => word[0].toUpperCase() + word.slice(1))
        .join("");
    if (value !== undefined) {
      extra = Object.assign({}, extra);
      extra.value = value;
    }
    Glean.normandy[name]?.record(extra);
  },
};
