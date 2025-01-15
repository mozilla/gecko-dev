/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// eslint-disable-next-line import/no-unresolved
import { html } from "lit.all.mjs";
import "./password-card.mjs";

export default {
  title: "Domain-specific UI Widgets/Credential Management/Password Card",
  component: "PasswordCard",
  argTypes: {
    website: { control: { type: "object" } },
    username: { control: { type: "object" } },
    password: { control: { type: "object" } },
  },
};

window.MozXULElement.insertFTLIfNeeded("preview/megalist.ftl");

const Template = ({ website, username, password }) => {
  return html`
    <password-card
      role="group"
      .origin=${website}
      .username=${username}
      .password=${password}
      .messageToViewModel=${() => {}}
      .reauthCommandHandler=${() => true}
    >
    </password-card>
  `;
};

export const Default = Template.bind({});
Default.args = {
  website: {
    value: "website.com",
    breached: false,
    valueIcon: "chrome://global/skin/icons/defaultFavicon.svg",
  },
  username: {
    value: "username",
  },
  password: {
    value: "password",
    vulnerable: false,
    concealed: true,
  },
};

export const AllAlertsOn = Template.bind({});
AllAlertsOn.args = {
  website: {
    value: "website.com",
    breached: true,
    valueIcon: "chrome://global/skin/icons/defaultFavicon.svg",
  },
  username: {
    value: "",
  },
  password: {
    value: "password",
    vulnerable: true,
    concealed: true,
  },
};
