/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html, ifDefined, classMap } from "../vendor/lit.all.mjs";
import "./moz-visual-picker.mjs";

export default {
  title: "UI Widgets/Visual Picker",
  component: "moz-visual-picker",
  argTypes: {
    value: {
      options: ["1", "2", "3"],
      control: { type: "select" },
    },
    slottedItem: {
      options: ["card", "avatar"],
      control: { type: "select" },
    },
    pickerL10nId: {
      options: ["moz-visual-picker", "moz-visual-picker-description"],
      control: { type: "select" },
    },
  },
  parameters: {
    actions: {
      handles: ["click", "input", "change"],
    },
    status: "in-development",
    fluent: `
moz-visual-picker =
  .label = Pick something
moz-visual-picker-description =
  .label = Pick something
  .description = Pick one of these cool things please
`,
  },
};

const AVATAR_ICONS = [
  "chrome://global/skin/icons/defaultFavicon.svg",
  "chrome://global/skin/icons/experiments.svg",
  "chrome://global/skin/icons/heart.svg",
];

function getSlottedContent(type, index) {
  if (type == "card") {
    return html`<div class="slotted demo-card">
      <img
        src="chrome://browser/content/profiles/assets/system-theme-background.svg"
      />
      <span>I'm card number ${index + 1}</span>
    </div>`;
  }
  return html`<div class="slotted avatar">
    <img src=${AVATAR_ICONS[index]} role="presentation" />
  </div>`;
}

const Template = ({ value, slottedItem, pickerL10nId, supportPage }) => {
  return html`
    <style>
      .slotted {
        display: flex;
        justify-content: center;
        align-items: center;
      }

      .demo-card {
        flex-direction: column;
        width: 120px;

        span {
          padding: var(--space-xsmall);
          text-align: center;
        }

        img {
          border-top-left-radius: inherit;
          border-top-right-radius: inherit;
        }
      }

      .avatar-item {
        --visual-picker-item-border-radius: var(--border-radius-circle);
      }

      .avatar {
        height: 50px;
        width: 50px;

        img {
          height: var(--icon-size-default);
          width: var(--icon-size-default);
          -moz-context-properties: fill;
          fill: var(--icon-color);
        }
      }
    </style>
    <moz-visual-picker
      data-l10n-id=${pickerL10nId}
      value=${ifDefined(value)}
      support-page=${supportPage}
    >
      ${[...Array.from({ length: 3 })].map(
        (_, i) =>
          html`<moz-visual-picker-item
            value=${i + 1}
            class=${classMap({ "avatar-item": slottedItem == "avatar" })}
          >
            ${getSlottedContent(slottedItem, i)}
          </moz-visual-picker-item>`
      )}
    </moz-visual-picker>
  `;
};

export const Default = Template.bind({});
Default.args = {
  pickerL10nId: "moz-visual-picker",
  slottedItem: "card",
  value: "1",
  supportPage: "",
};

export const WithPickerDescription = Template.bind({});
WithPickerDescription.args = {
  ...Default.args,
  pickerL10nId: "moz-visual-picker-description",
};

export const WithPickerSupportLink = Template.bind({});
WithPickerSupportLink.args = {
  ...WithPickerDescription.args,
  supportPage: "foo",
};

export const AllUnselected = Template.bind({});
AllUnselected.args = {
  ...Default.args,
  value: "",
};
