/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const flags = require("resource://devtools/shared/flags.js");
const ToolDefinitions =
  require("resource://devtools/client/definitions.js").Tools;
const CssLogic = require("resource://devtools/shared/inspector/css-logic.js");
const {
  style: { ELEMENT_STYLE },
} = require("resource://devtools/shared/constants.js");
const OutputParser = require("resource://devtools/client/shared/output-parser.js");
const { PrefObserver } = require("resource://devtools/client/shared/prefs.js");
const {
  createChild,
} = require("resource://devtools/client/inspector/shared/utils.js");
const {
  VIEW_NODE_SELECTOR_TYPE,
  VIEW_NODE_PROPERTY_TYPE,
  VIEW_NODE_VALUE_TYPE,
  VIEW_NODE_IMAGE_URL_TYPE,
  VIEW_NODE_FONT_TYPE,
} = require("resource://devtools/client/inspector/shared/node-types.js");
const TooltipsOverlay = require("resource://devtools/client/inspector/shared/tooltips-overlay.js");

loader.lazyRequireGetter(
  this,
  "StyleInspectorMenu",
  "resource://devtools/client/inspector/shared/style-inspector-menu.js"
);
loader.lazyRequireGetter(
  this,
  "KeyShortcuts",
  "resource://devtools/client/shared/key-shortcuts.js"
);
loader.lazyRequireGetter(
  this,
  "clipboardHelper",
  "resource://devtools/shared/platform/clipboard.js"
);
loader.lazyRequireGetter(
  this,
  "openContentLink",
  "resource://devtools/client/shared/link.js",
  true
);

const STYLE_INSPECTOR_PROPERTIES =
  "devtools/shared/locales/styleinspector.properties";
const { LocalizationHelper } = require("resource://devtools/shared/l10n.js");
const STYLE_INSPECTOR_L10N = new LocalizationHelper(STYLE_INSPECTOR_PROPERTIES);
const L10N_TWISTY_EXPAND_LABEL = STYLE_INSPECTOR_L10N.getStr(
  "rule.twistyExpand.label"
);
const L10N_TWISTY_COLLAPSE_LABEL = STYLE_INSPECTOR_L10N.getStr(
  "rule.twistyCollapse.label"
);
const L10N_EMPTY_VARIABLE = STYLE_INSPECTOR_L10N.getStr("rule.variableEmpty");

const FILTER_CHANGED_TIMEOUT = 150;

/**
 * Helper for long-running processes that should yield occasionally to
 * the mainloop.
 */
class UpdateProcess {
  /**
   * @param {Window} win
   *        Timeouts will be set on this window when appropriate.
   * @param {Array} array
   *        The array of items to process.
   * @param {Object} options
   *        Options for the update process:
   *          onItem {function} Will be called with the value of each iteration.
   *          onBatch {function} Will be called after each batch of iterations,
   *            before yielding to the main loop.
   *          onDone {function} Will be called when iteration is complete.
   *          onCancel {function} Will be called if the process is canceled.
   *          threshold {int} How long to process before yielding, in ms.
   */
  constructor(win, array, options) {
    this.win = win;
    this.index = 0;
    this.array = array;

    this.onItem = options.onItem || function () {};
    this.onBatch = options.onBatch || function () {};
    this.onDone = options.onDone || function () {};
    this.onCancel = options.onCancel || function () {};
    this.threshold = options.threshold || 45;
  }

  #canceled = false;
  #timeout = null;

  /**
   * Symbol returned when the array of items to process is empty.
   */
  static ITERATION_DONE = Symbol("UpdateProcess iteration done");

  /**
   * Schedule a new batch on the main loop.
   */
  schedule() {
    if (this.#canceled) {
      return;
    }
    this.#timeout = setTimeout(() => this.#timeoutHandler(), 0);
  }

  /**
   * Cancel the running process.  onItem will not be called again,
   * and onCancel will be called.
   */
  cancel() {
    if (this.#timeout) {
      clearTimeout(this.#timeout);
      this.#timeout = null;
    }
    this.#canceled = true;
    this.onCancel();
  }

  #timeoutHandler() {
    this.#timeout = null;
    if (this.#runBatch() === UpdateProcess.ITERATION_DONE) {
      this.onBatch();
      this.onDone();
      return;
    }
    this.schedule();
  }

  #runBatch() {
    const time = Date.now();
    while (!this.#canceled) {
      const next = this.#next();
      if (next === UpdateProcess.ITERATION_DONE) {
        return next;
      }

      this.onItem(next);
      if (Date.now() - time > this.threshold) {
        this.onBatch();
        return null;
      }
    }
    return null;
  }

  /**
   * Returns the item at the current index and increases the index.
   * If all items have already been processed, will return ITERATION_DONE.
   */
  #next() {
    if (this.index < this.array.length) {
      return this.array[this.index++];
    }
    return UpdateProcess.ITERATION_DONE;
  }
}

/**
 * CssComputedView is a panel that manages the display of a table
 * sorted by style. There should be one instance of CssComputedView
 * per style display (of which there will generally only be one).
 */
