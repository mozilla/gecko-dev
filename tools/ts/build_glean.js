/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * Build:   <srcdir>/tools/@types/glean/<path>.d.ts,
 * update:  <srcdir>/tools/@types/lib.gecko.glean.d.ts,
 *
 * from:    <srcdir>/<path>/metrics.yaml.
 */

const fs = require("fs");
const YAML = require("yaml");

const HEADER = `/**
 * NOTE: Do not modify this file by hand.
 * Content was generated from source glean .yaml files.
 * If you're updating some of the sources, see README for instructions.
 */
`;

const TYPES = {
  boolean: "GleanBoolean",
  labeled_boolean: "Record<string, GleanBoolean>",
  counter: "GleanCounter",
  labeled_counter: "Record<string, GleanCounter>",
  string: "GleanString",
  labeled_string: "Record<string, GleanString>",
  string_list: "GleanStringList",
  timespan: "GleanTimespan",
  timing_distribution: "GleanTimingDistribution",
  labeled_timing_distribution: "Record<string, GleanTimingDistribution>",
  memory_distribution: "GleanMemoryDistribution",
  labeled_memory_distribution: "Record<string, GleanMemoryDistribution>",
  uuid: "GleanUuid",
  url: "GleanUrl",
  datetime: "GleanDatetime",
  event: "GleanEvent",
  custom_distribution: "GleanCustomDistribution",
  labeled_custom_distribution: "Record<string, GleanCustomDistribution>",
  quantity: "GleanQuantity",
  labeled_quantity: "Record<string, GleanQuantity>",
  rate: "GleanRate",
  text: "GleanText",
  object: "GleanObject",
};

const SCHEMA = "moz://mozilla.org/schemas/glean";

function style(str, capital) {
  return capital ? str.charAt(0).toUpperCase() + str.slice(1) : str;
}

function camelCase(id) {
  return id.split(/[_.-]/).map(style).join("");
}

// Produce webidl types based on Glean metrics.
function emitGlean(yamlDoc) {
  let lines = [HEADER];
  lines.push("interface GleanImpl {");

  for (let [cat, metrics] of Object.entries(yamlDoc)) {
    if (!cat.startsWith("$")) {
      lines.push(`\n  ${camelCase(cat)}: {`);
      for (let [name, metric] of Object.entries(metrics)) {
        let type = TYPES[metric.type];
        if (!type) {
          throw new Error(`Unknown glean type ${metric.type}`);
        }
        lines.push(`    ${camelCase(name)}: ${type};`);
      }
      lines.push("  }");
    }
  }
  lines.push("}\n");
  return lines.join("\n");
}

// Produce webidl types based on Glean pings.
function emitGleanPings(yamlDoc) {
  let lines = [];
  lines.push("\ninterface GleanPingsImpl {");

  for (let [name, ping] of Object.entries(yamlDoc)) {
    if (!name.startsWith("$")) {
      if (!ping.reasons) {
        lines.push(`  ${camelCase(name)}: nsIGleanPingNoReason;`);
      } else {
        let rtype = Object.keys(ping.reasons)
          .map(r => `"${r}"`)
          .join("|");
        lines.push(`  ${camelCase(name)}: nsIGleanPingWithReason<${rtype}>;`);
      }
    }
  }
  lines.push("}\n");
  return lines.join("\n");
}

// Build lib.gecko.glean.d.ts.
async function main(src_dir, ...paths) {
  let lib = `${src_dir}/tools/@types/lib.gecko.glean.d.ts`;

  let metrics = {};
  let pings = {};

  for (let path of paths) {
    console.log(`[INFO] ${path}`);
    let yaml = fs.readFileSync(`${src_dir}/${path}`, "utf8");
    let parsed = YAML.parse(yaml, { merge: true, schema: "failsafe" });

    if (parsed.$schema === `${SCHEMA}/metrics/2-0-0`) {
      for (let [key, val] of Object.entries(parsed)) {
        if (typeof val === "object") {
          Object.assign((metrics[key] ??= {}), val);
        }
      }
    } else if (parsed.$schema === `${SCHEMA}/pings/2-0-0`) {
      Object.assign(pings, parsed);
    } else {
      throw new Error(`Unknown Glean schema: ${parsed.$schema}`);
    }
  }

  let all = emitGlean(metrics) + emitGleanPings(pings);
  console.log(`[WARN] ${lib} (${all.length.toLocaleString()} bytes)`);
  fs.writeFileSync(lib, all);
}

if (require.main === module) {
  main(...process.argv.slice(2));
}
