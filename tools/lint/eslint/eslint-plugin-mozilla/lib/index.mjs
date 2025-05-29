/**
 * @fileoverview A collection of rules that help enforce JavaScript coding
 * standard and avoid common errors in the Mozilla project.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import globals from "globals";
import helpers from "./helpers.js";
import packageData from "../package.json" with { type: "json" };

import noUnsanitizedPlugin from "eslint-plugin-no-unsanitized";
import sdlPlugin from "@microsoft/eslint-plugin-sdl";
import promisePlugin from "eslint-plugin-promise";
import jsdocPlugin from "eslint-plugin-jsdoc";

let { allFileExtensions, turnOff } = helpers;

let plugin = {
  meta: { name: packageData.name, version: packageData.version },
  configs: {
    // Filled in below due to circular references.
  },
  environments: {
    "browser-window": (await import("./environments/browser-window.js"))
      .default,
    "chrome-script": (await import("./environments/chrome-script.js")).default,
    "frame-script": (await import("./environments/frame-script.js")).default,
    sysmjs: (await import("./environments/sysmjs.js")).default,
    privileged: (await import("./environments/privileged.js")).default,
    "process-script": (await import("./environments/process-script.js"))
      .default,
    "remote-page": (await import("./environments/remote-page.js")).default,
    simpletest: (await import("./environments/simpletest.js")).default,
    sjs: (await import("./environments/sjs.js")).default,
    "special-powers-sandbox": (
      await import("./environments/special-powers-sandbox.js")
    ).default,
    specific: (await import("./environments/specific.js")).default,
    testharness: (await import("./environments/testharness.js")).default,
    xpcshell: (await import("./environments/xpcshell.js")).default,
  },
  rules: {
    "avoid-Date-timing": (await import("./rules/avoid-Date-timing.mjs"))
      .default,
    "avoid-removeChild": (await import("./rules/avoid-removeChild.mjs"))
      .default,
    "balanced-listeners": (await import("./rules/balanced-listeners.mjs"))
      .default,
    "balanced-observers": (await import("./rules/balanced-observers.mjs"))
      .default,
    "import-browser-window-globals": (
      await import("./rules/import-browser-window-globals.mjs")
    ).default,
    "import-content-task-globals": (
      await import("./rules/import-content-task-globals.mjs")
    ).default,
    "import-globals": (await import("./rules/import-globals.mjs")).default,
    "import-headjs-globals": (await import("./rules/import-headjs-globals.mjs"))
      .default,
    "lazy-getter-object-name": (
      await import("./rules/lazy-getter-object-name.mjs")
    ).default,
    "mark-test-function-used": (
      await import("./rules/mark-test-function-used.mjs")
    ).default,
    "no-aArgs": (await import("./rules/no-aArgs.mjs")).default,
    "no-addtask-setup": (await import("./rules/no-addtask-setup.mjs")).default,
    "no-arbitrary-setTimeout": (
      await import("./rules/no-arbitrary-setTimeout.mjs")
    ).default,
    "no-browser-refs-in-toolkit": (
      await import("./rules/no-browser-refs-in-toolkit.mjs")
    ).default,
    "no-compare-against-boolean-literals": (
      await import("./rules/no-compare-against-boolean-literals.mjs")
    ).default,
    "no-comparison-or-assignment-inside-ok": (
      await import("./rules/no-comparison-or-assignment-inside-ok.mjs")
    ).default,
    "no-cu-reportError": (await import("./rules/no-cu-reportError.mjs"))
      .default,
    "no-define-cc-etc": (await import("./rules/no-define-cc-etc.mjs")).default,
    "no-more-globals": (await import("./rules/no-more-globals.mjs")).default,
    "no-redeclare-with-import-autofix": (
      await import("./rules/no-redeclare-with-import-autofix.mjs")
    ).default,
    "no-throw-cr-literal": (await import("./rules/no-throw-cr-literal.mjs"))
      .default,
    "no-useless-parameters": (await import("./rules/no-useless-parameters.mjs"))
      .default,
    "no-useless-removeEventListener": (
      await import("./rules/no-useless-removeEventListener.mjs")
    ).default,
    "no-useless-run-test": (await import("./rules/no-useless-run-test.mjs"))
      .default,
    "prefer-boolean-length-check": (
      await import("./rules/prefer-boolean-length-check.mjs")
    ).default,
    "prefer-formatValues": (await import("./rules/prefer-formatValues.mjs"))
      .default,
    "reject-addtask-only": (await import("./rules/reject-addtask-only.mjs"))
      .default,
    "reject-eager-module-in-lazy-getter": (
      await import("./rules/reject-eager-module-in-lazy-getter.mjs")
    ).default,
    "reject-globalThis-modification": (
      await import("./rules/reject-globalThis-modification.mjs")
    ).default,
    "reject-import-system-module-from-non-system": (
      await import("./rules/reject-import-system-module-from-non-system.mjs")
    ).default,
    "reject-importGlobalProperties": (
      await import("./rules/reject-importGlobalProperties.mjs")
    ).default,
    "reject-lazy-imports-into-globals": (
      await import("./rules/reject-lazy-imports-into-globals.mjs")
    ).default,
    "reject-mixing-eager-and-lazy": (
      await import("./rules/reject-mixing-eager-and-lazy.mjs")
    ).default,
    "reject-multiple-await": (await import("./rules/reject-multiple-await.mjs"))
      .default,
    "reject-multiple-getters-calls": (
      await import("./rules/reject-multiple-getters-calls.mjs")
    ).default,
    "reject-scriptableunicodeconverter": (
      await import("./rules/reject-scriptableunicodeconverter.mjs")
    ).default,
    "reject-relative-requires": (
      await import("./rules/reject-relative-requires.mjs")
    ).default,
    "reject-some-requires": (await import("./rules/reject-some-requires.mjs"))
      .default,
    "reject-top-level-await": (
      await import("./rules/reject-top-level-await.mjs")
    ).default,
    "rejects-requires-await": (
      await import("./rules/rejects-requires-await.mjs")
    ).default,
    "use-cc-etc": (await import("./rules/use-cc-etc.mjs")).default,
    "use-chromeutils-generateqi": (
      await import("./rules/use-chromeutils-generateqi.mjs")
    ).default,
    "use-console-createInstance": (
      await import("./rules/use-console-createInstance.mjs")
    ).default,
    "use-default-preference-values": (
      await import("./rules/use-default-preference-values.mjs")
    ).default,
    "use-ownerGlobal": (await import("./rules/use-ownerGlobal.mjs")).default,
    "use-includes-instead-of-indexOf": (
      await import("./rules/use-includes-instead-of-indexOf.mjs")
    ).default,
    "use-isInstance": (await import("./rules/use-isInstance.mjs")).default,
    "use-returnValue": (await import("./rules/use-returnValue.mjs")).default,
    "use-services": (await import("./rules/use-services.mjs")).default,
    "use-static-import": (await import("./rules/use-static-import.mjs"))
      .default,
    "valid-ci-uses": (await import("./rules/valid-ci-uses.mjs")).default,
    "valid-lazy": (await import("./rules/valid-lazy.mjs")).default,
    "valid-services": (await import("./rules/valid-services.mjs")).default,
    "valid-services-property": (
      await import("./rules/valid-services-property.mjs")
    ).default,
    "var-only-at-top-level": (await import("./rules/var-only-at-top-level.mjs"))
      .default,
  },
  allFileExtensions,
  turnOff,
};

/**
 * Clones a flat configuration section, adjusting fields so that ESLint won't
 * fail.
 *
 * @param {object} section
 *   The section to clone.
 * @returns {object}
 *   The cloned section.
 */
