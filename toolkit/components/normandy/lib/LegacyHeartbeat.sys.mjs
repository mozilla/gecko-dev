/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  NimbusFeatures: "resource://nimbus/ExperimentAPI.sys.mjs",
});

/**
 * A bridge between Nimbus and Normandy's Heartbeat implementation.
 */
export const LegacyHeartbeat = {
  getHeartbeatRecipe() {
    const survey = lazy.NimbusFeatures.legacyHeartbeat.getVariable("survey");

    if (typeof survey == "undefined") {
      return null;
    }

    const { slug, isRollout } =
      lazy.NimbusFeatures.legacyHeartbeat.getEnrollmentMetadata();
    return {
      id: `nimbus:${slug}`,
      name: `Nimbus legacyHeartbeat ${isRollout ? "rollout" : "experiment"} ${slug}`,
      action: "show-heartbeat",
      arguments: survey,
      capabilities: ["action.show-heartbeat"],
      filter_expression: "true",
      use_only_baseline_capabilities: true,
      revision_id: "1", // Required for the Heartbeat telemetry ping.
    };
  },
};
