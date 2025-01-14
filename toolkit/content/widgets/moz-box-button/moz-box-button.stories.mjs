/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html } from "../vendor/lit.all.mjs";
import "./moz-box-button.mjs";

export default {
  title: "UI Widgets/Box Button",
  component: "moz-box-button",
  parameters: {
    status: "in-development",
    fluent: `
moz-box-button-label =
  .label = Click me to navigate!
    `,
  },
};

const Template = ({ l10nId }) => html`
  <div style="width: 300px">
    <moz-box-button data-l10n-id=${l10nId}></moz-box-button>
  </div>
`;

export const Default = Template.bind({});
Default.args = {
  l10nId: "moz-box-button-label",
};
