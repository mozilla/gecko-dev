/**
 * @fileoverview A collection of rules that help enforce JavaScript coding
 * standard and avoid common errors in the Mozilla project.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";

const path = require("path");
const globals = require("globals");

const { name, version } = require(path.join(__dirname, "..", "package.json"));

const plugin = {
  meta: { name, version },
  configs: {
    // Filled in below.
  },
  environments: {
    "browser-window": require("./environments/browser-window.js"),
    "chrome-script": require("./environments/chrome-script.js"),
    "frame-script": require("./environments/frame-script.js"),
    sysmjs: require("./environments/sysmjs.js"),
    privileged: require("./environments/privileged.js"),
    "process-script": require("./environments/process-script.js"),
    "remote-page": require("./environments/remote-page.js"),
    simpletest: require("./environments/simpletest.js"),
    sjs: require("./environments/sjs.js"),
    "special-powers-sandbox": require("./environments/special-powers-sandbox.js"),
    specific: require("./environments/specific"),
    testharness: require("./environments/testharness.js"),
    xpcshell: require("./environments/xpcshell.js"),
  },
  rules: {
    "avoid-Date-timing": require("./rules/avoid-Date-timing"),
    "avoid-removeChild": require("./rules/avoid-removeChild"),
    "balanced-listeners": require("./rules/balanced-listeners"),
    "balanced-observers": require("./rules/balanced-observers"),
    "import-browser-window-globals": require("./rules/import-browser-window-globals"),
    "import-content-task-globals": require("./rules/import-content-task-globals"),
    "import-globals": require("./rules/import-globals"),
    "import-headjs-globals": require("./rules/import-headjs-globals"),
    "lazy-getter-object-name": require("./rules/lazy-getter-object-name"),
    "mark-exported-symbols-as-used": require("./rules/mark-exported-symbols-as-used"),
    "mark-test-function-used": require("./rules/mark-test-function-used"),
    "no-aArgs": require("./rules/no-aArgs"),
    "no-addtask-setup": require("./rules/no-addtask-setup"),
    "no-arbitrary-setTimeout": require("./rules/no-arbitrary-setTimeout"),
    "no-browser-refs-in-toolkit": require("./rules/no-browser-refs-in-toolkit"),
    "no-compare-against-boolean-literals": require("./rules/no-compare-against-boolean-literals"),
    "no-comparison-or-assignment-inside-ok": require("./rules/no-comparison-or-assignment-inside-ok"),
    "no-cu-reportError": require("./rules/no-cu-reportError"),
    "no-define-cc-etc": require("./rules/no-define-cc-etc"),
    "no-more-globals": require("./rules/no-more-globals"),
    "no-redeclare-with-import-autofix": require("./rules/no-redeclare-with-import-autofix"),
    "no-throw-cr-literal": require("./rules/no-throw-cr-literal"),
    "no-useless-parameters": require("./rules/no-useless-parameters"),
    "no-useless-removeEventListener": require("./rules/no-useless-removeEventListener"),
    "no-useless-run-test": require("./rules/no-useless-run-test"),
    "prefer-boolean-length-check": require("./rules/prefer-boolean-length-check"),
    "prefer-formatValues": require("./rules/prefer-formatValues"),
    "reject-addtask-only": require("./rules/reject-addtask-only"),
    "reject-chromeutils-import": require("./rules/reject-chromeutils-import"),
    "reject-chromeutils-import-params": require("./rules/reject-chromeutils-import-params"),
    "reject-eager-module-in-lazy-getter": require("./rules/reject-eager-module-in-lazy-getter"),
    "reject-global-this": require("./rules/reject-global-this"),
    "reject-globalThis-modification": require("./rules/reject-globalThis-modification"),
    "reject-import-system-module-from-non-system": require("./rules/reject-import-system-module-from-non-system"),
    "reject-importGlobalProperties": require("./rules/reject-importGlobalProperties"),
    "reject-lazy-imports-into-globals": require("./rules/reject-lazy-imports-into-globals"),
    "reject-mixing-eager-and-lazy": require("./rules/reject-mixing-eager-and-lazy"),
    "reject-multiple-await": require("./rules/reject-multiple-await.js"),
    "reject-multiple-getters-calls": require("./rules/reject-multiple-getters-calls"),
    "reject-scriptableunicodeconverter": require("./rules/reject-scriptableunicodeconverter"),
    "reject-relative-requires": require("./rules/reject-relative-requires"),
    "reject-some-requires": require("./rules/reject-some-requires"),
    "reject-top-level-await": require("./rules/reject-top-level-await"),
    "rejects-requires-await": require("./rules/rejects-requires-await"),
    "use-cc-etc": require("./rules/use-cc-etc"),
    "use-chromeutils-definelazygetter": require("./rules/use-chromeutils-definelazygetter"),
    "use-chromeutils-generateqi": require("./rules/use-chromeutils-generateqi"),
    "use-chromeutils-import": require("./rules/use-chromeutils-import"),
    "use-console-createInstance": require("./rules/use-console-createInstance"),
    "use-default-preference-values": require("./rules/use-default-preference-values"),
    "use-ownerGlobal": require("./rules/use-ownerGlobal"),
    "use-includes-instead-of-indexOf": require("./rules/use-includes-instead-of-indexOf"),
    "use-isInstance": require("./rules/use-isInstance"),
    "use-returnValue": require("./rules/use-returnValue"),
    "use-services": require("./rules/use-services"),
    "use-static-import": require("./rules/use-static-import"),
    "valid-ci-uses": require("./rules/valid-ci-uses"),
    "valid-lazy": require("./rules/valid-lazy"),
    "valid-services": require("./rules/valid-services"),
    "valid-services-property": require("./rules/valid-services-property"),
    "var-only-at-top-level": require("./rules/var-only-at-top-level"),
  },
};

const configurations = [
  "browser-test",
  "chrome-test",
  "general-test",
  "mochitest-test",
  "recommended",
  "require-jsdoc",
  "valid-jsdoc",
  "xpcshell-test",
];

/**
 * Clones a legacy configuration section, adjusting fields so that ESLint won't
 * fail.
 *
 * @param {object} section
 *   The section to clone.
 * @returns {object}
 *   The cloned section.
 */
function cloneLegacySection(section) {
  let config = structuredClone(section);

  if (config.overrides) {
    for (let overridesSection of config.overrides) {
      // The legacy config doesn't support names in sections, so get rid of those.
      delete overridesSection.name;
      // Also, the legacy config supports "excludedFiles" rather than "ignores".
      if (overridesSection.ignores) {
        overridesSection.excludedFiles = overridesSection.ignores;
        delete overridesSection.ignores;
      }
    }
  }

  return config;
}

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
    "no-unsanitized": require("eslint-plugin-no-unsanitized"),
    "@microsoft/sdl": require("@microsoft/eslint-plugin-sdl"),
  };
  if (!config.languageOptions) {
    config.languageOptions = {};
  }

  // Handle changing the location of the sourceType.
  if (config.parserOptions?.sourceType) {
    config.languageOptions.sourceType = config.parserOptions.sourceType;
    delete config.parserOptions;
  }

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

for (let configName of configurations) {
  let config = require(`./configs/${configName}`);

  if (configName == "recommended") {
    plugin.configs[configName] = cloneLegacySection(config.getConfig("legacy"));
    plugin.configs[`flat/${configName}`] = config
      .getConfig("flat")
      .map(section => cloneFlatSection(section));
    continue;
  }

  plugin.configs[configName] = cloneLegacySection(config);
  plugin.configs[`flat/${configName}`] = cloneFlatSection(config);
}

module.exports = plugin;
