/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html } from "../vendor/lit.all.mjs";
import "./moz-input-color.mjs";

export default {
  title: "UI Widgets/Input Color",
  component: "moz-input-color",
  argTypes: {},
  parameters: {
    status: "stable",
  },
};

const Template = ({ value, label }) => {
  return html`
    <moz-input-color value=${value} label=${label}></moz-input-color>
  `;
};

export const Default = Template.bind({});
Default.args = {
  value: "#7293C9",
  label: "Background",
};
