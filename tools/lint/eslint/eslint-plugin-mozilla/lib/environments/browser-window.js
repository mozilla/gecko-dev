/**
 * @fileoverview Defines the environment when in the browser.xul window.
 *               Imports many globals from various files.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";

// -----------------------------------------------------------------------------
// Rule Definition
// -----------------------------------------------------------------------------

var fs = require("fs");
var path = require("path");
var helpers = require("../helpers");
var globals = require("../globals");

const rootDir = helpers.rootDir;

// When updating EXTRA_SCRIPTS or MAPPINGS, be sure to also update the
// 'support-files' config in `tools/lint/eslint.yml`.

// These are scripts not loaded from browser.xul or global-scripts.inc
// but via other includes.
const EXTRA_SCRIPTS = [
  "browser/base/content/nsContextMenu.js",
  "browser/components/places/content/editBookmark.js",
  "browser/components/downloads/content/downloads.js",
  "browser/components/downloads/content/indicator.js",
  "toolkit/content/customElements.js",
  "toolkit/content/editMenuOverlay.js",
];

const extraDefinitions = [
  // Via Components.utils, defineModuleGetter, defineLazyModuleGetters or
  // defineLazyScriptGetter (and map to
  // single) variable.
  {name: "XPCOMUtils", writable: false},
  {name: "Task", writable: false},
];

// Some files in global-scripts.inc need mapping to specific locations.
const MAPPINGS = {
  "printUtils.js": "toolkit/components/printing/content/printUtils.js",
  "panelUI.js": "browser/components/customizableui/content/panelUI.js",
  "viewSourceUtils.js":
    "toolkit/components/viewsource/content/viewSourceUtils.js",
};

const globalScriptsRegExp =
  /^\s*Services.scriptloader.loadSubScript\(\"(.*?)\", this\);$/;

function getGlobalScriptIncludes(scriptPath) {
  let fileData;
  try {
    fileData = fs.readFileSync(scriptPath, {encoding: "utf8"});
  } catch (ex) {
    // The file isn't present, so this isn't an m-c repository.
    return null;
  }

  fileData = fileData.split("\n");

  let result = [];

  for (let line of fileData) {
    let match = line.match(globalScriptsRegExp);
    if (match) {
      let sourceFile = match[1]
                .replace("chrome://browser/content/search/", "browser/components/search/content/")
                .replace("chrome://browser/content/", "browser/base/content/")
                .replace("chrome://global/content/", "toolkit/content/");

      for (let mapping of Object.getOwnPropertyNames(MAPPINGS)) {
        if (sourceFile.includes(mapping)) {
          sourceFile = MAPPINGS[mapping];
        }
      }

      result.push(sourceFile);
    }
  }

  return result;
}

function getGlobalScripts() {
  let results = [];
  for (let scriptPath of helpers.globalScriptPaths) {
    results = results.concat(getGlobalScriptIncludes(scriptPath));
  }
  return results;
}

function getScriptGlobals() {
  let fileGlobals = [];
  let scripts = getGlobalScripts();
  if (!scripts) {
    return [];
  }

  for (let script of scripts.concat(EXTRA_SCRIPTS)) {
    let fileName = path.join(rootDir, script);
    try {
      fileGlobals = fileGlobals.concat(globals.getGlobalsForFile(fileName));
    } catch (e) {
      console.error(`Could not load globals from file ${fileName}: ${e}`);
      console.error(
        `You may need to update the mappings in ${module.filename}`);
      throw new Error(`Could not load globals from file ${fileName}: ${e}`);
    }
  }

  return fileGlobals.concat(extraDefinitions);
}

function mapGlobals(fileGlobals) {
  let globalObjects = {};
  for (let global of fileGlobals) {
    globalObjects[global.name] = global.writable;
  }
  return globalObjects;
}

function getMozillaCentralItems() {
  return {
    globals: mapGlobals(getScriptGlobals()),
    browserjsScripts: getGlobalScripts().concat(EXTRA_SCRIPTS),
  };
}

module.exports = helpers.isMozillaCentralBased() ?
 getMozillaCentralItems() :
 helpers.getSavedEnvironmentItems("browser-window");
