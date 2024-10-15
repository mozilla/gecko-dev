/**
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/* eslint-env node */

import stylelint from "stylelint";
import rules from "./rules/index.mjs";
import { namespace } from "./helpers.mjs";

const plugins = Object.keys(rules).map(ruleName => {
  return stylelint.createPlugin(namespace(ruleName), rules[ruleName]);
});

export default plugins;
