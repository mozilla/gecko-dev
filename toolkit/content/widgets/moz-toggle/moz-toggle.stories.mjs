/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html, ifDefined, classMap } from "../vendor/lit.all.mjs";
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
        "moz-toggle-long-label",
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
moz-toggle-long-label =
  .label = Lorem ipsum dolor sit amet, consectetur adipiscing elit. Vestibulum libero enim, luctus eu ante a, maximus imperdiet mi. Suspendisse sodales, nisi et commodo malesuada, lectus.
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
  ellipsized,
}) => {
  let toggleTemplate = html`
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
      class=${classMap({ "text-truncated-ellipsis": ellipsized })}
    >
      ${hasSlottedSupportLink
        ? html`<a slot="support-link" href="www.example.com">Click me!</a>`
        : ""}
      ${nestedFields
        ? html`<moz-checkbox slot="nested" data-l10n-id=${ifDefined(l10nId)}>
            </moz-checkbox>
            <moz-checkbox slot="nested" data-l10n-id=${ifDefined(l10nId)}>
              <moz-checkbox slot="nested" data-l10n-id=${ifDefined(l10nId)}>
              </moz-checkbox>
            </moz-checkbox> `
        : ""}
    </moz-toggle>
  `;
  return nestedFields
    ? html`<moz-fieldset label="Toggle with nested fields"
        >${toggleTemplate}</moz-fieldset
      >`
    : toggleTemplate;
};

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

export const WithEllipsizedLabel = Template.bind({});
WithEllipsizedLabel.args = {
  ...Default.args,
  ellipsized: true,
  l10nId: "moz-toggle-long-label",
};
