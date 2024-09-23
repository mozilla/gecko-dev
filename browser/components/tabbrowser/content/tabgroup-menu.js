/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// This is loaded into chrome windows with the subscript loader. Wrap in
// a block to prevent accidentally leaking globals onto `window`.
{
  class MozTabbrowserTabGroupMenu extends MozXULElement {
    static COLORS = [
      "blue",
      "purple",
      "cyan",
      "orange",
      "yellow",
      "pink",
      "green",
      "gray",
      "red",
    ];
    static markup = `
    <panel
        type="arrow"
        class="panel tab-group-editor-panel"
        orient="vertical"
        role="menu"
        noautohide="true"
        norolluponanchor="true">
      <html:div class="panel-header">
        <html:h1 data-l10n-id="tab-group-editor-title-create"></html:h1>
      </html:div>
      <toolbarseparator />
      <html:div class="panel-body tab-group-editor-name">
        <html:label for="tab-group-name" data-l10n-id="tab-group-editor-name-label"></html:label>
        <html:input id="tab-group-name" type="text" name="tab-group-name" value="" data-l10n-id="tab-group-editor-name-field" />
      </html:div>
      <html:div class="panel-body tab-group-editor-swatches">
      </html:div>
      <html:moz-button-group class="panel-body tab-group-editor-actions">
        <html:moz-button id="tab-group-editor-button-cancel" data-l10n-id="tab-group-editor-cancel"></html:moz-button>
        <html:moz-button type="primary" id="tab-group-editor-button-create" data-l10n-id="tab-group-editor-create"></html:moz-button>
      </html:moz-button-group>
    </panel>
       `;

    #activeGroup;
    #cancelButton;
    #createButton;
    #nameField;
    #panel;
    #swatches;
    #swatchesContainer;

    constructor() {
      super();
    }

    connectedCallback() {
      if (this._initialized) {
        return;
      }

      this.textContent = "";
      this.appendChild(this.constructor.fragment);
      this.initializeAttributeInheritance();

      this._initialized = true;

      this.#cancelButton = this.querySelector(
        "#tab-group-editor-button-cancel"
      );
      this.#createButton = this.querySelector(
        "#tab-group-editor-button-create"
      );
      this.#panel = this.querySelector("panel");
      this.#nameField = this.querySelector("#tab-group-name");
      this.#swatchesContainer = this.querySelector(
        ".tab-group-editor-swatches"
      );
      this.#populateSwatches();

      this.#cancelButton.addEventListener("click", () => {
        this.#handleCancel();
      });

      this.#createButton.addEventListener("click", () => {
        this.#handleCreate();
      });

      this.#nameField.addEventListener("input", () => {
        if (this.activeGroup) {
          this.activeGroup.label = this.#nameField.value;
        }
      });

      this.addEventListener("change", this);
    }

    #populateSwatches() {
      for (let colorCode of MozTabbrowserTabGroupMenu.COLORS) {
        let input = document.createElement("input");
        input.id = `tab-group-editor-swatch-${colorCode}`;
        input.type = "radio";
        input.name = "tab-group-color";
        input.value = colorCode;
        input.title = colorCode;
        let label = document.createElement("label");
        label.classList.add("tab-group-editor-swatch");
        label.htmlFor = input.id;
        label.style.setProperty(
          "--tabgroup-swatch-color",
          `var(--tab-group-color-${colorCode})`
        );
        label.style.setProperty(
          "--tabgroup-swatch-color-invert",
          `var(--tab-group-color-${colorCode}-invert)`
        );
        this.#swatchesContainer.append(input); // todo appendchild?
        this.#swatchesContainer.append(label);
        this.#swatches = this.querySelectorAll('input[name="tab-group-color"]');
      }
    }

    get activeGroup() {
      return this.#activeGroup;
    }

    set activeGroup(group = null) {
      this.#activeGroup = group;
      this.#nameField.value = group ? group.label : "";
      this.#swatches.forEach(node => {
        if (group && node.value == group.color) {
          node.checked = true;
        } else {
          node.checked = false;
        }
      });
    }

    get panel() {
      return this.children[0];
    }

    openCreateModal(group) {
      this.activeGroup = group;
      this.#panel.openPopup(group, {
        position: "bottomleft topleft",
      });
    }

    /**
     * change handler for color input
     */
    on_change(aEvent) {
      if (aEvent.target.name != "tab-group-color") {
        return;
      }
      if (this.activeGroup) {
        this.activeGroup.color = aEvent.target.value;
      }
    }

    #handleCancel() {
      this.activeGroup.ungroupTabs();
      this.#panel.hidePopup();
    }

    #handleCreate() {
      this.#panel.hidePopup();
      this.activeGroup = null;
    }
  }

  customElements.define("tabgroup-menu", MozTabbrowserTabGroupMenu);
}
