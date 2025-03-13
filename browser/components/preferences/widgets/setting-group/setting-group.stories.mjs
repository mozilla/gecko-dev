/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html } from "chrome://global/content/vendor/lit.all.mjs";
import "chrome://browser/content/preferences/widgets/setting-group.mjs";

export default {
  title: "Domain-specific UI Widgets/Settings/Setting Group",
  component: "setting-group",
  parameters: {
    status: "in-development",
    handles: ["click", "input", "change"],
    fluent: `
group-example-label =
  .label = Group Label
  .description = Could have a description as well.
checkbox-example-input =
  .label = Checkbox example of setting-control
  .description = Could have a description like moz-checkbox.
checkbox-example-input2 =
  .label = Another checkbox
`,
  },
};

function getSetting() {
  return {
    value: true,
    on() {},
    off() {},
    userChange() {},
    visible: () => true,
  };
}

const Template = ({ config }) => html`
  <setting-group .config=${config} .getSetting=${getSetting}></setting-group>
`;

export const Group = Template.bind({});
Group.args = {
  config: {
    id: "group-example",
    l10nId: "group-example-label",
    items: [
      {
        id: "checkbox-example",
        l10nId: "checkbox-example-input",
      },
      {
        id: "checkbox-example2",
        l10nId: "checkbox-example-input2",
        supportPage: "example-support",
        iconSrc: "chrome://global/skin/icons/highlights.svg",
      },
    ],
  },
};
