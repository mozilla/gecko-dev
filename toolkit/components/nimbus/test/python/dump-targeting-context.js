/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  PREFS,
  TargetingContextRecorder,
  ATTRIBUTE_TRANSFORMS,
  normalizePrefName,
} = ChromeUtils.importESModule("resource://nimbus/lib/TargetingContextRecorder.sys.mjs");

const { PREF_STRING, PREF_INT, PREF_BOOL } = Ci.nsIPrefBranch;

const DUMP_TARGETING_CONTEXT_PATH = "DUMP_TARGETING_CONTEXT_PATH";

function toGleanMetricsYamlName(attr) {
  switch(attr) {
    case "isFxAEnabled":
      // Would transform to `is_fx_aenabled`.
      return "is_fx_a_enabled";

    case "defaultPDFHandler":
      // Would transform to `default_pdfhandler`.
      return "default_pdf_handler";

    default:
      return attr.replaceAll(/[A-Z]+/g, substr => {
        return `_${substr.toLowerCase()}`;
      });
  }
}


function prefTypeToObjectMetricType(pref, type) {
  switch (type) {
    case PREF_STRING:
      return "string";

    case PREF_INT:
      return "number";

    case PREF_BOOL:
      return "boolean";

    default:
      throw new Error(`Unexpected type ${type} for pref ${pref}`);
  }
}

/**
 * Dump information about the Nimbus targeting context and its environment to a
 * JSON file.
 *
 * This script is used by test_targeting_context_metrics.py to ensure that the
 * metrics in the nimbus_targeting_context and nimbus_targeting_environment
 * categories in toolkits/components/nimbus/metrics.yaml stay up to date with
 * the Nimbus targeting context.
 */
async function main() {
  const dumpPath = Services.env.get(DUMP_TARGETING_CONTEXT_PATH);
  if (!dumpPath) {
    throw new Error(`environment variable ${DUMP_TARGETING_CONTEXT_PATH} is undefined`);
  }

  const prefs = Object
    .entries(PREFS)
    .map(([pref, type]) => ({
      pref_name: pref,
      field_name: normalizePrefName(pref),
      type: prefTypeToObjectMetricType(pref, type)
    }));

  const attrs = Object.keys(ATTRIBUTE_TRANSFORMS).map(attrName => ({
    attr_name: attrName,
    metric_name: toGleanMetricsYamlName(attrName),
  }));

  await IOUtils.writeJSON(dumpPath, { prefs, attrs });
}

let exit = false;
let exitStatus = 0;

main().then(
  () => { exit = true; },
  (exc) => {
    console.error(`Unexpected error: ${exc}`);
    dump(`${exc.stack}\n`);
    exit = true;
    exitStatus = 1;
  }
);

Services.tm.spinEventLoopUntil("dump-targeting-context.js: waiting for completion", () => exit);
quit(exitStatus);
