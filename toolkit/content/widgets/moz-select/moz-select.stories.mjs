/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html, ifDefined } from "../vendor/lit.all.mjs";
import "./moz-select.mjs";

export default {
  title: "UI Widgets/Select",
  component: "moz-select",
  argTypes: {
    l10nId: {
      options: ["moz-select-label", "moz-select-description"],
      control: { type: "select" },
    },
    label: { table: { disable: true } },
    description: { table: { disable: true } },
  },
  parameters: {
    status: "in-development",
    handles: ["change", "input"],
    fluent: `
moz-select-label =
  .label = Select an option
moz-select-description =
  .label = Select an option
  .description = This is a description for the select dropdown
    `,
  },
};

const DEFAULT_OPTIONS = [
  { value: "1", label: "Option 1" },
  { value: "2", label: "Option 2" },
  { value: "3", label: "Option 3" },
];
const OTHER_OPTIONS = [
  { value: "A", label: "Option A" },
  { value: "B", label: "Option B" },
  { value: "C", label: "Option C" },
  { value: "D", label: "Option D" },
];

const Template = ({
  name,
  value,
  iconSrc,
  disabled,
  l10nId,
  description,
  supportPage,
  accessKey,
  hasSlottedDescription,
  useOtherOptions,
  options = useOtherOptions ? OTHER_OPTIONS : DEFAULT_OPTIONS,
}) => html`
  <moz-select
    name=${name}
    value=${ifDefined(value || null)}
    iconsrc=${ifDefined(iconSrc || null)}
    ?disabled=${disabled}
    data-l10n-id=${l10nId}
    support-page=${ifDefined(supportPage || null)}
    accesskey=${ifDefined(accessKey || null)}
  >
    ${hasSlottedDescription
      ? html`<div slot="description">${description}</div>`
      : ""}
    ${options.map(
      opt =>
        html`<moz-option value=${opt.value} label=${opt.label}></moz-option>`
    )}
  </moz-select>
`;

export const Default = Template.bind({});
Default.args = {
  name: "example-moz-select",
  value: "",
  iconSrc: "",
  disabled: false,
  l10nId: "moz-select-label",
  description: "",
  supportPage: "",
  accessKey: "",
  hasSlottedDescription: false,
  useOtherOptions: false,
};

export const WithIcon = Template.bind({});
WithIcon.args = {
  ...Default.args,
  iconSrc: "chrome://global/skin/icons/highlights.svg",
};

export const WithDescription = Template.bind({});
WithDescription.args = {
  ...Default.args,
  l10nId: "moz-select-description",
};

export const WithSlottedDescription = Template.bind({});
WithSlottedDescription.args = {
  ...Default.args,
  description: "This is a custom slotted description.",
  hasSlottedDescription: true,
};

export const Disabled = Template.bind({});
Disabled.args = {
  ...Default.args,
  disabled: true,
};

export const WithAccesskey = Template.bind({});
WithAccesskey.args = {
  ...Default.args,
  accessKey: "s",
};

export const WithSupportPage = Template.bind({});
WithSupportPage.args = {
  ...Default.args,
  supportPage: "support-page",
  l10nId: "moz-select-description",
};

export const PreselectedValue = Template.bind({});
PreselectedValue.args = {
  ...Default.args,
  value: "2",
};
