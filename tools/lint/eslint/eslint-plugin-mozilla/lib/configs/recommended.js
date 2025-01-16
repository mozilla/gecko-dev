/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const js = require("@eslint/js");

/**
 * The configuration is based on eslint:recommended config. It defines the
 * recommended rules for all files, as well as for rules relating to modules
 * and other module like files.
 *
 * The configuration intentionally does not specify the globals for the
 * majority of files. The globals will only be specified for Mozilla specific
 * files (e.g. system modules). The subscriber to this configuration is expect
 * to include the correct globals that they require in their project.
 *
 * The details for all the ESLint rules, and which ones are in the ESLint
 * recommended configuration can be found here:
 *
 * https://eslint.org/docs/rules/
 *
 * Rules that we've explicitly decided not to enable:
 *
 *   require-await - bug 1381030.
 *   no-prototype-builtins - bug 1551829.
 *   require-atomic-updates - bug 1551829.
 *     - This generates too many false positives that are not easy to work
 *       around, and false positives seem to be inherent in the rule.
 *   no-inner-declarations - bug 1487642.
 *     - Would be interested if this could apply to just vars, but at the moment
 *       it doesn't.
 *   max-depth
 *      - Don't enforce the maximum depth that blocks can be nested. The
 *        complexity rule is a better rule to check this.
 *   no-useless-escape - bug 1881262.
 *     - This doesn't reveal any actual errors, and is a lot of work to address.
 */

