/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * The properties that make up a sidebar's UI state.
 *
 * @typedef {object} SidebarStateProps
 *
 * @property {boolean} panelOpen
 *   Whether there is an open panel.
 * @property {number} panelWidth
 *   Current width of the sidebar panel.
 * @property {boolean} launcherVisible
 *   Whether the launcher is visible.
 * @property {boolean} launcherExpanded
 *   Whether the launcher is expanded.
 * @property {boolean} launcherDragActive
 *   Whether the launcher is currently being dragged.
 * @property {boolean} launcherHoverActive
 *   Whether the launcher is currently being hovered.
 * @property {number} launcherWidth
 *   Current width of the sidebar launcher.
 * @property {number} expandedLauncherWidth
 *   Width that the sidebar launcher should expand to.
 */

const LAUNCHER_MINIMUM_WIDTH = 100;

/**
 * A reactive data store for the sidebar's UI state. Similar to Lit's
 * ReactiveController, any updates to properties can potentially trigger UI
 * updates, or updates to other properties.
 */
export class SidebarState {
  #controller = null;
  /** @type {SidebarStateProps} */
  #props = {
    panelOpen: false,
    panelWidth: undefined,
    launcherVisible: true,
    launcherExpanded: false,
    launcherDragActive: false,
    launcherHoverActive: false,
    launcherWidth: undefined,
    expandedLauncherWidth: undefined,
  };
  #previousExpandedState = false;
  #previousLauncherVisible = undefined;

  /**
   * Construct a new SidebarState.
   *
   * @param {SidebarController} controller
   *   The controller this state belongs to. SidebarState is instantiated
   *   per-window, thereby allowing us to retrieve DOM elements attached to
   *   the controller.
   */
  constructor(controller) {
    this.#controller = controller;
    this.revampEnabled = controller.sidebarRevampEnabled;
    this.revampVisibility = controller.sidebarRevampVisibility;
  }

  /**
   * Get the sidebar launcher.
   *
   * @returns {HTMLElement}
   */
  get #launcherEl() {
    return this.#controller.sidebarMain;
  }

  /**
   * Get parent element of the sidebar launcher.
   *
   * @returns {XULElement}
   */
  get #launcherContainerEl() {
    return this.#controller.sidebarContainer;
  }

  /**
   * Get the sidebar panel.
   *
   * @returns {XULElement}
   */
  get #panelEl() {
    return this.#controller._box;
  }

  /**
   * Get window object from the controller.
   */
  get #controllerGlobal() {
    return this.#launcherContainerEl.ownerGlobal;
  }

  /**
   * Update the starting properties according to external factors such as
   * window type and user preferences.
   */
  initializeState() {
    const isPopup = !this.#controllerGlobal.toolbar.visible;
    if (isPopup) {
      // Don't show launcher if we're in a popup window.
      this.launcherVisible = false;
    } else if (this.revampVisibility === "hide-sidebar" && !this.panelOpen) {
      // When using "Show and hide sidebar", launcher will start off hidden.
      this.launcherVisible = false;
    } else if (this.revampVisibility === "always-show") {
      // When using "Expand and collapse sidebar", launcher must be visible.
      this.launcherVisible = true;
    }
  }

  /**
   * Update state properties.
   *
   * @param {SidebarStateProps} props
   *   New properties to merge with the current state.
   */
  updateState(props) {
    for (const [key, value] of Object.entries(props)) {
      if (Object.hasOwn(this.#props, key) && value !== undefined) {
        this[key] = value;
      }
    }
  }

  /**
   * Toggle the value of a boolean property.
   *
   * @param {string} key
   *   The property to toggle.
   */
  toggle(key) {
    if (Object.hasOwn(this.#props, key)) {
      this[key] = !this[key];
    }
  }

  /**
   * Serialize the state properties for persistence in session store or prefs.
   *
   * @returns {SidebarStateProps}
   */
  getProperties() {
    // TODO: Bug 1935482 - Replace legacy properties with SidebarState properties
    return {
      width: this.#panelEl.style.width,
      command: this.#controller.currentID,
      expanded: this.launcherExpanded,
      hidden: !this.launcherVisible,
      launcherWidth:
        typeof this.launcherWidth === "number"
          ? Math.round(this.launcherWidth)
          : this.launcherWidth,
      expandedLauncherWidth:
        typeof this.expandedLauncherWidth === "number"
          ? Math.round(this.expandedLauncherWidth)
          : this.expandedLauncherWidth,
    };
  }

  get panelOpen() {
    return this.#props.panelOpen;
  }

  set panelOpen(open) {
    this.#props.panelOpen = open;
    if (open) {
      // Launcher must be visible to open a panel.
      this.#previousLauncherVisible = this.launcherVisible;
      this.launcherVisible = true;

      // Whenever a panel is shown, the sidebar is collapsed. Upon hiding
      // that panel afterwards, `expanded` reverts back to what it was prior
      // to calling `show()`. Thus, we store the expanded state at this point.
      this.#previousExpandedState = this.launcherExpanded;
      this.launcherExpanded = false;
    } else if (this.revampVisibility === "hide-sidebar") {
      // When visibility is set to "Hide Sidebar", revert back to an expanded state except
      // when a panel was opened via keyboard shortcut and the launcher was previously hidden.
      this.launcherExpanded = this.#previousLauncherVisible;
      this.launcherVisible = this.#previousLauncherVisible;
    } else {
      this.launcherExpanded = this.#previousExpandedState;
    }
  }

  get panelWidth() {
    return this.#props.panelWidth;
  }

  set panelWidth(width) {
    this.#props.panelWidth = width;
    this.#panelEl.style.width = width;
  }

  get launcherVisible() {
    return this.#props.launcherVisible;
  }

  updateVisibility(visible, openedByToolbarButton = false) {
    switch (this.revampVisibility) {
      case "hide-sidebar":
        if (!openedByToolbarButton && !visible && this.panelOpen) {
          // no-op to handle the case when a user changes the visibility setting via the
          // customize panel, we don't want to close anything on them.
          return;
        }
        // we need this set to true to ensure it has the correct state when toggling the sidebar button
        this.launcherExpanded = true;

        if (!visible && this.panelOpen) {
          // Hiding the launcher should also close out any open panels and resets panelOpen
          this.#controller.hide();
        }
        this.launcherVisible = visible;
        break;
      case "always-show":
        this.launcherVisible = true;
        this.launcherExpanded = !this.launcherExpanded;
        break;
    }
  }

  set launcherVisible(visible) {
    if (!this.revampEnabled) {
      // Launcher not supported in legacy sidebar.
      this.#props.launcherVisible = false;
      this.#launcherContainerEl.hidden = true;
      return;
    }
    this.#props.launcherVisible = visible;
    this.#launcherContainerEl.hidden = !visible;
    this.#updateTabbrowser(visible);
  }

  get launcherExpanded() {
    return this.#props.launcherExpanded;
  }

  set launcherExpanded(expanded) {
    if (!this.revampEnabled) {
      // Launcher not supported in legacy sidebar.
      this.#props.launcherExpanded = false;
      return;
    }
    this.#props.launcherExpanded = expanded;
    this.#launcherEl.expanded = expanded;
    if (expanded) {
      Glean.sidebar.expand.record();
    }
    // Marking the tab container element as expanded or not simplifies the CSS logic
    // and selectors considerably.
    const { tabContainer } = this.#controllerGlobal.gBrowser;
    tabContainer.toggleAttribute("expanded", expanded);
    this.#controller.updateToolbarButton();
    if (!this.launcherDragActive) {
      this.#updateLauncherWidth();
    }
  }

  get launcherDragActive() {
    return this.#props.launcherDragActive;
  }

  set launcherDragActive(active) {
    this.#props.launcherDragActive = active;
    if (active) {
      this.#launcherEl.toggleAttribute("customWidth", true);
    } else if (this.launcherWidth < LAUNCHER_MINIMUM_WIDTH) {
      // Snap back to collapsed state when the new width is too narrow.
      this.launcherExpanded = false;
      if (this.revampVisibility === "hide-sidebar") {
        this.launcherVisible = false;
      }
    } else {
      // Store the user-preferred launcher width.
      this.expandedLauncherWidth = this.launcherWidth;
    }
  }

  get launcherWidth() {
    return this.#props.launcherWidth;
  }

  set launcherWidth(width) {
    this.#props.launcherWidth = width;
    // Expand the launcher when it gets wide enough.
    this.launcherExpanded = width >= LAUNCHER_MINIMUM_WIDTH;
  }

  get expandedLauncherWidth() {
    return this.#props.expandedLauncherWidth;
  }

  set expandedLauncherWidth(width) {
    this.#props.expandedLauncherWidth = width;
    this.#updateLauncherWidth();
  }

  /**
   * If the sidebar is expanded, resize the launcher to the user-preferred
   * width (if available). If it is collapsed, reset the launcher width.
   */
  #updateLauncherWidth() {
    if (this.launcherExpanded && this.expandedLauncherWidth) {
      this.#launcherContainerEl.style.width = `${this.expandedLauncherWidth}px`;
    } else if (!this.launcherExpanded) {
      this.#launcherContainerEl.style.width = "";
    }
    this.#launcherEl.toggleAttribute(
      "customWidth",
      !!this.expandedLauncherWidth
    );
  }

  #updateTabbrowser(isSidebarShown) {
    this.#controllerGlobal.document
      .getElementById("tabbrowser-tabbox")
      .toggleAttribute("sidebar-shown", isSidebarShown);
  }
}