class CssComputedView {
  /**
   * @param {Inspector} inspector
   *        Inspector toolbox panel
   * @param {Document} document
   *        The document that will contain the computed view.
   */
  constructor(inspector, document) {
    this.inspector = inspector;
    this.styleDocument = document;
    this.styleWindow = this.styleDocument.defaultView;

    this.propertyViews = [];

    this.#outputParser = new OutputParser(document, inspector.cssProperties);

    // Create bound methods.
    this.focusWindow = this.focusWindow.bind(this);
    this.refreshPanel = this.refreshPanel.bind(this);

    const doc = this.styleDocument;
    this.element = doc.getElementById("computed-property-container");
    this.searchField = doc.getElementById("computed-searchbox");
    this.searchClearButton = doc.getElementById("computed-searchinput-clear");
    this.includeBrowserStylesCheckbox = doc.getElementById(
      "browser-style-checkbox"
    );

    this.#abortController = new AbortController();
    const opts = { signal: this.#abortController.signal };

    this.shortcuts = new KeyShortcuts({ window: this.styleWindow });
    this.shortcuts.on(
      "CmdOrCtrl+F",
      event => this.#onShortcut("CmdOrCtrl+F", event),
      opts
    );
    this.shortcuts.on(
      "Escape",
      event => this.#onShortcut("Escape", event),
      opts
    );
    this.styleDocument.addEventListener("copy", this.#onCopy, opts);
    this.styleDocument.addEventListener("mousedown", this.focusWindow, opts);
    this.element.addEventListener("click", this.#onClick, opts);
    this.element.addEventListener("contextmenu", this.#onContextMenu, opts);
    this.searchField.addEventListener("input", this.#onFilterStyles, opts);
    this.searchClearButton.addEventListener("click", this.#onClearSearch, opts);
    this.includeBrowserStylesCheckbox.addEventListener(
      "input",
      this.#onIncludeBrowserStyles,
      opts
    );

    if (flags.testing) {
      // In tests, we start listening immediately to avoid having to simulate a mousemove.
      this.highlighters.addToView(this);
    } else {
      this.element.addEventListener(
        "mousemove",
        () => {
          this.highlighters.addToView(this);
        },
        { once: true, signal: this.#abortController.signal }
      );
    }

    if (!this.inspector.is3PaneModeEnabled) {
      // When the rules view is added in 3 pane mode, refresh the Computed view whenever
      // the rules are changed.
      this.inspector.once(
        "ruleview-added",
        () => {
          this.ruleView.on("ruleview-changed", this.refreshPanel, opts);
        },
        opts
      );
    }

    if (this.ruleView) {
      this.ruleView.on("ruleview-changed", this.refreshPanel, opts);
    }

    this.searchClearButton.hidden = true;

    // No results text.
    this.noResults = this.styleDocument.getElementById("computed-no-results");

    // Refresh panel when color unit changed or pref for showing
    // original sources changes.
    this.#prefObserver = new PrefObserver("devtools.");
    this.#prefObserver.on(
      "devtools.defaultColorUnit",
      this.#handlePrefChange,
      opts
    );

    // The PageStyle front related to the currently selected element
    this.viewedElementPageStyle = null;

    this.createStyleViews();

    // Add the tooltips and highlightersoverlay
    this.tooltips = new TooltipsOverlay(this);
  }

  /**
   * Lookup a l10n string in the shared styleinspector string bundle.
   *
   * @param {String} name
   *        The key to lookup.
   * @returns {String} localized version of the given key.
   */
  static l10n(name) {
    try {
      return STYLE_INSPECTOR_L10N.getStr(name);
    } catch (ex) {
      console.log("Error reading '" + name + "'");
      throw new Error("l10n error with " + name);
    }
  }

  #abortController;
  #contextMenu;
  #computed;
  #createViewsProcess;
  #createViewsPromise;
  // Used for cancelling timeouts in the style filter.
  #filterChangedTimeout = null;
  #highlighters;
  #isDestroyed = false;
  // Cache the list of properties that match the selected element.
  #matchedProperties = null;
  #outputParser = null;
  #prefObserver;
  #refreshProcess;
  #sourceFilter;
  // The element that we're inspecting, and the document that it comes from.
  #viewedElement = null;

  // Number of visible properties
  numVisibleProperties = 0;

  get outputParser() {
    return this.#outputParser;
  }

  get computed() {
    return this.#computed;
  }

  get contextMenu() {
    if (!this.#contextMenu) {
      this.#contextMenu = new StyleInspectorMenu(this);
    }

    return this.#contextMenu;
  }

  // Get the highlighters overlay from the Inspector.
  get highlighters() {
    if (!this.#highlighters) {
      // highlighters is a lazy getter in the inspector.
      this.#highlighters = this.inspector.highlighters;
    }

    return this.#highlighters;
  }

  get includeBrowserStyles() {
    return this.includeBrowserStylesCheckbox.checked;
  }

  get ruleView() {
    return (
      this.inspector.hasPanel("ruleview") &&
      this.inspector.getPanel("ruleview").view
    );
  }

  get viewedElement() {
    return this.#viewedElement;
  }

  #handlePrefChange = () => {
    if (this.#computed) {
      this.refreshPanel();
    }
  };

  /**
   * Update the view with a new selected element. The CssComputedView panel
   * will show the style information for the given element.
   *
   * @param {NodeFront} element
   *        The highlighted node to get styles for.
   * @returns a promise that will be resolved when highlighting is complete.
   */
  selectElement(element) {
    if (!element) {
      if (this.viewedElementPageStyle) {
        this.viewedElementPageStyle.off(
          "stylesheet-updated",
          this.refreshPanel
        );
        this.viewedElementPageStyle = null;
      }
      this.#viewedElement = null;
      this.noResults.hidden = false;

      if (this.#refreshProcess) {
        this.#refreshProcess.cancel();
      }
      // Hiding all properties
      for (const propView of this.propertyViews) {
        propView.refresh();
      }
      return Promise.resolve(undefined);
    }

    if (element === this.#viewedElement) {
      return Promise.resolve(undefined);
    }

    if (this.viewedElementPageStyle) {
      this.viewedElementPageStyle.off("stylesheet-updated", this.refreshPanel);
    }
    this.viewedElementPageStyle = element.inspectorFront.pageStyle;
    this.viewedElementPageStyle.on("stylesheet-updated", this.refreshPanel, {
      signal: this.#abortController.signal,
    });

    this.#viewedElement = element;

    this.refreshSourceFilter();

    return this.refreshPanel();
  }

  /**
   * Get the type of a given node in the computed-view
   *
   * @param {DOMNode} node
   *        The node which we want information about
   * @return {Object} The type information object contains the following props:
   * - view {String} Always "computed" to indicate the computed view.
   * - type {String} One of the VIEW_NODE_XXX_TYPE const in
   *   client/inspector/shared/node-types
   * - value {Object} Depends on the type of the node
   * returns null if the node isn't anything we care about
   */
  // eslint-disable-next-line complexity
  getNodeInfo(node) {
    if (!node) {
      return null;
    }

    const classes = node.classList;

    // Check if the node isn't a selector first since this doesn't require
    // walking the DOM
    if (
      classes.contains("matched") ||
      classes.contains("bestmatch") ||
      classes.contains("parentmatch")
    ) {
      let selectorText = "";

      for (const child of node.childNodes[1].childNodes) {
        if (child.nodeType === node.TEXT_NODE) {
          selectorText += child.textContent;
        }
      }
      return {
        type: VIEW_NODE_SELECTOR_TYPE,
        value: selectorText.trim(),
      };
    }

    const propertyView = node.closest(".computed-property-view");
    const propertyMatchedSelectors = node.closest(".matchedselectors");
    const parent = propertyMatchedSelectors || propertyView;

    if (!parent) {
      return null;
    }

    let value, type;

    // Get the property and value for a node that's a property name or value
    const isHref =
      classes.contains("theme-link") && !classes.contains("computed-link");

    if (classes.contains("computed-font-family")) {
      if (propertyMatchedSelectors) {
        const view = propertyMatchedSelectors.closest("li");
        value = {
          property: view.querySelector(".computed-property-name").firstChild
            .textContent,
          value: node.parentNode.textContent,
        };
      } else if (propertyView) {
        value = {
          property: parent.querySelector(".computed-property-name").firstChild
            .textContent,
          value: node.parentNode.textContent,
        };
      } else {
        return null;
      }
    } else if (
      propertyMatchedSelectors &&
      (classes.contains("computed-other-property-value") || isHref)
    ) {
      const view = propertyMatchedSelectors.closest("li");
      value = {
        property: view.querySelector(".computed-property-name").firstChild
          .textContent,
        value: node.textContent,
      };
    } else if (
      propertyView &&
      (classes.contains("computed-property-name") ||
        classes.contains("computed-property-value") ||
        isHref)
    ) {
      value = {
        property: parent.querySelector(".computed-property-name").firstChild
          .textContent,
        value: parent.querySelector(".computed-property-value").textContent,
      };
    }

    // Get the type
    if (classes.contains("computed-property-name")) {
      type = VIEW_NODE_PROPERTY_TYPE;
    } else if (
      classes.contains("computed-property-value") ||
      classes.contains("computed-other-property-value")
    ) {
      type = VIEW_NODE_VALUE_TYPE;
    } else if (classes.contains("computed-font-family")) {
      type = VIEW_NODE_FONT_TYPE;
    } else if (isHref) {
      type = VIEW_NODE_IMAGE_URL_TYPE;
      value.url = node.href;
    } else {
      return null;
    }

    return {
      view: "computed",
      type,
      value,
    };
  }

  #createPropertyViews() {
    if (this.#createViewsPromise) {
      return this.#createViewsPromise;
    }

    this.refreshSourceFilter();
    this.numVisibleProperties = 0;
    const fragment = this.styleDocument.createDocumentFragment();

    this.#createViewsPromise = new Promise((resolve, reject) => {
      this.#createViewsProcess = new UpdateProcess(
        this.styleWindow,
        CssComputedView.propertyNames,
        {
          onItem: propertyName => {
            // Per-item callback.
            const propView = new PropertyView(this, propertyName);
            fragment.append(propView.createListItemElement());

            if (propView.visible) {
              this.numVisibleProperties++;
            }
            this.propertyViews.push(propView);
          },
          onCancel: () => {
            reject("#createPropertyViews cancelled");
          },
          onDone: () => {
            // Completed callback.
            this.element.appendChild(fragment);
            this.noResults.hidden = this.numVisibleProperties > 0;
            resolve(undefined);
          },
        }
      );
    });

    this.#createViewsProcess.schedule();

    return this.#createViewsPromise;
  }

  isPanelVisible() {
    return (
      this.inspector.toolbox &&
      this.inspector.sidebar &&
      this.inspector.toolbox.currentToolId === "inspector" &&
      this.inspector.sidebar.getCurrentTabID() == "computedview"
    );
  }

  /**
   * Refresh the panel content. This could be called by a "ruleview-changed" event, but
   * we avoid the extra processing unless the panel is visible.
   */
  async refreshPanel() {
    if (!this.#viewedElement || !this.isPanelVisible()) {
      return;
    }

    // Capture the current viewed element to return from the promise handler
    // early if it changed
    const viewedElement = this.#viewedElement;

    try {
      // Create the properties views only once for the whole lifecycle of the inspector
      // via `_createPropertyViews`.
      // The properties are created without backend data. This queries typical property
      // names via `DOMWindow.getComputedStyle` on the frontend inspector document.
      // We then have to manually update the list of PropertyView's for custom properties
      // based on backend data (`getComputed()`/`computed`).
      // Also note that PropertyView/PropertyView are refreshed via their refresh method
      // which will ultimately query `CssComputedView._computed`, which we update in this method.
      const [computed] = await Promise.all([
        this.viewedElementPageStyle.getComputed(this.#viewedElement, {
          filter: this.#sourceFilter,
          onlyMatched: !this.includeBrowserStyles,
          markMatched: true,
        }),
        this.#createPropertyViews(),
      ]);

      if (viewedElement !== this.#viewedElement) {
        return;
      }

      this.#computed = computed;
      this.#matchedProperties = new Set();
      const customProperties = new Set();

      for (const name in computed) {
        if (computed[name].matched) {
          this.#matchedProperties.add(name);
        }
        if (name.startsWith("--")) {
          customProperties.add(name);
        }
      }

      // Removing custom property PropertyViews which won't be used
      let customPropertiesStartIndex;
      for (let i = this.propertyViews.length - 1; i >= 0; i--) {
        const propView = this.propertyViews[i];

        // custom properties are displayed at the bottom of the list, and we're looping
        // backward through propertyViews, so if the current item does not represent
        // a custom property, we can stop looping.
        if (!propView.isCustomProperty) {
          customPropertiesStartIndex = i + 1;
          break;
        }

        // If the custom property will be used, move to the next item.
        if (customProperties.has(propView.name)) {
          customProperties.delete(propView.name);
          continue;
        }

        // Otherwise remove property view element
        if (propView.element) {
          propView.element.remove();
        }

        propView.destroy();
        this.propertyViews.splice(i, 1);
      }

      // At this point, `customProperties` only contains custom property names for
      // which we don't have a PropertyView yet.
      let insertIndex = customPropertiesStartIndex;
      for (const customPropertyName of Array.from(customProperties).sort()) {
        const propertyView = new PropertyView(
          this,
          customPropertyName,
          // isCustomProperty
          true
        );

        const len = this.propertyViews.length;
        if (insertIndex !== len) {
          for (let i = insertIndex; i <= len; i++) {
            const existingPropView = this.propertyViews[i];
            if (
              !existingPropView ||
              !existingPropView.isCustomProperty ||
              customPropertyName < existingPropView.name
            ) {
              insertIndex = i;
              break;
            }
          }
        }
        this.propertyViews.splice(insertIndex, 0, propertyView);

        // Insert the custom property PropertyView at the right spot so we
        // keep the list ordered.
        const previousSibling = this.element.childNodes[insertIndex - 1];
        previousSibling.insertAdjacentElement(
          "afterend",
          propertyView.createListItemElement()
        );
      }

      if (this.#refreshProcess) {
        this.#refreshProcess.cancel();
      }

      this.noResults.hidden = true;

      // Reset visible property count
      this.numVisibleProperties = 0;

      await new Promise((resolve, reject) => {
        this.#refreshProcess = new UpdateProcess(
          this.styleWindow,
          this.propertyViews,
          {
            onItem: propView => {
              propView.refresh();
            },
            onCancel: () => {
              reject("#refreshProcess of computed view cancelled");
            },
            onDone: () => {
              this.#refreshProcess = null;
              this.noResults.hidden = this.numVisibleProperties > 0;

              const searchBox = this.searchField.parentNode;
              searchBox.classList.toggle(
                "devtools-searchbox-no-match",
                !!this.searchField.value.length && !this.numVisibleProperties
              );

              this.inspector.emit("computed-view-refreshed");
              resolve(undefined);
            },
          }
        );
        this.#refreshProcess.schedule();
      });
    } catch (e) {
      console.error(e);
    }
  }

  /**
   * Handle the shortcut events in the computed view.
   */
  #onShortcut = (name, event) => {
    if (!event.target.closest("#sidebar-panel-computedview")) {
      return;
    }
    // Handle the search box's keypress event. If the escape key is pressed,
    // clear the search box field.
    if (
      name === "Escape" &&
      event.target === this.searchField &&
      this.#onClearSearch()
    ) {
      event.preventDefault();
      event.stopPropagation();
    } else if (name === "CmdOrCtrl+F") {
      this.searchField.focus();
      event.preventDefault();
    }
  };

  /**
   * Set the filter style search value.
   * @param {String} value
   *        The search value.
   */
  setFilterStyles(value = "") {
    this.searchField.value = value;
    this.searchField.focus();
    this.#onFilterStyles();
  }

  /**
   * Called when the user enters a search term in the filter style search box.
   */
  #onFilterStyles = () => {
    if (this.#filterChangedTimeout) {
      clearTimeout(this.#filterChangedTimeout);
    }

    const filterTimeout = this.searchField.value.length
      ? FILTER_CHANGED_TIMEOUT
      : 0;
    this.searchClearButton.hidden = this.searchField.value.length === 0;

    this.#filterChangedTimeout = setTimeout(() => {
      this.refreshPanel();
      this.#filterChangedTimeout = null;
    }, filterTimeout);
  };

  /**
   * Called when the user clicks on the clear button in the filter style search
   * box. Returns true if the search box is cleared and false otherwise.
   */
  #onClearSearch = () => {
    if (this.searchField.value) {
      this.setFilterStyles("");
      return true;
    }

    return false;
  };

  /**
   * The change event handler for the includeBrowserStyles checkbox.
   */
  #onIncludeBrowserStyles = () => {
    this.refreshSourceFilter();
    this.refreshPanel();
  };

  /**
   * When includeBrowserStylesCheckbox.checked is false we only display
   * properties that have matched selectors and have been included by the
   * document or one of thedocument's stylesheets. If .checked is false we
   * display all properties including those that come from UA stylesheets.
   */
  refreshSourceFilter() {
    this.#matchedProperties = null;
    this.#sourceFilter = this.includeBrowserStyles
      ? CssLogic.FILTER.UA
      : CssLogic.FILTER.USER;
  }

  /**
   * The CSS as displayed by the UI.
   */
  createStyleViews() {
    if (CssComputedView.propertyNames) {
      return;
    }

    CssComputedView.propertyNames = [];

    // Here we build and cache a list of css properties supported by the browser
    // We could use any element but let's use the main document's root element
    const styles = this.styleWindow.getComputedStyle(
      this.styleDocument.documentElement
    );
    const mozProps = [];
    for (let i = 0, numStyles = styles.length; i < numStyles; i++) {
      const prop = styles.item(i);
      if (prop.startsWith("--")) {
        // Skip any CSS variables used inside of browser CSS files
        continue;
      } else if (prop.startsWith("-")) {
        mozProps.push(prop);
      } else {
        CssComputedView.propertyNames.push(prop);
      }
    }

    CssComputedView.propertyNames.sort();
    CssComputedView.propertyNames.push.apply(
      CssComputedView.propertyNames,
      mozProps.sort()
    );

    this.#createPropertyViews().catch(e => {
      if (!this.#isDestroyed) {
        console.warn(
          "The creation of property views was cancelled because " +
            "the computed-view was destroyed before it was done creating views"
        );
      } else {
        console.error(e);
      }
    });
  }

  /**
   * Get a set of properties that have matched selectors.
   *
   * @return {Set} If a property name is in the set, it has matching selectors.
   */
  get matchedProperties() {
    return this.#matchedProperties || new Set();
  }

  /**
   * Focus the window on mousedown.
   */
  focusWindow() {
    this.styleWindow.focus();
  }

  /**
   * Context menu handler.
   */
  #onContextMenu = event => {
    // Call stopPropagation() and preventDefault() here so that avoid to show default
    // context menu in about:devtools-toolbox. See Bug 1515265.
    event.stopPropagation();
    event.preventDefault();
    this.contextMenu.show(event);
  };

  #onClick = event => {
    const target = event.target;

    if (target.nodeName === "a") {
      event.stopPropagation();
      event.preventDefault();
      openContentLink(target.href);
    }
  };

  /**
   * Callback for copy event. Copy selected text.
   *
   * @param {Event} event
   *        copy event object.
   */
  #onCopy = event => {
    const win = this.styleWindow;
    const text = win.getSelection().toString().trim();
    if (text !== "") {
      this.copySelection();
      event.preventDefault();
    }
  };

  /**
   * Copy the current selection to the clipboard
   */
  copySelection() {
    try {
      const win = this.styleWindow;
      const text = win.getSelection().toString().trim();

      clipboardHelper.copyString(text);
    } catch (e) {
      console.error(e);
    }
  }

  /**
   * Destructor for CssComputedView.
   */
  destroy() {
    this.#viewedElement = null;
    this.#abortController.abort();
    this.#abortController = null;

    if (this.viewedElementPageStyle) {
      this.viewedElementPageStyle = null;
    }
    this.#outputParser = null;

    this.#prefObserver.destroy();

    // Cancel tree construction
    if (this.#createViewsProcess) {
      this.#createViewsProcess.cancel();
    }
    if (this.#refreshProcess) {
      this.#refreshProcess.cancel();
    }

    if (this.#contextMenu) {
      this.#contextMenu.destroy();
      this.#contextMenu = null;
    }

    if (this.#highlighters) {
      this.#highlighters.removeFromView(this);
      this.#highlighters = null;
    }

    this.tooltips.destroy();

    // Nodes used in templating
    this.element = null;
    this.searchField = null;
    this.searchClearButton = null;
    this.includeBrowserStylesCheckbox = null;

    // Property views
    for (const propView of this.propertyViews) {
      propView.destroy();
    }
    this.propertyViews = null;

    this.inspector = null;
    this.styleDocument = null;
    this.styleWindow = null;

    this.#isDestroyed = true;
  }
}