const coreRules = {
  // When adding items to this file please check for effects on all of toolkit
  // and browser
  rules: {
    // This may conflict with prettier, so we turn it off.
    "arrow-body-style": "off",

    // Warn about cyclomatic complexity in functions.
    // XXX Get this down to 20?
    complexity: ["error", 34],

    // Functions must always return something or nothing
    "consistent-return": "error",

    // Encourage the use of dot notation whenever possible.
    "dot-notation": "error",

    // Maximum depth callbacks can be nested.
    "max-nested-callbacks": ["error", 10],

    "mozilla/avoid-removeChild": "error",
    "mozilla/import-browser-window-globals": "error",
    "mozilla/import-globals": "error",
    "mozilla/no-compare-against-boolean-literals": "error",
    "mozilla/no-cu-reportError": "error",
    "mozilla/no-define-cc-etc": "error",
    "mozilla/no-throw-cr-literal": "error",
    "mozilla/no-useless-parameters": "error",
    "mozilla/no-useless-removeEventListener": "error",
    "mozilla/prefer-boolean-length-check": "error",
    "mozilla/prefer-formatValues": "error",
    "mozilla/reject-addtask-only": "error",
    "mozilla/reject-chromeutils-import": "error",
    "mozilla/reject-chromeutils-import-params": "error",
    "mozilla/reject-importGlobalProperties": ["error", "allownonwebidl"],
    "mozilla/reject-multiple-await": "error",
    "mozilla/reject-multiple-getters-calls": "error",
    "mozilla/reject-scriptableunicodeconverter": "warn",
    "mozilla/rejects-requires-await": "error",
    "mozilla/use-cc-etc": "error",
    "mozilla/use-chromeutils-definelazygetter": "error",
    "mozilla/use-chromeutils-generateqi": "error",
    "mozilla/use-chromeutils-import": "error",
    "mozilla/use-console-createInstance": "error",
    "mozilla/use-default-preference-values": "error",
    "mozilla/use-includes-instead-of-indexOf": "error",
    "mozilla/use-isInstance": "error",
    "mozilla/use-ownerGlobal": "error",
    "mozilla/use-returnValue": "error",
    "mozilla/use-services": "error",
    "mozilla/valid-lazy": "error",
    "mozilla/valid-services": "error",

    // Use [] instead of Array()
    "no-array-constructor": "error",

    // Disallow use of arguments.caller or arguments.callee.
    "no-caller": "error",

    // Disallow the use of console, except for errors and warnings.
    "no-console": ["error", { allow: ["createInstance", "error", "warn"] }],

    // Disallows expressions where the operation doesn't affect the value.
    // TODO: This is enabled by default in ESLint's v9 recommended configuration.
    "no-constant-binary-expression": "error",

    // If an if block ends with a return no need for an else block
    "no-else-return": "error",

    // No empty statements
    "no-empty": ["error", { allowEmptyCatch: true }],

    // Disallow empty static blocks.
    // This rule will be a recommended rule in ESLint v9 so may be removed
    // when we upgrade to that.
    "no-empty-static-block": "error",

    // Disallow eval and setInteral/setTimeout with strings
    "no-eval": "error",

    // Disallow unnecessary calls to .bind()
    "no-extra-bind": "error",

    // Disallow fallthrough of case statements
    "no-fallthrough": [
      "error",
      {
        // The eslint rule doesn't allow for case-insensitive regex option.
        // The following pattern allows for a dash between "fall through" as
        // well as alternate spelling of "fall thru". The pattern also allows
        // for an optional "s" at the end of "fall" ("falls through").
        commentPattern:
          "[Ff][Aa][Ll][Ll][Ss]?[\\s-]?([Tt][Hh][Rr][Oo][Uu][Gg][Hh]|[Tt][Hh][Rr][Uu])",
      },
    ],

    // Disallow eval and setInteral/setTimeout with strings
    "no-implied-eval": "error",

    // See explicit decisions at top of file.
    "no-inner-declarations": "off",

    // Disallow the use of the __iterator__ property
    "no-iterator": "error",

    // No labels
    "no-labels": "error",

    // Disallow unnecessary nested blocks
    "no-lone-blocks": "error",

    // No single if block inside an else block
    "no-lonely-if": "error",

    // Nested ternary statements are confusing
    "no-nested-ternary": "error",

    // Disallow new operators with global non-constructor functions.
    // This rule will be a recommended rule in ESLint v9 so may be removed
    // when we upgrade to that.
    "no-new-native-nonconstructor": "error",

    // Disallow use of new wrappers
    "no-new-wrappers": "error",

    // Use {} instead of new Object(), unless arguments are passed.
    "no-object-constructor": "error",

    // We don't want this, see bug 1551829
    "no-prototype-builtins": "off",

    // Disable builtinGlobals for no-redeclare as this conflicts with our
    // globals declarations especially for browser window.
    "no-redeclare": ["error", { builtinGlobals: false }],

    // Disallow use of event global.
    "no-restricted-globals": ["error", "event"],

    // No unnecessary comparisons
    "no-self-compare": "error",

    // No comma sequenced statements
    "no-sequences": "error",

    // No declaring variables from an outer scope
    "no-shadow": "error",

    // Disallow throwing literals (eg. throw "error" instead of
    // throw new Error("error")).
    "no-throw-literal": "error",

    // Disallow the use of Boolean literals in conditional expressions.
    "no-unneeded-ternary": "error",

    // No unsanitized use of innerHTML=, document.write() etc.
    // cf. https://github.com/mozilla/eslint-plugin-no-unsanitized#rule-details
    "no-unsanitized/method": "error",
    "no-unsanitized/property": "error",

    // Disallow unused private class members.
    // This rule will be a recommended rule in ESLint v9 so may be removed
    // when we upgrade to that.
    "no-unused-private-class-members": "error",

    // No declaring variables that are never used
    "no-unused-vars": [
      "error",
      {
        argsIgnorePattern: "^_",
        vars: "local",
      },
    ],

    // No using variables before defined
    // "no-use-before-define": ["error", "nofunc"],

    // Disallow unnecessary .call() and .apply()
    "no-useless-call": "error",

    // Don't concatenate string literals together (unless they span multiple
    // lines)
    "no-useless-concat": "error",

    // See explicit decisions at top of file.
    "no-useless-escape": "off",

    // Disallow redundant return statements
    "no-useless-return": "error",

    // Require object-literal shorthand with ES6 method syntax
    "object-shorthand": ["error", "always", { avoidQuotes: true }],

    // This may conflict with prettier, so turn it off.
    "prefer-arrow-callback": "off",

    // Not passing anything to .catch/.then doesn't work, error:
    "promise/valid-params": "error",
  },
};

