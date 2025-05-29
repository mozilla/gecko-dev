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
moz-option-1 =
    .label = Option 1
moz-option-2 =
    .label = Option 2
moz-option-3 =
    .label = Option 3
moz-option-a =
    .label = Option A
moz-option-b =
    .label = Option B
moz-option-c =
    .label = Option C
moz-option-d =
    .label = Option D
    `,
  },
};

const DEFAULT_OPTIONS = [
  { value: "1", l10nId: "moz-option-1" },
  { value: "2", l10nId: "moz-option-2" },
  { value: "3", l10nId: "moz-option-3" },
];
const OTHER_OPTIONS = [
  { value: "A", l10nId: "moz-option-a" },
  { value: "B", l10nId: "moz-option-b" },
  { value: "C", l10nId: "moz-option-c" },
  { value: "D", l10nId: "moz-option-d" },
];
const WITH_ICON_DEFAULT_OPTIONS = [
  {
    value: "1",
    l10nId: "moz-option-1",
    iconSrc: "chrome://global/skin/icons/settings.svg",
  },
  {
    value: "2",
    l10nId: "moz-option-2",
    iconSrc: "chrome://global/skin/icons/info.svg",
  },
  {
    value: "3",
    l10nId: "moz-option-3",
    iconSrc: "chrome://global/skin/icons/warning.svg",
  },
];
const WITH_ICON_OTHER_OPTIONS = [
  {
    value: "A",
    l10nId: "moz-option-a",
    iconSrc: "chrome://global/skin/icons/heart.svg",
  },
  {
    value: "B",
    l10nId: "moz-option-b",
    iconSrc: "chrome://global/skin/icons/edit.svg",
  },
  {
    value: "C",
    l10nId: "moz-option-c",
    iconSrc: "chrome://global/skin/icons/delete.svg",
  },
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
  hasSlottedSupportLink,
}) => html`
  <div style="width:300px">
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
      ${hasSlottedSupportLink
        ? html`<a slot="support-link" href="www.example.com">Click me!</a>`
        : ""}
      ${options.map(
        opt =>
          html`<moz-option
            value=${opt.value}
            data-l10n-id=${opt.l10nId}
            iconsrc=${opt.iconSrc}
          ></moz-option>`
      )}
    </moz-select>
  </div>
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
  hasSlottedSupportLink: false,
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

export const WithSelectedOptionIcon = args => {
  const options = args.useOtherOptions
    ? WITH_ICON_OTHER_OPTIONS
    : WITH_ICON_DEFAULT_OPTIONS;

  return Template({ ...args, options });
};

WithSelectedOptionIcon.args = {
  ...Default.args,
  useOtherOptions: false,
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

export const WithSupportLink = Template.bind({});
WithSupportLink.args = {
  ...Default.args,
  supportPage: "support-page",
  l10nId: "moz-select-description",
};

export const WithSlottedSupportLink = Template.bind({});
WithSlottedSupportLink.args = {
  ...Default.args,
  hasSlottedSupportLink: true,
  l10nId: "moz-select-description",
};

export const PreselectedValue = Template.bind({});
PreselectedValue.args = {
  ...Default.args,
  value: "2",
};
