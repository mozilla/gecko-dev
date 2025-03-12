/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "verticalTabsEnabled",
  "sidebar.verticalTabs"
);

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
 * @property {number} collapsedLauncherWidth
 *   Width that the sidebar launcher should collapse to.
 */

const LAUNCHER_MINIMUM_WIDTH = 100;
const SIDEBAR_MAXIMUM_WIDTH = "75vw";

const LEGACY_USED_PREF = "sidebar.old-sidebar.has-used";
const REVAMP_USED_PREF = "sidebar.new-sidebar.has-used";

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
    launcherVisible: true,
    launcherExpanded: false,
    launcherDragActive: false,
    launcherHoverActive: false,
    collapsedLauncherWidth: undefined,
    command: undefined,
  };
  #previousLauncherExpanded = undefined;

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
   * Get the sidebar panel element.
   *
   * @returns {XULElement}
   */
  get #sidebarBoxEl() {
    return this.#controller._box;
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
    } else {
      this.launcherVisible = true;
    }

    // Explicitly trigger effects to ensure that the UI is kept up to date.
    if (lazy.verticalTabsEnabled) {
      this.#props.launcherExpanded = true;
    }
    this.launcherExpanded = this.#props.launcherExpanded;
  }

  /**
   * Load the state information given by session store, backup state, or
   * adopted window.
   *
   * @param {SidebarStateProps} props
   *   New properties to overwrite the default state with.
   */
  loadInitialState(props) {
    for (const [key, value] of Object.entries(props)) {
      if (value === undefined) {
        // `undefined` means we should use the default value.
        continue;
      }
      switch (key) {
        case "command":
          this.command = value;
          break;
        case "panelWidth":
          this.#panelEl.style.width = `${value}px`;
          break;
        case "width":
          this.#panelEl.style.width = value;
          break;
        case "expanded":
          this.launcherExpanded = value;
          break;
        case "hidden":
          this.launcherVisible = !value;
          break;
        case "panelOpen":
          // we need to know if we have a command value before finalizing panelOpen
          break;
        default:
          this[key] = value;
      }
    }

    if (this.command && !props.hasOwnProperty("panelOpen")) {
      // legacy state saved before panelOpen was a thing
      props.panelOpen = true;
    }
    if (!this.command) {
      props.panelOpen = false;
    }
    this.panelOpen = !!props.panelOpen;
    if (this.command && this.panelOpen) {
      this.launcherVisible = true;
      // show() is async, so make sure we return its promise here
      return this.#controller.showInitially(this.command);
    }
    return this.#controller.hide();
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
    return {
      command: this.command,
      panelOpen: this.panelOpen,
      panelWidth: this.panelWidth,
      launcherWidth: convertToInt(this.launcherWidth),
      expandedLauncherWidth: convertToInt(this.expandedLauncherWidth),
      launcherExpanded: this.launcherExpanded,
      launcherVisible: this.launcherVisible,
      collapsedLauncherWidth:
        typeof this.collapsedLauncherWidth === "number"
          ? Math.round(this.collapsedLauncherWidth)
          : this.collapsedLauncherWidth,
    };
  }

  get panelOpen() {
    return this.#props.panelOpen;
  }

  set panelOpen(open) {
    if (this.#props.panelOpen == open) {
      return;
    }
    this.#props.panelOpen = !!open;
    if (open) {
      // Launcher must be visible to open a panel.
      this.launcherVisible = true;
      // We need to know how to revert the launcher when the panel closes
      this.#previousLauncherExpanded = this.launcherExpanded;

      Services.prefs.setBoolPref(
        this.revampEnabled ? REVAMP_USED_PREF : LEGACY_USED_PREF,
        true
      );
    } else if (this.revampVisibility === "hide-sidebar") {
      this.launcherExpanded = lazy.verticalTabsEnabled
        ? this.#previousLauncherExpanded
        : false;
    }
  }

  get panelWidth() {
    // Use the value from `style`. This is a more accurate user preference, as
    // opposed to what the resize observer gives us.
    return convertToInt(this.#panelEl.style.width);
  }

  set panelWidth(width) {
    this.#launcherContainerEl.style.maxWidth = `calc(${SIDEBAR_MAXIMUM_WIDTH} - ${width}px)`;
  }

  get launcherVisible() {
    return this.#props.launcherVisible;
  }

  /**
   * Update the launcher `visible` and `expanded` states to handle the
   * following scenarios:
   *
   * - Toggling "Hide tabs and sidebar" from the customize panel.
   * - Clicking sidebar button from the toolbar.
   * - Removing sidebar button from the toolbar.
   * - Force expand value
   *
   * @param {boolean} visible
   * @param {boolean} onUserToggle
   * @param {boolean} onToolbarButtonRemoval
   * @param {boolean} forceExpandValue
   */
  updateVisibility(
    visible,
    onUserToggle = false,
    onToolbarButtonRemoval = false,
    forceExpandValue = null
  ) {
    switch (this.revampVisibility) {
      case "hide-sidebar":
        if (onToolbarButtonRemoval) {
          // If we are hiding the sidebar because we removed the toolbar button, close everything
          this.#previousLauncherExpanded = false;
          this.launcherVisible = false;
          this.launcherExpanded = false;

          if (this.panelOpen) {
            this.#controller.hide();
          }
          return;
        }
        // we need this set to verticalTabsEnabled to ensure it has the correct state when toggling the sidebar button
        this.launcherExpanded = lazy.verticalTabsEnabled && visible;
        if (!visible && this.panelOpen) {
          if (onUserToggle) {
            // Hiding the launcher with the toolbar button or context menu should also close out any open panels and resets panelOpen
            this.#controller.hide();
          } else {
            // Hide the launcher when the pref is set to hide-sidebar
            this.launcherVisible = false;
            this.#previousLauncherExpanded = false;
            return;
          }
        }
        this.launcherVisible = visible;
        break;
      case "always-show":
      case "expand-on-hover":
        this.launcherVisible = true;
        if (forceExpandValue !== null) {
          this.launcherExpanded = forceExpandValue;
        } else if (onUserToggle) {
          this.launcherExpanded = !this.launcherExpanded;
        }
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
    this.#sidebarBoxEl.style.paddingInlineStart =
      this.panelOpen && !visible ? "var(--space-small)" : "unset";
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
    const previousExpanded = this.#props.launcherExpanded;
    this.#props.launcherExpanded = expanded;
    this.#launcherEl.expanded = expanded;
    if (expanded && !previousExpanded) {
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

  get launcherHoverActive() {
    return this.#props.launcherHoverActive;
  }

  set launcherHoverActive(active) {
    this.#props.launcherHoverActive = active;
  }

  get launcherWidth() {
    return this.#props.launcherWidth;
  }

  set launcherWidth(width) {
    this.#props.launcherWidth = width;
    const { document } = this.#controllerGlobal;
    if (!document.documentElement.hasAttribute("inDOMFullscreen")) {
      this.#panelEl.style.maxWidth = `calc(${SIDEBAR_MAXIMUM_WIDTH} - ${width}px)`;
      // Expand the launcher when it gets wide enough.
      if (this.launcherDragActive) {
        this.launcherExpanded = width >= LAUNCHER_MINIMUM_WIDTH;
      }
    }
  }

  get expandedLauncherWidth() {
    return this.#props.expandedLauncherWidth;
  }

  set expandedLauncherWidth(width) {
    this.#props.expandedLauncherWidth = width;
    this.#updateLauncherWidth();
  }

  get collapsedLauncherWidth() {
    return this.#props.collapsedLauncherWidth;
  }

  set collapsedLauncherWidth(width) {
    this.#props.collapsedLauncherWidth = width;
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

  get command() {
    return this.#props.command || "";
  }

  set command(id) {
    if (id && !this.#controller.sidebars.has(id)) {
      throw new Error("Setting command to an invalid value");
    }
    if (id && id !== this.#props.command) {
      this.#props.command = id;
      // We need the attribute to mirror the command property as its used as a CSS hook
      this.#controller._box.setAttribute("sidebarcommand", id);
    } else if (!id) {
      delete this.#props.command;
      this.#controller._box.setAttribute("sidebarcommand", "");
    }
  }
}

/**
 * Convert a value to an integer.
 *
 * @param {string} value
 *   The value to convert.
 * @returns {number}
 *   The resulting integer, or `undefined` if it's not a number.
 */
function convertToInt(value) {
  const intValue = parseInt(value);
  if (isNaN(intValue)) {
    return undefined;
  }
  return intValue;
}