class PropertyInfo {
  /*
   * @param {CssComputedView} tree
   *        The CssComputedView instance we are working with.
   * @param {String} name
   *        The CSS property name
   */
  constructor(tree, name) {
    this.#tree = tree;
    this.name = name;
  }

  #tree;

  get isSupported() {
    // There can be a mismatch between the list of properties
    // supported on the server and on the client.
    // Ideally we should build PropertyInfo only for property names supported on
    // the server. See Bug 1722348.
    return this.#tree.computed && this.name in this.#tree.computed;
  }

  get value() {
    if (this.isSupported) {
      const value = this.#tree.computed[this.name].value;
      return value;
    }
    return null;
  }
}

/**
 * A container to give easy access to property data from the template engine.
 */
class PropertyView {
  /*
   * @param {CssComputedView} tree
   *        The CssComputedView instance we are working with.
   * @param {String} name
   *        The CSS property name for which this PropertyView
   *        instance will render the rules.
   * @param {Boolean} isCustomProperty
   *        Set to true if this will represent a custom property.
   */
  constructor(tree, name, isCustomProperty = false) {
    this.#tree = tree;
    this.name = name;

    this.isCustomProperty = isCustomProperty;

    if (!this.isCustomProperty) {
      this.link = "https://developer.mozilla.org/docs/Web/CSS/" + name;
    }

    this.#propertyInfo = new PropertyInfo(tree, name);
    const win = this.#tree.styleWindow;
    this.#abortController = new win.AbortController();
  }

