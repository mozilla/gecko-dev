/**
 * @file Provides utilities for setting up environments.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import fs from "fs";
import path from "path";
import helpers from "../helpers.mjs";
import globals from "../globals.mjs";

/**
 * @typedef {{[name: string]: "readonly"|"writeable"|"off"}} Globals
 *   A list of globals compatible with ESLint's configuration format. The type
 *   name "Globals" reflects the type in the ESLint definition.
 */

let savedGlobals = null;

/**
 * Loads environment items from the saved globals file. Used when eslint-plugin-mozilla
 * is installed outside of the firefox repository.
 *
 * @param {string} environment
 * @returns {{globals: {Globals}}}
 *   The globals for the given environment.
 */
function getSavedEnvironmentItems(environment) {
  if (!savedGlobals) {
    savedGlobals = JSON.parse(
      fs.readFileSync(
        path.join(import.meta.dirname, "environments", "saved-globals.json"),
        {
          encoding: "utf-8",
        }
      )
    );
  }
  return savedGlobals.environments[environment];
}

/**
 * Obtains the globals for a list of files and if they are writeable or not.
 *
 * @param {string} environmentName
 *   The name of the environment that globals are being obtained for.
 * @param {string[]} files
 *   The array of files to get globals for. The paths are relative to the topsrcdir.
 * @param {Globals} [extraGlobals]
 *   Any additional globals to add to the globals list.
 */
function getGlobalsForScripts(environmentName, files, extraGlobals) {
  /** @type {ReturnType<typeof globals.getGlobalsForFile>} */
  let fileGlobals = [];
  const root = helpers.rootDir;
  for (const file of files) {
    const fileName = path.join(root, file);
    try {
      fileGlobals = fileGlobals.concat(globals.getGlobalsForFile(fileName));
    } catch (e) {
      console.error(`Could not load globals from file ${fileName}: ${e}`);
      console.error(
        `You may need to update the mappings for the ${environmentName} environment`
      );
      throw new Error(`Could not load globals from file ${fileName}: ${e}`);
    }
  }

  /** @type {Globals}} */
  var globalObjects = { ...extraGlobals };
  for (let { name: globalName, writable } of fileGlobals) {
    globalObjects[globalName] = writable ? "writeable" : "readonly";
  }
  return globalObjects;
}

/**
 * Gets the complete globals list for a set of scripts, adding extra globals and
 * environments as required.
 *
 * When run from within the Firefox repository, this will use process the given
 * files. When run from outside the Firefox repository, this will use the
 * cache of saved globals.
 *
 * @param {object} options
 * @param {string} options.environmentName
 *   The name of the environment we are getting the globals for.
 * @param {string[]} options.files
 *   The array of files to process
 * @param {Globals} [options.extraGlobals]
 *   Any additional globals to add to the globals list.
 */
export function getScriptGlobals({
  environmentName,
  files,
  extraGlobals = {},
}) {
  if (helpers.isMozillaCentralBased()) {
    return {
      globals: getGlobalsForScripts(environmentName, files, extraGlobals),
    };
  }
  return getSavedEnvironmentItems(environmentName);
}
