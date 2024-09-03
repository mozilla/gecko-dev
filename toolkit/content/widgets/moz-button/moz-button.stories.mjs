/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html, ifDefined } from "../vendor/lit.all.mjs";
import "./moz-button.mjs";

export default {
  title: "UI Widgets/Button",
  component: "moz-button",
  argTypes: {
    l10nId: {
      options: [
        "moz-button-labelled",
        "moz-button-titled",
        "moz-button-aria-labelled",
      ],
      control: { type: "select" },
    },
    size: {
      options: ["default", "small"],
      control: { type: "radio" },
    },
    type: {
      options: ["default", "primary", "destructive", "icon", "icon ghost"],
      control: { type: "select" },
    },
  },
  parameters: {
    actions: {
      handles: ["click"],
    },
    status: "stable",
    fluent: `
moz-button-labelled =
  .label = Button
moz-button-primary = Primary
moz-button-destructive = Destructive
moz-button-titled =
  .title = View logins
moz-button-aria-labelled =
  .aria-label = View logins
`,
  },
};

const Template = ({
  type,
  size,
  l10nId,
  iconSrc,
  disabled,
  accesskey,
  clickHandler,
}) => html`
  <moz-button
    @click=${clickHandler}
    data-l10n-id=${l10nId}
    data-l10n-attrs="label"
    type=${type}
    size=${size}
    ?disabled=${disabled}
    iconSrc=${ifDefined(iconSrc)}
    accesskey=${ifDefined(accesskey)}
  ></moz-button>
`;

export const Default = Template.bind({});
Default.args = {
  type: "default",
  size: "default",
  l10nId: "moz-button-labelled",
  iconSrc: "",
  disabled: false,
};
export const DefaultSmall = Template.bind({});
DefaultSmall.args = {
  type: "default",
  size: "small",
  l10nId: "moz-button-labelled",
  iconSrc: "",
  disabled: false,
};
export const Primary = Template.bind({});
Primary.args = {
  ...Default.args,
  type: "primary",
  l10nId: "moz-button-primary",
};
export const Destructive = Template.bind({});
Destructive.args = {
  ...Default.args,
  type: "destructive",
  l10nId: "moz-button-destructive",
};
export const Icon = Template.bind({});
Icon.args = {
  ...Default.args,
  iconSrc: "chrome://global/skin/icons/more.svg",
  l10nId: "moz-button-titled",
};
export const IconSmall = Template.bind({});
IconSmall.args = {
  ...Icon.args,
  size: "small",
};
export const IconGhost = Template.bind({});
IconGhost.args = {
  ...Icon.args,
  type: "ghost",
};
export const IconText = Template.bind({});
IconText.args = {
  type: "default",
  size: "default",
  iconSrc: "chrome://global/skin/icons/edit-copy.svg",
  l10nId: "moz-button-labelled",
};
export const WithAccesskey = Template.bind({});
WithAccesskey.args = {
  ...Default.args,
  accesskey: "t",
  clickHandler: () => alert("Activating the accesskey clicks the button"),
};
