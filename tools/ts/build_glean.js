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
 * Content was generated from source metrics.yaml files.
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
  memory_distribution: "GleanMemoryDistribution",
  uuid: "GleanUuid",
  url: "GleanUrl",
  datetime: "GleanDatetime",
  event: "GleanEvent",
  custom_distribution: "GleanCustomDistribution",
  quantity: "GleanQuantity",
  labeled_quantity: "Record<string, GleanQuantity>",
  rate: "GleanRate",
  text: "GleanText",
  object: "GleanObject",
};

function style(str, capital) {
  return capital ? str.charAt(0).toUpperCase() + str.slice(1) : str;
}

function camelCase(id) {
  return id.split(/_|\./).map(style).join("");
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

// Build target .d.ts and update the Glean lib.
async function main(src_dir, path, types_dir) {
  if (!path.endsWith(".yaml")) {
    path += "/metrics.yaml";
  }

  let glean = `${src_dir}/${types_dir}/glean/`;
  let lib = `${src_dir}/${types_dir}/lib.gecko.glean.d.ts`;
  let dir = path.replaceAll("/", "_").replace(/_*(metrics.yaml)?$/, "");

  let target = `${glean}/${dir}.d.ts`;
  let yaml = fs.readFileSync(`${src_dir}/${path}`, "utf8");
  let dts = emitGlean(YAML.parse(yaml, { merge: true, schema: "failsafe" }));

  console.log(`[INFO] ${target} (${dts.length.toLocaleString()} bytes)`);
  fs.writeFileSync(target, dts);

  let files = fs.readdirSync(glean).sort();
  let refs = files.map(f => `/// <reference types="./glean/${f}" />`);
  let all = `${HEADER}\n${refs.join("\n")}\n`;

  console.log(`[INFO] ${lib} (${all.length.toLocaleString()} bytes)`);
  fs.writeFileSync(lib, all);
}

if (require.main === module) {
  main(...process.argv.slice(2));
}
