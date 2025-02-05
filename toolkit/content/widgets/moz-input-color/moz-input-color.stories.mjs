/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html } from "../vendor/lit.all.mjs";
import "./moz-input-color.mjs";

export default {
  title: "UI Widgets/Color Input",
  component: "moz-input-color",
  argTypes: {},
  parameters: {
    status: "stable",
    fluent: `moz-input-color-label = Background`,
  },
};

const Template = ({ color, propName, labelL10nId }) => html`
  <moz-input-color
    color=${color}
    data-l10n-id=${labelL10nId}
    prop-name=${propName}
  ></moz-input-color>
`;

export const Default = Template.bind({});
Default.args = {
  propName: "background",
  color: "#7293C9",
  labelL10nId: "moz-input-color-label",
};