  // The parent element which contains the open attribute
  element = null;

  // Destination for property values
  valueNode = null;

  // Are matched rules expanded?
  matchedExpanded = false;

  // Matched selector container
  matchedSelectorsContainer = null;

  // Result of call to getMatchedSelectors
  #matchedSelectorResponse = null;

  // Matched selector expando
  #matchedExpander = null;

  // AbortController for event listeners
  #abortController = null;

  // Cache for matched selector views
  #matchedSelectorViews = null;

  // The previously selected element used for the selector view caches
  #prevViewedElement = null;

  // PropertyInfo
  #propertyInfo = null;

  #tree;

  /**
   * Get the computed style for the current property.
   *
   * @return {String} the computed style for the current property of the
   * currently highlighted element.
   */
  get value() {
    return this.propertyInfo.value;
  }

  /**
   * An easy way to access the CssPropertyInfo behind this PropertyView.
   */
  get propertyInfo() {
    return this.#propertyInfo;
  }

  /**
   * Does the property have any matched selectors?
   */
  get hasMatchedSelectors() {
    return this.#tree.matchedProperties.has(this.name);
  }

  /**
   * Should this property be visible?
   */
  get visible() {
    if (!this.#tree.viewedElement) {
      return false;
    }

    if (!this.#tree.includeBrowserStyles && !this.hasMatchedSelectors) {
      return false;
    }

    const searchTerm = this.#tree.searchField.value.toLowerCase();
    const isValidSearchTerm = !!searchTerm.trim().length;
    if (
      isValidSearchTerm &&
      !this.name.toLowerCase().includes(searchTerm) &&
      !this.value.toLowerCase().includes(searchTerm)
    ) {
      return false;
    }

    return this.propertyInfo.isSupported;
  }