const extraRules = [
  {
    // System mjs files files are not loaded in the browser scope,
    // so we turn that off for those. Though we do have our own special
    // environment for them.
    env: {
      "mozilla/privileged": true,
      "mozilla/specific": true,
      "mozilla/sysmjs": true,
    },
    files: ["**/*.sys.mjs"],
    name: "mozilla/recommended/system-modules",
    rules: {
      "mozilla/lazy-getter-object-name": "error",
      "mozilla/reject-eager-module-in-lazy-getter": "error",
      "mozilla/reject-global-this": "error",
      "mozilla/reject-globalThis-modification": "error",
      // For all system modules, we expect no properties to need importing,
      // hence reject everything.
      "mozilla/reject-importGlobalProperties": ["error", "everything"],
      "mozilla/reject-mixing-eager-and-lazy": "error",
      "mozilla/reject-top-level-await": "error",
    },
  },
  {
    files: ["**/*.mjs", "**/*.jsx", "**/?(*.)worker.?(m)js"],
    name: "mozilla/recommended/file-scoped-globals-rules",
    rules: {
      // We enable builtinGlobals for modules and workers due to their
      // contained scopes.
      "no-redeclare": ["error", { builtinGlobals: true }],
      "no-shadow": ["error", { allow: ["event"], builtinGlobals: true }],
      // Modules and workers are far easier to check for no-unused-vars on a
      // global scope, than our content files. Hence we turn that on here.
      "no-unused-vars": [
        "error",
        {
          argsIgnorePattern: "^_",
          vars: "all",
        },
      ],
    },
  },
  {
    files: ["**/*.mjs"],
    ignores: ["**/*.sys.mjs"],
    name: "mozilla/recommended/modules-not-system-modules",
    rules: {
      "mozilla/reject-import-system-module-from-non-system": "error",
      "mozilla/reject-lazy-imports-into-globals": "error",
    },
  },
  {
    files: ["**/*.mjs", "**/*.jsx"],
    name: "mozilla/recommended/module-only",
    parserOptions: {
      sourceType: "module",
    },
    rules: {
      "mozilla/use-static-import": "error",
      // This rule defaults to not allowing "use strict" in module files since
      // they are always loaded in strict mode.
      strict: "error",
    },
  },
  {
    env: {
      "mozilla/sjs": true,
    },
    files: ["**/*.sjs"],
    name: "mozilla/recommended/sjs",
    rules: {
      // For sjs files, reject everything as we should update the sandbox
      // to include the globals we need, as these are test-only files.
      "mozilla/reject-importGlobalProperties": ["error", "everything"],
    },
  },
  {
    env: {
      worker: true,
    },
    files: [
      // Most files should use the `.worker.` format to be consistent with
      // other items like `.sys.mjs`, but we allow simply calling the file
      // "worker" as well.
      "**/?(*.)worker.?(m)js",
    ],
  },
];

const legacyConfig = {
  extends: ["eslint:recommended"],

  overrides: structuredClone(extraRules),

  parserOptions: {
    // If this changes, ensure the version in `flatConfig` is updated, as well
    // as the return value of `helpers.getECMAVersion()`.
    ecmaVersion: "latest",
  },

  // When adding items to this file please check for effects on sub-directories.
  plugins: ["no-unsanitized"],

  rules: coreRules.rules,
};

// Note: plugins are added in the top-level index.js file. This is to avoid
// needing to import them multiple times for different configs.
const flatConfig = [
  {
    languageOptions: {
      // If this changes, ensure the version in `legacyConfig` is updated, as well
      // as the return value of `helpers.getECMAVersion()`.
      ecmaVersion: "latest",
    },
    name: "mozilla/recommended/main-rules",
    rules: {
      ...js.configs.recommended.rules,
      ...coreRules.rules,
    },
  },
  ...structuredClone(extraRules),
];

module.exports = {
  getConfig(configType) {
    if (configType == "flat") {
      return flatConfig;
    }
    return legacyConfig;
  },
};
