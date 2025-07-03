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
select-example-input =
  .label = Select example of setting-control
  .description = Could have a description like moz-select.
select-option-0 =
  .label = Option 0
select-option-1 =
  .label = Option 1
select-option-2 =
  .label = Option 2
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
    getControlConfig: c => c,
  },
};

export const Select = Template.bind({});
Select.args = {
  config: {
    id: "select-example",
    l10nId: "select-example-input",
    control: "moz-select",
    supportPage: "example-support",
    options: [
      {
        value: "0",
        l10nId: "select-option-0",
      },
      {
        value: "1",
        l10nId: "select-option-1",
      },
      {
        value: "2",
        l10nId: "select-option-2",
      },
    ],
  },
  setting: {
    value: "1",
    on() {},
    off() {},
    userChange() {},
    getControlConfig: c => c,
  },
};
