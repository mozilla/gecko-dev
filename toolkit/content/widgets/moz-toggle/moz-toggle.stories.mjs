/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html, ifDefined } from "../vendor/lit.all.mjs";
import "./moz-toggle.mjs";
import "../moz-support-link/moz-support-link.mjs";

export default {
  title: "UI Widgets/Toggle",
  component: "moz-toggle",
  argTypes: {
    l10nId: {
      options: [
        "moz-toggle-label",
        "moz-toggle-aria-label",
        "moz-toggle-description",
      ],
      control: { type: "select" },
    },
  },
  parameters: {
    status: "stable",
    actions: {
      handles: ["toggle"],
    },
    fluent: `
moz-toggle-aria-label =
  .aria-label = This is the aria-label
moz-toggle-label =
  .label = This is the label
moz-toggle-description =
  .label = This is the label
  .description = This is the description.
    `,
  },
};

const Template = ({
  pressed,
  disabled,
  label,
  description,
  ariaLabel,
  l10nId,
  supportPage,
  accessKey,
  iconSrc,
  hasSlottedSupportLink,
  nestedFields,
}) => html`
  <moz-toggle
    ?pressed=${pressed}
    ?disabled=${disabled}
    label=${ifDefined(label)}
    description=${ifDefined(description)}
    aria-label=${ifDefined(ariaLabel)}
    data-l10n-id=${ifDefined(l10nId)}
    accesskey=${ifDefined(accessKey)}
    support-page=${ifDefined(supportPage)}
    iconsrc=${ifDefined(iconSrc)}
  >
    ${hasSlottedSupportLink
      ? html`<a slot="support-link" href="www.example.com">Click me!</a>`
      : ""}
    ${nestedFields
      ? html`<moz-checkbox
            slot="nested"
            ?disabled=${disabled}
            data-l10n-id=${ifDefined(l10nId)}
          >
          </moz-checkbox>
          <moz-checkbox
            slot="nested"
            ?disabled=${disabled}
            data-l10n-id=${ifDefined(l10nId)}
          >
            <moz-checkbox
              slot="nested"
              ?disabled=${disabled}
              data-l10n-id=${ifDefined(l10nId)}
            >
            </moz-checkbox>
          </moz-checkbox> `
      : ""}
  </moz-toggle>
`;

export const Default = Template.bind({});
Default.args = {
  pressed: true,
  disabled: false,
  l10nId: "moz-toggle-label",
  hasSupportLink: false,
  accessKey: "",
  supportPage: "",
  iconSrc: "",
  hasSlottedSupportLink: false,
  nestedFields: false,
};

export const Disabled = Template.bind({});
Disabled.args = {
  ...Default.args,
  disabled: true,
};

export const ToggleOnly = Template.bind({});
ToggleOnly.args = {
  ...Default.args,
  l10nId: "moz-toggle-aria-label",
};

export const WithAccesskey = Template.bind({});
WithAccesskey.args = {
  ...Default.args,
  accessKey: "h",
};

export const WithDescription = Template.bind({});
WithDescription.args = {
  ...Default.args,
  l10nId: "moz-toggle-description",
};

export const WithSupportLink = Template.bind({});
WithSupportLink.args = {
  ...Default.args,
  supportPage: "addons",
};

export const WithSlottedSupportLink = Template.bind({});
WithSlottedSupportLink.args = {
  ...Default.args,
  hasSlottedSupportLink: true,
};

export const WithIcon = Template.bind({});
WithIcon.args = {
  ...Default.args,
  iconSrc: "chrome://global/skin/icons/highlights.svg",
};

export const WithNestedFields = Template.bind({});
WithNestedFields.args = {
  ...Default.args,
  nestedFields: true,
};
