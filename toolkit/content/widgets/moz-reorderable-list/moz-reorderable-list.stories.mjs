/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { LitElement, html } from "../vendor/lit.all.mjs";
// eslint-disable-next-line import/no-unassigned-import
import "./moz-reorderable-list.mjs";

export default {
  title: "UI Widgets/Reorderable List",
  component: "moz-reorderable-list",
  parameters: {
    actions: {
      handles: ["reorder"],
    },
  },
};

class ReorderableDemo extends LitElement {
  static properties = {
    items: { type: Array, state: true },
    itemSelector: { type: String },
    focusableSelector: { type: String },
  };

  // Chosing not to use Shadow DOM here for demo purposes.
  createRenderRoot() {
    return this;
  }

  constructor() {
    super();
    this.items = ["Item 1", "Item 2", "Item 3", "Item 4"];
  }

  async reorderItems(draggedElement, targetElement, before = false) {
    const draggedIndex = this.items.indexOf(draggedElement.textContent);
    const targetIndex = this.items.indexOf(targetElement.textContent);

    let nextItems = [...this.items];
    const [draggedItem] = nextItems.splice(draggedIndex, 1);

    let adjustedTargetIndex = targetIndex;
    if (draggedIndex < targetIndex) {
      adjustedTargetIndex--;
    }

    if (before) {
      nextItems.splice(adjustedTargetIndex, 0, draggedItem);
    } else {
      nextItems.splice(adjustedTargetIndex + 1, 0, draggedItem);
    }

    this.items = nextItems;
    await this.updateComplete;
    targetElement.firstElementChild.focus();
  }

  handleReorder(e) {
    const { draggedElement, targetElement, position } = e.detail;
    this.reorderItems(draggedElement, targetElement, position === -1);
  }

  handleKeydown(e) {
    const result = this.children[1].evaluateKeyDownEvent(e);
    if (!result) {
      return;
    }
    const { draggedElement, targetElement } = result;
    this.reorderItems(draggedElement, targetElement);
  }

  addItem() {
    this.items = [...this.items, `Item ${this.items.length + 1}`];
  }

  render() {
    return html`
      <style>
        ul {
          padding: 0;
        }
        li {
          list-style: none;
          display: flex;
        }
        button {
          display: block;
          padding: 10px;
          background-color: #eee;
        }
      </style>
      <moz-reorderable-list
        itemselector=${this.itemSelector}
        focusableselector=${this.focusableSelector}
        @reorder=${this.handleReorder}
      >
        <ul>
          ${this.items.map(
            item => html`
              <li><button @keydown=${this.handleKeydown}>${item}</button></li>
            `
          )}
        </ul>
      </moz-reorderable-list>
      <button @click=${this.addItem}>Add another item</button>
    `;
  }
}
customElements.define("reorderable-demo", ReorderableDemo);

const Template = ({ itemSelector, focusableSelector }) => html`
  <style>
    ul {
      padding: 0;
    }
    li {
      list-style: none;
      display: flex;
    }
    button {
      display: block;
      padding: 10px;
      background-color: #eee;
    }
  </style>
  <reorderable-demo
    .itemSelector=${itemSelector}
    .focusableSelector=${focusableSelector}
  ></reorderable-demo>
`;

export const ReorderableList = Template.bind({});
ReorderableList.args = {
  itemSelector: "li",
  focusableSelector: "li > button",
};