function cloneFlatSection(section) {
  let config = structuredClone(section);

  // We assume all parts of the flat config need the plugins defined. In
  // practice, they only need to be defined where they are used, but for
  // now this is simpler.
  config.plugins = {
    mozilla: plugin,
    "no-unsanitized": noUnsanitizedPlugin,
    "@microsoft/sdl": sdlPlugin,
    promise: promisePlugin,
    jsdoc: jsdocPlugin,
  };
  if (!config.languageOptions) {
    config.languageOptions = {};
  }

  if (config.globals) {
    config.languageOptions.globals = { ...config.globals };
    delete config.globals;
  }

  // Handle changing the location of the sourceType.
  if (config.parserOptions?.sourceType) {
    config.languageOptions.sourceType = config.parserOptions.sourceType;
  }
  if (config.parserOptions?.ecmaFeatures) {
    config.languageOptions.parserOptions = {
      ecmaFeatures: config.parserOptions.ecmaFeatures,
    };
  }
  delete config.parserOptions;

  // Convert any environments into a list of globals.
  for (let [key, value] of Object.entries(config.env ?? {})) {
    if (!value) {
      throw new Error(
        "Removing environments is not supported by eslint-plugin-mozilla"
      );
    }
    if (!config.languageOptions.globals) {
      config.languageOptions.globals = {};
    }
    if (key.startsWith("mozilla/")) {
      config.languageOptions.globals = {
        ...config.languageOptions.globals,
        ...plugin.environments[key.substring("mozilla/".length)].globals,
      };
    } else {
      config.languageOptions.globals = {
        ...config.languageOptions.globals,
        ...globals[key],
      };
    }
  }
  delete config.env;

  return config;
}

plugin.configs = {
  "flat/browser-test": cloneFlatSection(
    (await import("./configs/browser-test.mjs")).default
  ),
  "flat/chrome-test": cloneFlatSection(
    (await import("./configs/chrome-test.mjs")).default
  ),
  "flat/general-test": cloneFlatSection(
    (await import("./configs/general-test.mjs")).default
  ),
  "flat/mochitest-test": cloneFlatSection(
    (await import("./configs/mochitest-test.mjs")).default
  ),
  "flat/recommended": (await import("./configs/recommended.mjs")).default.map(
    section => cloneFlatSection(section)
  ),
  "flat/require-jsdoc": cloneFlatSection(
    (await import("./configs/require-jsdoc.mjs")).default
  ),
  "flat/valid-jsdoc": cloneFlatSection(
    (await import("./configs/valid-jsdoc.mjs")).default
  ),
  "flat/xpcshell-test": cloneFlatSection(
    (await import("./configs/xpcshell-test.mjs")).default
  ),
};

export default plugin;