  /**
   * Returns the className that should be assigned to the propertyView.
   *
   * @return {String}
   */
  get propertyHeaderClassName() {
    return this.visible ? "computed-property-view" : "computed-property-hidden";
  }

  /**
   * Is the property invalid at computed value time
   *
   * @returns {Boolean}
   */
  get invalidAtComputedValueTime() {
    return this.#tree.computed[this.name].invalidAtComputedValueTime;
  }

  /**
   * If this is a registered property, returns its syntax (e.g. "<color>")
   *
   * @returns {Text|undefined}
   */
  get registeredPropertySyntax() {
    return this.#tree.computed[this.name].registeredPropertySyntax;
  }

  /**
   * If this is a registered property, return its initial-value
   *
   * @returns {Text|undefined}
   */
  get registeredPropertyInitialValue() {
    return this.#tree.computed[this.name].registeredPropertyInitialValue;
  }

  /**
   * Create DOM elements for a property
   *
   * @return {Element} The <li> element
   */
  createListItemElement() {
    const doc = this.#tree.styleDocument;
    const baseEventListenerConfig = { signal: this.#abortController.signal };

    // Build the container element
    this.onMatchedToggle = this.onMatchedToggle.bind(this);
    this.element = doc.createElement("li");
    this.element.className = this.propertyHeaderClassName;
    this.element.addEventListener(
      "dblclick",
      this.onMatchedToggle,
      baseEventListenerConfig
    );

    // Make it keyboard navigable
    this.element.setAttribute("tabindex", "0");
    this.shortcuts = new KeyShortcuts({
      window: this.#tree.styleWindow,
      target: this.element,
    });
    this.shortcuts.on("F1", event => {
      this.mdnLinkClick(event);
      // Prevent opening the options panel
      event.preventDefault();
      event.stopPropagation();
    });
    this.shortcuts.on("Return", this.onMatchedToggle);
    this.shortcuts.on("Space", this.onMatchedToggle);

    const nameContainer = doc.createElement("span");
    nameContainer.className = "computed-property-name-container";

    // Build the twisty expand/collapse
    this.#matchedExpander = doc.createElement("div");
    this.#matchedExpander.className = "computed-expander theme-twisty";
    this.#matchedExpander.setAttribute("role", "button");
    this.#matchedExpander.setAttribute("aria-label", L10N_TWISTY_EXPAND_LABEL);
    this.#matchedExpander.addEventListener(
      "click",
      this.onMatchedToggle,
      baseEventListenerConfig
    );

    // Build the style name element
    const nameNode = doc.createElement("span");
    nameNode.classList.add("computed-property-name", "theme-fg-color3");

    // Give it a heading role for screen readers.
    nameNode.setAttribute("role", "heading");

    // Reset its tabindex attribute otherwise, if an ellipsis is applied
    // it will be reachable via TABing
    nameNode.setAttribute("tabindex", "");
    // Avoid english text (css properties) from being altered
    // by RTL mode
    nameNode.setAttribute("dir", "ltr");
    nameNode.textContent = nameNode.title = this.name;
    // Make it hand over the focus to the container
    const focusElement = () => this.element.focus();
    nameNode.addEventListener("click", focusElement, baseEventListenerConfig);

    // Build the style name ":" separator
    const nameSeparator = doc.createElement("span");
    nameSeparator.classList.add("visually-hidden");
    nameSeparator.textContent = ": ";
    nameNode.appendChild(nameSeparator);

    nameContainer.appendChild(nameNode);

    const valueContainer = doc.createElement("span");
    valueContainer.className = "computed-property-value-container";

    // Build the style value element
    this.valueNode = doc.createElement("span");
    this.valueNode.classList.add("computed-property-value", "theme-fg-color1");
    // Reset its tabindex attribute otherwise, if an ellipsis is applied
    // it will be reachable via TABing
    this.valueNode.setAttribute("tabindex", "");
    this.valueNode.setAttribute("dir", "ltr");
    // Make it hand over the focus to the container
    this.valueNode.addEventListener(
      "click",
      focusElement,
      baseEventListenerConfig
    );

    // Build the style value ";" separator
    const valueSeparator = doc.createElement("span");
    valueSeparator.classList.add("visually-hidden");
    valueSeparator.textContent = ";";

    valueContainer.append(this.valueNode, valueSeparator);

    // If the value is invalid at computed value time (IACVT), display the same
    // warning icon that we have in the rules view for IACVT declarations.
    if (this.isCustomProperty) {
      this.invalidAtComputedValueTimeNode = doc.createElement("div");
      this.invalidAtComputedValueTimeNode.classList.add(
        "invalid-at-computed-value-time-warning"
      );
      this.refreshInvalidAtComputedValueTime();
      valueContainer.append(this.invalidAtComputedValueTimeNode);
    }

    // Build the matched selectors container
    this.matchedSelectorsContainer = doc.createElement("div");
    this.matchedSelectorsContainer.classList.add("matchedselectors");

    this.element.append(
      this.#matchedExpander,
      nameContainer,
      valueContainer,
      this.matchedSelectorsContainer
    );

    return this.element;
  }

