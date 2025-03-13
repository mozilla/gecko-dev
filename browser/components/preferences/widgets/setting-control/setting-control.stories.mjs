/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html } from "chrome://global/content/vendor/lit.all.mjs";
import "chrome://browser/content/preferences/widgets/setting-control.mjs";

export default {
  title: "Domain-specific UI Widgets/Settings/Setting Control",
  component: "setting-control",
  parameters: {
    status: "in-development",
    handles: ["click", "input", "change"],
    fluent: `
checkbox-example-input =
  .label = Checkbox example of setting-control
  .description = Could have a description like moz-checkbox.
`,
  },
};

const Template = ({ config, setting }) => html`
  <setting-control .config=${config} .setting=${setting}></setting-control>
`;

export const Checkbox = Template.bind({});
Checkbox.args = {
  config: {
    id: "checkbox-example",
    l10nId: "checkbox-example-input",
  },
  setting: {
    value: true,
    on() {},
    off() {},
    userChange() {},
  },
};
