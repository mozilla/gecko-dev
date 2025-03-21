/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// This is loaded into chrome windows with the subscript loader. Wrap in
// a block to prevent accidentally leaking globals onto `window`.
{
  const { TabStateFlusher } = ChromeUtils.importESModule(
    "resource:///modules/sessionstore/TabStateFlusher.sys.mjs"
  );

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

    static AI_ICON = "chrome://global/skin/icons/highlights.svg";

    static headerSection = /*html*/ `
      <html:div class="panel-header">
        <html:h1
          id="tab-group-editor-title-create"
          class="tab-group-create-mode-only"
          data-l10n-id="tab-group-editor-title-create">
        </html:h1>
        <html:h1
          id="tab-group-editor-title-edit"
          class="tab-group-edit-mode-only"
          data-l10n-id="tab-group-editor-title-edit">
        </html:h1>
      </html:div>
    `;

    static editActions = /*html*/ `
      <html:div
        class="panel-body tab-group-edit-actions tab-group-edit-mode-only">
        <toolbarbutton
          tabindex="0"
          id="tabGroupEditor_addNewTabInGroup"
          class="subviewbutton"
          data-l10n-id="tab-group-editor-action-new-tab">
        </toolbarbutton>
        <toolbarbutton
          tabindex="0"
          id="tabGroupEditor_moveGroupToNewWindow"
          class="subviewbutton"
          data-l10n-id="tab-group-editor-action-new-window">
        </toolbarbutton>
        <toolbarbutton
          tabindex="0"
          id="tabGroupEditor_saveAndCloseGroup"
          class="subviewbutton"
          data-l10n-id="tab-group-editor-action-save">
        </toolbarbutton>
        <toolbarbutton
          tabindex="0"
          id="tabGroupEditor_ungroupTabs"
          class="subviewbutton"
          data-l10n-id="tab-group-editor-action-ungroup">
        </toolbarbutton>
      </html:div>
        
      <toolbarseparator class="tab-group-edit-mode-only" />

      <html:div class="tab-group-edit-mode-only panel-body tab-group-delete">
        <toolbarbutton
          tabindex="0"
          id="tabGroupEditor_deleteGroup"
          class="subviewbutton"
          data-l10n-id="tab-group-editor-action-delete">
        </toolbarbutton>
      </html:div>
    `;

    static suggestionsHeader = /*html*/ `
      <html:div id="tab-group-suggestions-heading" hidden="true">
        <html:div class="panel-header">
          <html:h1 data-l10n-id="tab-group-editor-title-suggest"></html:h1>
        </html:div>
        <toolbarseparator />
      </html:div>
    `;

    static suggestionsSection = /*html*/ `
      <html:div id="tab-group-suggestions-container" hidden="true">

        <html:div
          id="tab-group-suggestions-header"
          data-l10n-id="tab-group-editor-suggestions-header">
        </html:div>

        <html:div id="tab-group-suggestions"></html:div>

        <html:moz-button
          id="tab-group-select-toggle"
          data-l10n-id="tab-group-editor-deselect-suggestions"
          size="small"
          data-state="deselect">
        </html:moz-button>

        <html:moz-button-group class="panel-body tab-group-create-actions">
          <html:moz-button
            id="tab-group-cancel-suggestions-button"
            data-l10n-id="tab-group-editor-cancel">
          </html:moz-button>
          <html:moz-button
            type="primary"
            id="tab-group-create-suggestions-button"
            data-l10n-id="tab-group-editor-done">
          </html:moz-button>
        </html:moz-button-group>

      </html:div>
    `;

    static suggestionsButton = /*html*/ `
      <html:moz-button
        hidden="true"
        id="tab-group-suggestion-button"
        type="icon ghost"
        data-l10n-id="tab-group-editor-smart-suggest-button-create">
      </html:moz-button>
    `;

    static loadingSection = /*html*/ `
      <html:div id="tab-group-suggestions-loading" hidden="true">
        <html:div
          class="tab-group-suggestions-loading-header"
          data-l10n-id="tab-group-suggestions-loading-header">
        </html:div>
        <html:div class="tab-group-suggestions-loading-block"></html:div>
        <html:div class="tab-group-suggestions-loading-block"></html:div>
        <html:div class="tab-group-suggestions-loading-block"></html:div>
      </html:div>
    `;

    static defaultActions = /*html*/ `
      <html:moz-button-group
        class="panel-body tab-group-create-actions tab-group-create-mode-only"
        id="tab-group-default-actions">
        <html:moz-button
          id="tab-group-editor-button-cancel"
          data-l10n-id="tab-group-editor-cancel">
        </html:moz-button>
        <html:moz-button
          type="primary"
          id="tab-group-editor-button-create"
          data-l10n-id="tab-group-editor-done">
        </html:moz-button>
      </html:moz-button-group>
    `;

    static loadingActions = /*html*/ `
      <html:moz-button-group id="tab-group-suggestions-load-actions" hidden="true">
        <html:moz-button
          id="tab-group-suggestions-load-cancel"
          data-l10n-id="tab-group-editor-cancel">
        </html:moz-button>
      </html:moz-button-group>
    `;

    static markup = /*html*/ `
    <panel
        type="arrow"
        class="panel tab-group-editor-panel"
        orient="vertical"
        role="dialog"
        ignorekeys="true"
        norolluponanchor="true">

      <html:div id="tab-group-main">
        ${this.headerSection}

        <toolbarseparator />

        <html:div
          class="panel-body
          tab-group-editor-name">
          <html:label
            for="tab-group-name"
            data-l10n-id="tab-group-editor-name-label">
          </html:label>
          <html:input
            id="tab-group-name"
            type="text"
            name="tab-group-name"
            value=""
            data-l10n-id="tab-group-editor-name-field"
          />
        </html:div>

        <html:div
          class="panel-body tab-group-editor-swatches"
          role="radiogroup"
          data-l10n-id="tab-group-editor-color-selector"
        />

        <toolbarseparator class="tab-group-edit-mode-only"/>
        
        ${this.editActions}
        
        <toolbarseparator id="tab-group-suggestions-separator" hidden="true"/>

        ${this.suggestionsButton}

        <html:p
          hidden="true"
          id="tab-group-suggestions-disclaimer"
          data-l10n-id="tab-group-suggestions-disclaimer">
            <a data-l10n-name="support" href="#"></a>
        </html:p>

        <html:moz-button
          hidden="true"
          disabled="true"
          type="icon ghost"
          id="tab-group-suggestions-message"
          data-l10n-id="tab-group-editor-no-tabs-found">
        </html:moz-button>

        ${this.defaultActions}
      
      </html:div>

      ${this.suggestionsHeader}
      ${this.loadingSection}
      ${this.loadingActions}
      ${this.suggestionsSection}
      
    </panel>
       `;

    static State = {
      // Standard create mode (No AI UI)
      CREATE_STANDARD_INITIAL: 0,
      // Create mode with AI able to suggest tabs
      CREATE_AI_INITIAL: 1,
      // No ungrouped tabs to suggest (hide AI interactions)
      CREATE_AI_INITIAL_SUGGESTIONS_DISABLED: 2,
      // Create mode with suggestions
      CREATE_AI_WITH_SUGGESTIONS: 3,
      // Create mode with no suggestions
      CREATE_AI_WITH_NO_SUGGESTIONS: 4,
      // Standard edit mode (No AI UI)
      EDIT_STANDARD_INITIAL: 5,
      // Edit mode with AI able to suggest tabs
      EDIT_AI_INITIAL: 6,
      // No ungrouped tabs to suggest
      EDIT_AI_INITIAL_SUGGESTIONS_DISABLED: 7,
      // Edit mode with suggestions
      EDIT_AI_WITH_SUGGESTIONS: 8,
      // Edit mode with no suggestions
      EDIT_AI_WITH_NO_SUGGESTIONS: 9,
      LOADING: 10,
      ERROR: 11,
    };

    #tabGroupMain;
    #activeGroup;
    #cancelButton;
    #createButton;
    #createMode;
    #keepNewlyCreatedGroup;
    #nameField;
    #panel;
    #swatches;
    #swatchesContainer;
    #defaultActions;
    #suggestionState = MozTabbrowserTabGroupMenu.State.CREATE_STANDARD_INITIAL;
    #suggestionsHeading;
    #suggestionsHeader;
    #suggestionsContainer;
    #suggestions;
    #suggestionButton;
    #cancelSuggestionsButton;
    #createSuggestionsButton;
    #suggestionsLoading;
    #selectSuggestionsToggle;
    #suggestionsMessage;
    #suggestionsDisclaimer;
    #selectedSuggestedTabs = [];
    #suggestedMlLabel;
    #hasSuggestedMlTabs = false;
    #suggestedTabs = [];
    #suggestionsLoadActions;
    #suggestionsLoadCancel;
    #suggestionsSeparator;
    #smartTabGroupingManager;

    constructor() {
      super();
      this.smartTabGroupsEnabled = false;
      XPCOMUtils.defineLazyPreferenceGetter(
        this,
        "smartTabGroupsEnabled",
        "browser.tabs.groups.smart.enabled",
        false,
        this.#onSmartTabGroupsPrefChange.bind(this)
      );
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

      this.#defaultActions = this.querySelector("#tab-group-default-actions");
      this.#tabGroupMain = this.querySelector("#tab-group-main");
      this.#initSuggestions();

      this.#populateSwatches();

      this.#cancelButton.addEventListener("click", () => {
        this.#handleMlTelemetry("cancel");
        this.close(false);
      });

      this.#createButton.addEventListener("click", () => {
        this.#handleMlTelemetry("save");
        this.close();
      });

      this.#nameField.addEventListener("input", () => {
        if (this.activeGroup) {
          this.activeGroup.label = this.#nameField.value;
        }
      });

      /**
       * Check if the smart suggest button should be shown
       * If there are no ungrouped tabs, the button should be hidden
       */
      this.canShowAIUserInterface = () => {
        const { tabs } = gBrowser;
        let show = false;
        tabs.forEach(tab => {
          if (tab.group === null) {
            show = true;
          }
        });

        return show;
      };

      document
        .getElementById("tabGroupEditor_addNewTabInGroup")
        .addEventListener("command", () => {
          this.#handleNewTabInGroup();
        });

      document
        .getElementById("tabGroupEditor_moveGroupToNewWindow")
        .addEventListener("command", () => {
          gBrowser.replaceGroupWithWindow(this.activeGroup);
        });

      document
        .getElementById("tabGroupEditor_ungroupTabs")
        .addEventListener("command", () => {
          this.activeGroup.ungroupTabs();
        });

      document
        .getElementById("tabGroupEditor_saveAndCloseGroup")
        .addEventListener("command", () => {
          this.activeGroup.saveAndClose();
        });

      document
        .getElementById("tabGroupEditor_deleteGroup")
        .addEventListener("command", () => {
          gBrowser.removeTabGroup(this.activeGroup);
        });

      this.panel.addEventListener("popupshown", this);
      this.panel.addEventListener("popuphidden", this);
      this.panel.addEventListener("keypress", this);
      this.#swatchesContainer.addEventListener("change", this);
    }

    #onSmartTabGroupsPrefChange(_preName, _prev, latest) {
      const icon = latest ? MozTabbrowserTabGroupMenu.AI_ICON : "";

      this.#suggestionButton.iconSrc = icon;
      this.#suggestionsMessage.iconSrc = icon;
    }

    #initSuggestions() {
      const { SmartTabGroupingManager } = ChromeUtils.importESModule(
        "moz-src:///browser/components/tabbrowser/SmartTabGrouping.sys.mjs"
      );
      this.#smartTabGroupingManager = new SmartTabGroupingManager();

      // Init Suggestion Button
      this.#suggestionButton = this.querySelector(
        "#tab-group-suggestion-button"
      );
      this.#suggestionButton.iconSrc = this.smartTabGroupsEnabled
        ? MozTabbrowserTabGroupMenu.AI_ICON
        : "";
      this.#suggestionButton.addEventListener("click", () => {
        this.#handleSmartSuggest();
      });

      // Init Suggestions UI
      this.#suggestionsHeading = this.querySelector(
        "#tab-group-suggestions-heading"
      );
      this.#suggestionsHeader = this.querySelector(
        "#tab-group-suggestions-header"
      );
      this.#suggestionsContainer = this.querySelector(
        "#tab-group-suggestions-container"
      );
      this.#suggestions = this.querySelector("#tab-group-suggestions");
      this.#selectSuggestionsToggle = this.querySelector(
        "#tab-group-select-toggle"
      );
      this.#selectSuggestionsToggle.addEventListener("click", () => {
        this.#handleSelectToggle();
      });
      this.#suggestionsMessage = this.querySelector(
        "#tab-group-suggestions-message"
      );
      this.#suggestionsMessage.iconSrc = this.smartTabGroupsEnabled
        ? MozTabbrowserTabGroupMenu.AI_ICON
        : "";
      this.#suggestionsDisclaimer = this.querySelector(
        "#tab-group-suggestions-disclaimer"
      );
      this.#createSuggestionsButton = this.querySelector(
        "#tab-group-create-suggestions-button"
      );
      this.#createSuggestionsButton.addEventListener("click", () => {
        this.#handleMlTelemetry("save");
        this.activeGroup.addTabs(this.#selectedSuggestedTabs);
        this.close(true);
      });
      this.#cancelSuggestionsButton = this.querySelector(
        "#tab-group-cancel-suggestions-button"
      );
      this.#cancelSuggestionsButton.addEventListener("click", () => {
        this.#handleMlTelemetry("cancel");
        this.close();
      });
      this.#suggestionsSeparator = this.querySelector(
        "#tab-group-suggestions-separator"
      );

      // Init Loading UI
      this.#suggestionsLoading = this.querySelector(
        "#tab-group-suggestions-loading"
      );
      this.#suggestionsLoadActions = this.querySelector(
        "#tab-group-suggestions-load-actions"
      );
      this.#suggestionsLoadCancel = this.querySelector(
        "#tab-group-suggestions-load-cancel"
      );
      this.#suggestionsLoadCancel.addEventListener("click", () => {
        this.#handleLoadSuggestionsCancel();
      });
    }

    #populateSwatches() {
      this.#clearSwatches();
      for (let colorCode of MozTabbrowserTabGroupMenu.COLORS) {
        let input = document.createElement("input");
        input.id = `tab-group-editor-swatch-${colorCode}`;
        input.type = "radio";
        input.name = "tab-group-color";
        input.value = colorCode;
        let label = document.createElement("label");
        label.classList.add("tab-group-editor-swatch");
        label.setAttribute(
          "data-l10n-id",
          `tab-group-editor-color-selector2-${colorCode}`
        );
        label.htmlFor = input.id;
        label.style.setProperty(
          "--tabgroup-swatch-color",
          `var(--tab-group-color-${colorCode})`
        );
        label.style.setProperty(
          "--tabgroup-swatch-color-invert",
          `var(--tab-group-color-${colorCode}-invert)`
        );
        this.#swatchesContainer.append(input, label);
        this.#swatches.push(input);
      }
    }

    #clearSwatches() {
      this.#swatchesContainer.innerHTML = "";
      this.#swatches = [];
    }

    get createMode() {
      return this.#createMode;
    }

    set createMode(enableCreateMode) {
      this.#panel.classList.toggle(
        "tab-group-editor-mode-create",
        enableCreateMode
      );
      this.#panel.setAttribute(
        "aria-labelledby",
        enableCreateMode
          ? "tab-group-editor-title-create"
          : "tab-group-editor-title-edit"
      );
      this.#createMode = enableCreateMode;
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

    get nextUnusedColor() {
      let usedColors = [];
      gBrowser.getAllTabGroups().forEach(group => {
        usedColors.push(group.color);
      });
      let color = MozTabbrowserTabGroupMenu.COLORS.find(
        colorCode => !usedColors.includes(colorCode)
      );
      if (!color) {
        // if all colors are used, pick one randomly
        let randomIndex = Math.floor(
          Math.random() * MozTabbrowserTabGroupMenu.COLORS.length
        );
        color = MozTabbrowserTabGroupMenu.COLORS[randomIndex];
      }
      return color;
    }

    get panel() {
      return this.children[0];
    }

    get #panelPosition() {
      if (gBrowser.tabContainer.verticalMode) {
        return SidebarController._positionStart
          ? "topleft topright"
          : "topright topleft";
      }
      return "bottomleft topleft";
    }

    #initMlGroupLabel() {
      if (!this.smartTabGroupsEnabled) {
        return;
      }
      gBrowser.getGroupTitleForTabs(this.activeGroup.tabs).then(newLabel => {
        this.#setMlGroupLabel(newLabel);
      });
    }

    /**
     * Check if the label should be updated with the suggested label
     * @returns {boolean}
     */
    #shouldUpdateLabelWithMlLabel() {
      return !this.#nameField.value && this.panel.state !== "closed";
    }

    /**
     * Attempt to set the label of the group to the suggested label
     * @param {MozTabbrowserTabGroup} group
     * @param {string} newLabel
     * @returns
     */
    #setMlGroupLabel(newLabel) {
      if (!this.#shouldUpdateLabelWithMlLabel()) {
        return;
      }
      this.#activeGroup.label = newLabel;
      this.#nameField.value = newLabel;
      this.#suggestedMlLabel = newLabel;
    }

    openCreateModal(group) {
      this.activeGroup = group;
      this.createMode = true;
      this.suggestionState = this.smartTabGroupsEnabled
        ? MozTabbrowserTabGroupMenu.State.CREATE_AI_INITIAL
        : MozTabbrowserTabGroupMenu.State.CREATE_STANDARD_INITIAL;

      this.#panel.openPopup(group.firstChild, {
        position: this.#panelPosition,
      });
      this.#initMlGroupLabel();
    }

    /*
     * Set the ml generated label - used for testing
     */
    set mlLabel(label) {
      this.#suggestedMlLabel = label;
    }

    get mlLabel() {
      return this.#suggestedMlLabel;
    }

    /*
     * Set if the ml suggest tab flow was done
     */
    set hasSuggestedMlTabs(suggested) {
      this.#hasSuggestedMlTabs = suggested;
    }

    get hasSuggestedMlTabs() {
      return this.#hasSuggestedMlTabs;
    }

    openEditModal(group) {
      this.activeGroup = group;
      this.createMode = false;
      this.suggestionState = this.smartTabGroupsEnabled
        ? MozTabbrowserTabGroupMenu.State.EDIT_AI_INITIAL
        : MozTabbrowserTabGroupMenu.State.EDIT_STANDARD_INITIAL;

      this.#panel.openPopup(group.firstChild, {
        position: this.#panelPosition,
      });
      document.getElementById("tabGroupEditor_moveGroupToNewWindow").disabled =
        gBrowser.openTabs.length == this.activeGroup?.tabs.length;
      this.#maybeDisableOrHideSaveButton();
    }

    #maybeDisableOrHideSaveButton() {
      const saveAndCloseGroup = document.getElementById(
        "tabGroupEditor_saveAndCloseGroup"
      );
      if (PrivateBrowsingUtils.isWindowPrivate(this.ownerGlobal)) {
        saveAndCloseGroup.hidden = true;
        return;
      }

      let flushes = [];
      this.activeGroup.tabs.forEach(tab => {
        flushes.push(TabStateFlusher.flush(tab.linkedBrowser));
      });
      Promise.allSettled(flushes).then(() => {
        saveAndCloseGroup.disabled = !SessionStore.shouldSaveTabGroup(
          this.activeGroup
        );
      });
    }

    close(keepNewlyCreatedGroup = true) {
      if (this.createMode) {
        this.#keepNewlyCreatedGroup = keepNewlyCreatedGroup;
      }
      this.#panel.hidePopup();
    }

    on_popupshown() {
      if (this.createMode) {
        this.#keepNewlyCreatedGroup = true;
      }
      this.#nameField.focus();
    }

    on_popuphidden() {
      if (this.createMode) {
        if (this.#keepNewlyCreatedGroup) {
          this.dispatchEvent(
            new CustomEvent("TabGroupCreateDone", { bubbles: true })
          );
        } else {
          this.activeGroup.ungroupTabs();
        }
      }
      this.activeGroup = null;
      this.#smartTabGroupingManager.terminateProcess();
    }

    on_keypress(event) {
      if (event.defaultPrevented) {
        // The event has already been consumed inside of the panel.
        return;
      }

      switch (event.keyCode) {
        case KeyEvent.DOM_VK_ESCAPE:
          this.close(false);
          break;
        case KeyEvent.DOM_VK_RETURN:
          this.close();
          break;
      }
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

    async #handleNewTabInGroup() {
      let lastTab = this.activeGroup?.tabs.at(-1);
      let onTabOpened = async aEvent => {
        this.activeGroup?.addTabs([aEvent.target]);
        this.close();
        window.removeEventListener("TabOpen", onTabOpened);
      };
      window.addEventListener("TabOpen", onTabOpened);
      gBrowser.addAdjacentNewTab(lastTab);
    }

    /**
     * @param {number} newState - See MozTabbrowserTabGroupMenu.State
     */
    set suggestionState(newState) {
      if (this.#suggestionState === newState) {
        return;
      }
      this.#suggestionState = newState;
      this.#renderSuggestionState();
    }

    #handleLoadSuggestionsCancel() {
      // TODO look into actually canceling any processes
      this.suggestionState = this.createMode
        ? MozTabbrowserTabGroupMenu.State.CREATE_AI_INITIAL
        : MozTabbrowserTabGroupMenu.State.EDIT_AI_INITIAL;
    }

    #handleSelectToggle() {
      const currentState =
        this.#selectSuggestionsToggle.getAttribute("data-state");
      const isDeselect = currentState === "deselect";

      isDeselect ? this.#handleDeselectAll() : this.#handleSelectAll();
      const newState = isDeselect ? "select" : "deselect";
      this.#setSelectToggleState(newState);
    }

    #handleSelectAll() {
      document
        .querySelectorAll(".tab-group-suggestion-checkbox")
        .forEach(checkbox => {
          checkbox.checked = true;
        });
      // Reset selected tabs to all suggested tabs
      this.#selectedSuggestedTabs = this.#suggestedTabs;
    }

    #handleDeselectAll() {
      document
        .querySelectorAll(".tab-group-suggestion-checkbox")
        .forEach(checkbox => {
          checkbox.checked = false;
        });
      this.#selectedSuggestedTabs = [];
    }

    async #handleSmartSuggest() {
      // Loading
      this.suggestionState = MozTabbrowserTabGroupMenu.State.LOADING;
      const tabs = await this.#smartTabGroupingManager.smartTabGroupingForGroup(
        this.activeGroup,
        gBrowser.tabs
      );

      if (!tabs.length) {
        // No un-grouped tabs found
        this.suggestionState = this.#createMode
          ? MozTabbrowserTabGroupMenu.State.CREATE_AI_WITH_NO_SUGGESTIONS
          : MozTabbrowserTabGroupMenu.State.EDIT_AI_WITH_NO_SUGGESTIONS;
        return;
      }

      this.#selectedSuggestedTabs = tabs;
      this.#suggestedTabs = tabs;
      tabs.forEach((tab, index) => {
        this.#createRow(tab, index);
      });

      this.suggestionState = this.#createMode
        ? MozTabbrowserTabGroupMenu.State.CREATE_AI_WITH_SUGGESTIONS
        : MozTabbrowserTabGroupMenu.State.EDIT_AI_WITH_SUGGESTIONS;

      this.#hasSuggestedMlTabs = true;
    }

    /**
     * Sends Glean metrics if smart tab grouping is enabled
     * @param {string} action "save" or "cancel"
     */
    #handleMlTelemetry(action) {
      if (!this.smartTabGroupsEnabled) {
        return;
      }
      if (this.#suggestedMlLabel) {
        this.#smartTabGroupingManager.handleLabelTelemetry({
          action,
          numTabsInGroup: this.#activeGroup.tabs.length,
          mlLabel: this.#suggestedMlLabel,
          userLabel: this.#nameField.value,
        });
        this.#suggestedMlLabel = "";
      }
      if (this.#hasSuggestedMlTabs) {
        this.#smartTabGroupingManager.handleSuggestTelemetry({
          action,
          numTabsInWindow: gBrowser.tabs.length,
          numTabsInGroup: this.#activeGroup.tabs.length,
          numTabsSuggested: this.#suggestedTabs.length,
          numTabsApproved: this.#selectedSuggestedTabs.length,
          numTabsRemoved:
            this.#suggestedTabs.length - this.#selectedSuggestedTabs.length,
        });
        this.#hasSuggestedMlTabs = false;
      }
    }

    #createRow(tab, index) {
      // Create Row
      let row = document.createXULElement("toolbaritem");
      row.setAttribute("context", "tabContextMenu");
      row.setAttribute("id", `tab-bar-${index}`);

      // Create Checkbox
      let checkbox = document.createXULElement("checkbox");
      checkbox.value = tab;
      checkbox.setAttribute("checked", true);
      checkbox.classList.add("tab-group-suggestion-checkbox");
      checkbox.addEventListener("CheckboxStateChange", e => {
        const isChecked = e.target.checked;
        const currentTab = e.target.value;

        if (isChecked) {
          this.#selectedSuggestedTabs.push(currentTab);
        } else {
          this.#selectedSuggestedTabs = this.#selectedSuggestedTabs.filter(
            t => t != currentTab
          );
        }
      });

      row.appendChild(checkbox);

      // Create Row Label
      let label = document.createXULElement("toolbarbutton");
      label.classList.add(
        "all-tabs-button",
        "subviewbutton",
        "subviewbutton-iconic",
        "tab-group-suggestion-label"
      );
      label.setAttribute("flex", "1");
      label.setAttribute("crop", "end");
      label.label = tab.label;
      label.image = tab.image;
      label.disabled = true;
      row.appendChild(label);

      // Apply Row to Suggestions
      this.#suggestions.appendChild(row);
    }

    /**
     * Set the state of the select toggle button
     * @param {string} "select" | "deselect"
     */
    #setSelectToggleState(state) {
      this.#selectSuggestionsToggle.setAttribute("data-state", state);
      this.#selectSuggestionsToggle.setAttribute(
        "data-l10n-id",
        `tab-group-editor-${state}-suggestions`
      );
    }

    /**
     * Element visibility utility function.
     * Toggles the `hidden` attribute of a DOM element.
     *
     * @param {HTMLElement|XULElement} element - The DOM element to show/hide.
     * @param {boolean} shouldShow - Whether the element should be shown (true) or hidden (false).
     */
    #setElementVisibility(element, shouldShow) {
      element.hidden = !shouldShow;
    }

    #showDefaultTabGroupActions(value) {
      this.#setElementVisibility(this.#defaultActions, value);
    }

    #showSmartSuggestionsContainer(value) {
      this.#setElementVisibility(this.#suggestionsContainer, value);
    }

    #showSuggestionButton(value) {
      this.#setElementVisibility(this.#suggestionButton, value);
    }

    #showSuggestionMessage(value) {
      this.#setElementVisibility(this.#suggestionsMessage, value);
    }

    #showSuggestionsDisclaimer(value) {
      this.#setElementVisibility(this.#suggestionsDisclaimer, value);
    }

    #showSuggestionsSeparator(value) {
      this.#setElementVisibility(this.#suggestionsSeparator, value);
    }

    #setLoadingState(value) {
      this.#setElementVisibility(this.#suggestionsLoadActions, value);
      this.#setElementVisibility(this.#suggestionsLoading, value);
    }

    #setSuggestionsButtonCreateModeState(value) {
      const translationString = value
        ? "tab-group-editor-smart-suggest-button-create"
        : "tab-group-editor-smart-suggest-button-edit";

      this.#suggestionButton.setAttribute("data-l10n-id", translationString);
    }

    /**
     * Unique state setter for a "3rd" panel state while in Edit Mode
     * that just shows suggestions and hides the majority of the panel
     * @param {boolean} value
     */
    #setEditModeSuggestionState(value) {
      this.#setElementVisibility(this.#suggestionsHeader, !value);
      this.#setElementVisibility(this.#tabGroupMain, !value);
      this.#setElementVisibility(this.#suggestionsHeading, value);
    }

    #resetCommonUI() {
      this.#setLoadingState(false);
      this.#setEditModeSuggestionState(false);
      this.#suggestedTabs = [];
      this.#selectedSuggestedTabs = [];
      this.#suggestions.innerHTML = "";
      this.#showSmartSuggestionsContainer(false);
    }

    #renderSuggestionState() {
      switch (this.#suggestionState) {
        // CREATE STANDARD INITIAL
        case MozTabbrowserTabGroupMenu.State.CREATE_STANDARD_INITIAL:
          this.#resetCommonUI();
          this.#showDefaultTabGroupActions(true);
          this.#showSuggestionButton(false);
          this.#showSuggestionMessage(false);
          this.#showSuggestionsDisclaimer(false);
          this.#showSuggestionsSeparator(false);
          break;

        //CREATE AI INITIAL
        case MozTabbrowserTabGroupMenu.State.CREATE_AI_INITIAL:
          this.#resetCommonUI();
          this.#showSuggestionButton(true);
          this.#showDefaultTabGroupActions(true);
          this.#showSuggestionMessage(false);
          this.#setSelectToggleState("deselect");
          this.#showSuggestionsDisclaimer(true);
          this.#setSuggestionsButtonCreateModeState(true);
          this.#showSuggestionsSeparator(true);
          break;

        // CREATE AI INITIAL SUGGESTIONS DISABLED
        case MozTabbrowserTabGroupMenu.State
          .CREATE_AI_INITIAL_SUGGESTIONS_DISABLED:
          this.#resetCommonUI();
          this.#showSuggestionButton(false);
          this.#showSuggestionsDisclaimer(false);
          this.#showSuggestionMessage(true);
          this.#showDefaultTabGroupActions(true);
          this.#showSuggestionsSeparator(true);
          break;

        // CREATE AI WITH SUGGESTIONS
        case MozTabbrowserTabGroupMenu.State.CREATE_AI_WITH_SUGGESTIONS:
          this.#setLoadingState(false);
          this.#showSmartSuggestionsContainer(true);
          this.#showSuggestionButton(false);
          this.#showSuggestionsSeparator(true);
          this.#showDefaultTabGroupActions(false);
          break;

        // CREATE AI WITH NO SUGGESTIONS
        case MozTabbrowserTabGroupMenu.State.CREATE_AI_WITH_NO_SUGGESTIONS:
          this.#setLoadingState(false);
          this.#showSuggestionMessage(true);
          this.#showDefaultTabGroupActions(true);
          this.#showSuggestionButton(false);
          this.#showSuggestionsSeparator(true);
          break;

        // EDIT STANDARD INITIAL
        case MozTabbrowserTabGroupMenu.State.EDIT_STANDARD_INITIAL:
          this.#resetCommonUI();
          this.#showSuggestionButton(false);
          this.#showSuggestionMessage(false);
          this.#showDefaultTabGroupActions(false);
          this.#showSuggestionsDisclaimer(false);
          this.#showSuggestionsSeparator(false);
          break;

        // EDIT AI INITIAL
        case MozTabbrowserTabGroupMenu.State.EDIT_AI_INITIAL:
          this.#resetCommonUI();
          this.#showSuggestionMessage(false);
          this.#setSelectToggleState("deselect");
          this.#showSuggestionButton(true);
          this.#showDefaultTabGroupActions(false);
          this.#showSuggestionsDisclaimer(false);
          this.#setSuggestionsButtonCreateModeState(false);
          this.#showSuggestionsSeparator(true);
          break;

        // EDIT AI INITIAL SUGGESTIONS DISABLED
        case MozTabbrowserTabGroupMenu.State
          .EDIT_AI_INITIAL_SUGGESTIONS_DISABLED:
          this.#resetCommonUI();
          this.#showSuggestionMessage(true);
          this.#showSuggestionButton(false);
          this.#showDefaultTabGroupActions(false);
          this.#showSuggestionsDisclaimer(false);
          this.#showSuggestionsSeparator(true);
          break;

        // EDIT AI WITH SUGGESTIONS
        case MozTabbrowserTabGroupMenu.State.EDIT_AI_WITH_SUGGESTIONS:
          this.#setLoadingState(false);
          this.#showSmartSuggestionsContainer(true);
          this.#setEditModeSuggestionState(true);
          this.#showSuggestionsSeparator(false);
          break;

        // EDIT AI WITH NO SUGGESTIONS
        case MozTabbrowserTabGroupMenu.State.EDIT_AI_WITH_NO_SUGGESTIONS:
          this.#setLoadingState(false);
          this.#showSuggestionMessage(true);
          this.#showSuggestionsSeparator(true);
          break;

        // LOADING
        case MozTabbrowserTabGroupMenu.State.LOADING:
          this.#showDefaultTabGroupActions(false);
          this.#showSuggestionButton(false);
          this.#showSuggestionsDisclaimer(false);
          this.#showSuggestionMessage(false);
          this.#setLoadingState(true);
          this.#showSuggestionsSeparator(true);
          this.#showDefaultTabGroupActions(false);
          break;

        // ERROR
        case MozTabbrowserTabGroupMenu.State.ERROR:
          //TODO
          break;
      }
    }
  }

  customElements.define("tabgroup-menu", MozTabbrowserTabGroupMenu);
}