  /**
   * Refresh the panel's CSS property value.
   */
  refresh() {
    const className = this.propertyHeaderClassName;
    if (this.element.className !== className) {
      this.element.className = className;
    }

    if (this.#prevViewedElement !== this.#tree.viewedElement) {
      this.#matchedSelectorViews = null;
      this.#prevViewedElement = this.#tree.viewedElement;
    }

    if (!this.#tree.viewedElement || !this.visible) {
      this.valueNode.textContent = this.valueNode.title = "";
      this.matchedSelectorsContainer.parentNode.hidden = true;
      this.matchedSelectorsContainer.textContent = "";
      this.#matchedExpander.removeAttribute("open");
      this.#matchedExpander.setAttribute(
        "aria-label",
        L10N_TWISTY_EXPAND_LABEL
      );
      return;
    }

    this.#tree.numVisibleProperties++;

    this.valueNode.innerHTML = "";
    // No need to pass the baseURI argument here as computed URIs are never relative.
    this.valueNode.appendChild(this.#parseValue(this.propertyInfo.value));

    this.refreshInvalidAtComputedValueTime();
    this.refreshMatchedSelectors();
  }

  /**
   * Refresh the panel matched rules.
   */
  refreshMatchedSelectors() {
    const hasMatchedSelectors = this.hasMatchedSelectors;
    this.matchedSelectorsContainer.parentNode.hidden = !hasMatchedSelectors;

    if (hasMatchedSelectors) {
      this.#matchedExpander.classList.add("computed-expandable");
    } else {
      this.#matchedExpander.classList.remove("computed-expandable");
    }

    if (this.matchedExpanded && hasMatchedSelectors) {
      return this.#tree.viewedElementPageStyle
        .getMatchedSelectors(this.#tree.viewedElement, this.name)
        .then(matched => {
          if (!this.matchedExpanded) {
            return;
          }

          this.#matchedSelectorResponse = matched;

          this.#buildMatchedSelectors();
          this.#matchedExpander.setAttribute("open", "");
          this.#matchedExpander.setAttribute(
            "aria-label",
            L10N_TWISTY_COLLAPSE_LABEL
          );
          this.#tree.inspector.emit("computed-view-property-expanded");
        })
        .catch(console.error);
    }

    this.matchedSelectorsContainer.innerHTML = "";
    this.#matchedExpander.removeAttribute("open");
    this.#matchedExpander.setAttribute("aria-label", L10N_TWISTY_EXPAND_LABEL);
    this.#tree.inspector.emit("computed-view-property-collapsed");
    return Promise.resolve(undefined);
  }

  /**
   * Show/Hide IACVT icon and sets its title attribute
   */
  refreshInvalidAtComputedValueTime() {
    if (!this.isCustomProperty) {
      return;
    }

    if (!this.invalidAtComputedValueTime) {
      this.invalidAtComputedValueTimeNode.setAttribute("hidden", "");
      this.invalidAtComputedValueTimeNode.removeAttribute("title");
    } else {
      this.invalidAtComputedValueTimeNode.removeAttribute("hidden", "");
      this.invalidAtComputedValueTimeNode.setAttribute(
        "title",
        STYLE_INSPECTOR_L10N.getFormatStr(
          "rule.warningInvalidAtComputedValueTime.title",
          `"${this.registeredPropertySyntax}"`
        )
      );
    }
  }

  get matchedSelectors() {
    return this.#matchedSelectorResponse;
  }

  #buildMatchedSelectors() {
    const frag = this.element.ownerDocument.createDocumentFragment();

