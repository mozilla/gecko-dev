/**
 * @file A script to export the known globals to a file.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

// This is a non-production script.
/* eslint-disable no-console */

import fsPromises from "fs/promises";
import path from "path";
import helpers from "../lib/helpers.mjs";
import plugin from "../lib/index.mjs";

const eslintDir = path.join(helpers.rootDir, "tools", "lint", "eslint");

const globalsFile = path.join(
  eslintDir,
  "eslint-plugin-mozilla",
  "lib",
  "environments",
  "saved-globals.json"
);

console.log("Copying services.json");

const env = helpers.getBuildEnvironment();

const servicesFile = path.join(
  env.topobjdir,
  "xpcom",
  "components",
  "services.json"
);
const shipServicesFile = path.join(
  eslintDir,
  "eslint-plugin-mozilla",
  "lib",
  "services.json"
);

await fsPromises.copyFile(servicesFile, shipServicesFile);

console.log("Generating globals file");

// Export the environments.
await fsPromises.writeFile(
  globalsFile,
  JSON.stringify({ environments: plugin.environments })
);

console.log("Globals file generation complete");
