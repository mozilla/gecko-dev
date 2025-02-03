/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Note: the schema can be found in
 * https://searchfox.org/mozilla-central/source/toolkit/components/telemetry/Events.yaml
 */
const EXTRAS_FIELD_NAMES = [
  "addon_version",
  "session_id",
  "page",
  "user_prefs",
  "action_position",
];

export class UTEventReporting {
  constructor() {
    this.sendUserEvent = this.sendUserEvent.bind(this);
    this.sendSessionEndEvent = this.sendSessionEndEvent.bind(this);
  }

  _createExtras(data, value) {
    // Make a copy of the given data and delete/modify it as needed.
    let utExtras = Object.assign({}, data);
    for (let field of Object.keys(utExtras)) {
      if (!EXTRAS_FIELD_NAMES.includes(field)) {
        delete utExtras[field];
      }
    }
    utExtras.value = value;
    return utExtras;
  }

  sendUserEvent(data) {
    const eventName = data.event
      .split("_")
      .map(word => word[0] + word.slice(1).toLowerCase())
      .join("");
    Glean.activityStream[`event${eventName}`].record(
      this._createExtras(data, data.source)
    );
  }

  sendSessionEndEvent(data) {
    Glean.activityStream.endSession.record(
      this._createExtras(data, data.session_duration)
    );
  }
}