    for (const selector of this.matchedSelectorViews) {
      const p = createChild(frag, "p");
      const span = createChild(p, "span", {
        class: "rule-link",
      });

      const link = createChild(span, "a", {
        target: "_blank",
        class: "computed-link theme-link",
        title: selector.longSource,
        sourcelocation: selector.source,
        tabindex: "0",
        textContent: selector.source,
      });
      link.addEventListener("click", selector.openStyleEditor);
      const shortcuts = new KeyShortcuts({
        window: this.#tree.styleWindow,
        target: link,
      });
      shortcuts.on("Return", () => selector.openStyleEditor());

      const status = createChild(p, "span", {
        dir: "ltr",
        class: "rule-text theme-fg-color3 " + selector.statusClass,
        title: selector.statusText,
      });

      // Add an explicit status text span for screen readers.
      // They won't pick up the title from the status span.
      createChild(status, "span", {
        dir: "ltr",
        class: "visually-hidden",
        textContent: selector.statusText + " ",
      });

      createChild(status, "div", {
        class: "fix-get-selection",
        textContent: selector.sourceText,
      });

      const valueDiv = createChild(status, "div", {
        class:
          "fix-get-selection computed-other-property-value theme-fg-color1",
      });
      valueDiv.appendChild(
        this.#parseValue(
          selector.selectorInfo.value,
          selector.selectorInfo.rule.href
        )
      );

      // If the value is invalid at computed value time (IACVT), display the same
      // warning icon that we have in the rules view for IACVT declarations.
      if (selector.selectorInfo.invalidAtComputedValueTime) {
        createChild(status, "div", {
          class: "invalid-at-computed-value-time-warning",
          title: STYLE_INSPECTOR_L10N.getFormatStr(
            "rule.warningInvalidAtComputedValueTime.title",
            `"${selector.selectorInfo.registeredPropertySyntax}"`
          ),
        });
      }
    }

    if (this.registeredPropertyInitialValue !== undefined) {
      const p = createChild(frag, "p");
      const status = createChild(p, "span", {
        dir: "ltr",
        class: "rule-text theme-fg-color3",
      });

      createChild(status, "div", {
        class: "fix-get-selection",
        textContent: "initial-value",
      });

      const valueDiv = createChild(status, "div", {
        class:
          "fix-get-selection computed-other-property-value theme-fg-color1",
      });
      valueDiv.appendChild(
        this.#parseValue(this.registeredPropertyInitialValue)
      );
    }

    this.matchedSelectorsContainer.innerHTML = "";
    this.matchedSelectorsContainer.appendChild(frag);
  }

  /**
   * Parse a property value using the OutputParser.
   *
   * @param {String} value
   * @param {String} baseURI
   * @returns {DocumentFragment|Element}
   */
  #parseValue(value, baseURI) {
    if (this.isCustomProperty && value === "") {
      const doc = this.#tree.styleDocument;
      const el = doc.createElement("span");
      el.classList.add("empty-css-variable");
      el.append(doc.createTextNode(`<${L10N_EMPTY_VARIABLE}>`));
      return el;
    }

    // Sadly, because this fragment is added to the template by DOM Templater
    // we lose any events that are attached. This means that URLs will open in a
    // new window. At some point we should fix this by stopping using the
    // templater.
    return this.#tree.outputParser.parseCssProperty(this.name, value, {
      colorSwatchClass:
        "computed-colorswatch inspector-swatch inspector-colorswatch",
      colorClass: "computed-color",
      urlClass: "theme-link",
      fontFamilyClass: "computed-font-family",
      baseURI,
    });
  }

  /**
   * Provide access to the matched SelectorViews that we are currently
   * displaying.
   */
  get matchedSelectorViews() {
    if (!this.#matchedSelectorViews) {
      this.#matchedSelectorViews = [];
      this.#matchedSelectorResponse.forEach(selectorInfo => {
        const selectorView = new SelectorView(this.#tree, selectorInfo);
        this.#matchedSelectorViews.push(selectorView);
      }, this);
    }
    return this.#matchedSelectorViews;
  }

  /**
   * The action when a user expands matched selectors.
   *
   * @param {Event} event
   *        Used to determine the class name of the targets click
   *        event.
   */
  onMatchedToggle(event) {
    if (event.shiftKey) {
      return;
    }
    this.matchedExpanded = !this.matchedExpanded;
    this.refreshMatchedSelectors();
    event.preventDefault();
  }

  /**
   * The action when a user clicks on the MDN help link for a property.
   */
  mdnLinkClick() {
    if (!this.link) {
      return;
    }
    openContentLink(this.link);
  }

  /**
   * Destroy this property view, removing event listeners
   */
  destroy() {
    if (this.#matchedSelectorViews) {
      for (const view of this.#matchedSelectorViews) {
        view.destroy();
      }
    }

    if (this.#abortController) {
      this.#abortController.abort();
      this.#abortController = null;
    }

    if (this.shortcuts) {
      this.shortcuts.destroy();
    }

    this.shortcuts = null;
    this.element = null;
    this.#matchedExpander = null;
    this.valueNode = null;
  }
}

/**
 * A container to give us easy access to display data from a CssRule
 */
