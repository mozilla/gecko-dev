/**
 * @file Defines the environment for xpcshell test files.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import { getScriptGlobals } from "./utils.mjs";

/**
 * @type {{[key: string]: "readonly"|"writeable"|"off"}}
 */
const extraGlobals = {
  // Defined in XPCShellImpl.cpp
  print: "readonly",
  readline: "readonly",
  load: "readonly",
  quit: "readonly",
  dumpXPC: "readonly",
  dump: "readonly",
  gc: "readonly",
  gczeal: "readonly",
  options: "readonly",
  sendCommand: "readonly",
  atob: "readonly",
  btoa: "readonly",
  setInterruptCallback: "readonly",
  simulateNoScriptActivity: "readonly",
  registerXPCTestComponents: "readonly",

  // Assert.sys.mjs globals.
  setReporter: "readonly",
  report: "readonly",
  ok: "readonly",
  equal: "readonly",
  notEqual: "readonly",
  deepEqual: "readonly",
  notDeepEqual: "readonly",
  strictEqual: "readonly",
  notStrictEqual: "readonly",
  throws: "readonly",
  rejects: "readonly",
  greater: "readonly",
  greaterOrEqual: "readonly",
  less: "readonly",
  lessOrEqual: "readonly",
  // TestingFunctions.cpp globals
  allocationMarker: "readonly",
  byteSize: "readonly",
  saveStack: "readonly",
};

export default getScriptGlobals({
  environmentName: "xpcshell",
  files: ["testing/xpcshell/head.js"],
  extraGlobals,
});
