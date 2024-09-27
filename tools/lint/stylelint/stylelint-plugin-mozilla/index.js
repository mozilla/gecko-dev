/**
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/* eslint-env node */

const { createPlugin } = require("stylelint");
const rules = require("./rules");
const { namespace } = require("./helpers");

const plugins = Object.keys(rules).map(ruleName => {
  return createPlugin(namespace(ruleName), rules[ruleName]);
});

module.exports = plugins;
