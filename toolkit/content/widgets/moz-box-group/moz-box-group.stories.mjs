/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html, ifDefined } from "../vendor/lit.all.mjs";
import "./moz-box-group.mjs";

export default {
  title: "UI Widgets/Box Group",
  component: "moz-box-group",
  parameters: {
    status: "in-development",
    fluent: `
moz-box-item =
  .label = I'm a box item
  .description = I'm part of a group
moz-box-button-1 =
  .label = I'm a box button in a group
moz-box-button-2 =
  .label = I'm another box button in a group
moz-box-link =
  .label = I'm a box link in a group
    `,
  },
};

const Template = ({ type }) => html`
  <style>
    .delete {
      margin-top: var(--space-medium);
    }
  </style>
  <moz-box-group type=${ifDefined(type)}>
    <moz-box-item data-l10n-id="moz-box-item">
      <moz-toggle slot="actions"></moz-toggle>
    </moz-box-item>
    <moz-box-link data-l10n-id="moz-box-link"></moz-box-link>
    <moz-box-button data-l10n-id="moz-box-button-1"></moz-box-button>
    <moz-box-button data-l10n-id="moz-box-button-2"></moz-box-button>
  </moz-box-group>
  ${type == "list"
    ? html`<moz-button class="delete" @click=${appendItem}>
        Add an item
      </moz-button>`
    : ""}
`;

const appendItem = event => {
  let group = event.target.getRootNode().querySelector("moz-box-group");

  let boxItem = document.createElement("moz-box-item");
  boxItem.label = "New box item";
  boxItem.description = "New items are added to the list";

  let actionButton = document.createElement("moz-button");
  actionButton.addEventListener("click", () => boxItem.remove());
  actionButton.iconSrc = "chrome://global/skin/icons/delete.svg";
  actionButton.slot = "actions";
  boxItem.append(actionButton);

  group.prepend(boxItem);
};

export const Default = Template.bind({});
Default.args = {};

export const List = Template.bind({});
List.args = {
  type: "list",
};