class SelectorView {
  /**
   * @param CssComputedView tree
   *        the owning CssComputedView
   * @param selectorInfo
   */
  constructor(tree, selectorInfo) {
    this.#tree = tree;
    this.selectorInfo = selectorInfo;
    this.#cacheStatusNames();

    this.openStyleEditor = this.openStyleEditor.bind(this);

    const rule = this.selectorInfo.rule;
    if (!rule || !rule.parentStyleSheet || rule.type == ELEMENT_STYLE) {
      this.source = CssLogic.l10n("rule.sourceElement");
      this.longSource = this.source;
    } else {
      // This always refers to the generated location.
      const sheet = rule.parentStyleSheet;
      const sourceSuffix = rule.line > 0 ? ":" + rule.line : "";
      this.source = CssLogic.shortSource(sheet) + sourceSuffix;
      this.longSource = CssLogic.longSource(sheet) + sourceSuffix;

      this.#generatedLocation = {
        sheet,
        href: sheet.href || sheet.nodeHref,
        line: rule.line,
        column: rule.column,
      };
      this.#unsubscribeCallback =
        this.#tree.inspector.toolbox.sourceMapURLService.subscribeByID(
          this.#generatedLocation.sheet.resourceId,
          this.#generatedLocation.line,
          this.#generatedLocation.column,
          this.#updateLocation
        );
    }
  }

  #generatedLocation;
  #href;
  #tree;
  #unsubscribeCallback;

  /**
   * Decode for cssInfo.rule.status
   * @see SelectorView.prototype.#cacheStatusNames
   * @see CssLogic.STATUS
   */
  static STATUS_NAMES = [
    // "Parent Match", "Matched", "Best Match"
  ];

  static CLASS_NAMES = ["parentmatch", "matched", "bestmatch"];

  /**
   * Cache localized status names.
   *
   * These statuses are localized inside the styleinspector.properties string
   * bundle.
   * @see css-logic.js - the CssLogic.STATUS array.
   */
  #cacheStatusNames() {
    if (SelectorView.STATUS_NAMES.length) {
      return;
    }

    for (const status in CssLogic.STATUS) {
      const i = CssLogic.STATUS[status];
      if (i > CssLogic.STATUS.UNMATCHED) {
        const value = CssComputedView.l10n("rule.status." + status);
        // Replace normal spaces with non-breaking spaces
        SelectorView.STATUS_NAMES[i] = value.replace(/ /g, "\u00A0");
      }
    }
  }

  /**
   * A localized version of cssRule.status
   */
  get statusText() {
    return SelectorView.STATUS_NAMES[this.selectorInfo.status];
  }

  /**
   * Get class name for selector depending on status
   */
  get statusClass() {
    return SelectorView.CLASS_NAMES[this.selectorInfo.status - 1];
  }

  get href() {
    if (this.#href) {
      return this.#href;
    }
    const sheet = this.selectorInfo.rule.parentStyleSheet;
    this.#href = sheet ? sheet.href : "#";
    return this.#href;
  }

  get sourceText() {
    return this.selectorInfo.sourceText;
  }

  get value() {
    return this.selectorInfo.value;
  }

  /**
   * Update the text of the source link to reflect whether we're showing
   * original sources or not.  This is a callback for
   * SourceMapURLService.subscribe, which see.
   *
   * @param {Object | null} originalLocation
   *        The original position object (url/line/column) or null.
   */
  #updateLocation = originalLocation => {
    if (!this.#tree.element) {
      return;
    }

    // Update |currentLocation| to be whichever location is being
    // displayed at the moment.
    let currentLocation = this.#generatedLocation;
    if (originalLocation) {
      const { url, line, column } = originalLocation;
      currentLocation = { href: url, line, column };
    }

    const selector = '[sourcelocation="' + this.source + '"]';
    const link = this.#tree.element.querySelector(selector);
    if (link) {
      const text =
        CssLogic.shortSource(currentLocation) + ":" + currentLocation.line;
      link.textContent = text;
    }

    this.#tree.inspector.emit("computed-view-sourcelinks-updated");
  };

  /**
   * When a css link is clicked this method is called in order to either:
   *   1. Open the link in view source (for chrome stylesheets).
   *   2. Open the link in the style editor.
   *
   *   We can only view stylesheets contained in document.styleSheets inside the
   *   style editor.
   */
  openStyleEditor() {
    const inspector = this.#tree.inspector;
    const rule = this.selectorInfo.rule;

    // The style editor can only display stylesheets coming from content because
    // chrome stylesheets are not listed in the editor's stylesheet selector.
    //
    // If the stylesheet is a content stylesheet we send it to the style
    // editor else we display it in the view source window.
    const parentStyleSheet = rule.parentStyleSheet;
    if (!parentStyleSheet || parentStyleSheet.isSystem) {
      inspector.toolbox.viewSource(rule.href, rule.line);
      return;
    }

    const { sheet, line, column } = this.#generatedLocation;
    if (ToolDefinitions.styleEditor.isToolSupported(inspector.toolbox)) {
      inspector.toolbox.viewSourceInStyleEditorByResource(sheet, line, column);
    }
  }

  /**
   * Destroy this selector view, removing event listeners
   */
  destroy() {
    if (this.#unsubscribeCallback) {
      this.#unsubscribeCallback();
    }
  }
}

class ComputedViewTool {
  /**
   * @param {Inspector} inspector
   * @param {Window} window
   */
  constructor(inspector, window) {
    this.inspector = inspector;
    this.document = window.document;

    this.computedView = new CssComputedView(this.inspector, this.document);

    this.onDetachedFront = this.onDetachedFront.bind(this);
    this.onSelected = this.onSelected.bind(this);
    this.refresh = this.refresh.bind(this);
    this.onPanelSelected = this.onPanelSelected.bind(this);

    this.#abortController = new AbortController();
    const opts = { signal: this.#abortController.signal };
    this.inspector.selection.on("detached-front", this.onDetachedFront, opts);
    this.inspector.selection.on("new-node-front", this.onSelected, opts);
    this.inspector.selection.on("pseudoclass", this.refresh, opts);
    this.inspector.sidebar.on(
      "computedview-selected",
      this.onPanelSelected,
      opts
    );
    this.inspector.styleChangeTracker.on("style-changed", this.refresh, opts);

    this.computedView.selectElement(null);

    this.onSelected();
  }

  #abortController;

  isPanelVisible() {
    if (!this.computedView) {
      return false;
    }
    return this.computedView.isPanelVisible();
  }

  onDetachedFront() {
    this.onSelected(false);
  }

  async onSelected(selectElement = true) {
    // Ignore the event if the view has been destroyed, or if it's inactive.
    // But only if the current selection isn't null. If it's been set to null,
    // let the update go through as this is needed to empty the view on
    // navigation.
    if (!this.computedView) {
      return;
    }

    const isInactive =
      !this.isPanelVisible() && this.inspector.selection.nodeFront;
    if (isInactive) {
      return;
    }

    if (
      !this.inspector.selection.isConnected() ||
      !this.inspector.selection.isElementNode()
    ) {
      this.computedView.selectElement(null);
      return;
    }

    if (selectElement) {
      const done = this.inspector.updating("computed-view");
      await this.computedView.selectElement(this.inspector.selection.nodeFront);
      done();
    }
  }

  refresh() {
    if (this.isPanelVisible()) {
      this.computedView.refreshPanel();
    }
  }

  onPanelSelected() {
    if (
      this.inspector.selection.nodeFront === this.computedView.viewedElement
    ) {
      this.refresh();
    } else {
      this.onSelected();
    }
  }

  destroy() {
    this.#abortController.abort();
    this.computedView.destroy();

    this.computedView =
      this.document =
      this.inspector =
      this.#abortController =
        null;
  }
}

exports.CssComputedView = CssComputedView;
exports.ComputedViewTool = ComputedViewTool;
exports.PropertyView = PropertyView;
