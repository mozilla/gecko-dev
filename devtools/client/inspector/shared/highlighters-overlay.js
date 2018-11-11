/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");
const EventEmitter = require("devtools/shared/event-emitter");
const {
  VIEW_NODE_VALUE_TYPE,
  VIEW_NODE_SHAPE_POINT_TYPE,
} = require("devtools/client/inspector/shared/node-types");

const DEFAULT_GRID_COLOR = "#4B0082";

/**
 * Highlighters overlay is a singleton managing all highlighters in the Inspector.
 */
class HighlightersOverlay {
  /**
   * @param  {Inspector} inspector
   *         Inspector toolbox panel.
   */
  constructor(inspector) {
    this.inspector = inspector;
    this.highlighterUtils = this.inspector.toolbox.highlighterUtils;
    this.store = this.inspector.store;
    this.telemetry = inspector.telemetry;
    this.maxGridHighlighters =
      Services.prefs.getIntPref("devtools.gridinspector.maxHighlighters");

    // Collection of instantiated highlighter actors like FlexboxHighlighter,
    // ShapesHighlighter and GeometryEditorHighlighter.
    this.highlighters = {};
    // Map of grid container NodeFront to their instantiated grid highlighter actors.
    this.gridHighlighters = new Map();
    // Array of reusable grid highlighters that have been instantiated and are not
    // associated with any NodeFront.
    this.extraGridHighlighterPool = [];

    // Collection of instantiated in-context editors, like ShapesInContextEditor, which
    // behave like highlighters but with added editing capabilities that need to map value
    // changes to properties in the Rule view.
    this.editors = {};

    // Saved state to be restore on page navigation.
    this.state = {
      flexbox: {},
      // Map of grid container NodeFront to the their stored grid options
      grids: new Map(),
      shapes: {},
    };

    // NodeFront of the flexbox container that is highlighted.
    this.flexboxHighlighterShown = null;
    // NodeFront of the flex item that is highlighted.
    this.flexItemHighlighterShown = null;
    // NodeFront of element that is highlighted by the geometry editor.
    this.geometryEditorHighlighterShown = null;
    // Name of the highlighter shown on mouse hover.
    this.hoveredHighlighterShown = null;
    // Name of the selector highlighter shown.
    this.selectorHighlighterShown = null;
    // NodeFront of the shape that is highlighted
    this.shapesHighlighterShown = null;

    this.onClick = this.onClick.bind(this);
    this.onMarkupMutation = this.onMarkupMutation.bind(this);
    this.onMouseMove = this.onMouseMove.bind(this);
    this.onMouseOut = this.onMouseOut.bind(this);
    this.onWillNavigate = this.onWillNavigate.bind(this);
    this.hideFlexboxHighlighter = this.hideFlexboxHighlighter.bind(this);
    this.hideFlexItemHighlighter = this.hideFlexItemHighlighter.bind(this);
    this.hideGridHighlighter = this.hideGridHighlighter.bind(this);
    this.hideShapesHighlighter = this.hideShapesHighlighter.bind(this);
    this.showFlexboxHighlighter = this.showFlexboxHighlighter.bind(this);
    this.showFlexItemHighlighter = this.showFlexItemHighlighter.bind(this);
    this.showGridHighlighter = this.showGridHighlighter.bind(this);
    this.showShapesHighlighter = this.showShapesHighlighter.bind(this);
    this._handleRejection = this._handleRejection.bind(this);
    this.onShapesHighlighterShown = this.onShapesHighlighterShown.bind(this);
    this.onShapesHighlighterHidden = this.onShapesHighlighterHidden.bind(this);

    // Add inspector events, not specific to a given view.
    this.inspector.on("markupmutation", this.onMarkupMutation);
    this.inspector.target.on("will-navigate", this.onWillNavigate);

    EventEmitter.decorate(this);
  }

  /**
   * Returns true if the grid highlighter can be toggled on/off for the given node, and
   * false otherwise. A grid container can be toggled on if the max grid highlighters
   * is only 1 or less than the maximum grid highlighters that can be displayed or if
   * the grid highlighter already highlights the given node.
   *
   * @param  {NodeFront} node
   *         Grid container NodeFront.
   * @return {Boolean}
   */
  canGridHighlighterToggle(node) {
    const maxGridHighlighters =
      Services.prefs.getIntPref("devtools.gridinspector.maxHighlighters");
    return maxGridHighlighters === 1 ||
           this.gridHighlighters.size < maxGridHighlighters ||
           this.gridHighlighters.has(node);
  }

  /**
   * Returns whether `node` is somewhere inside the DOM of the rule view.
   *
   * @param {DOMNode} node
   * @return {Boolean}
   */
  isRuleView(node) {
    return !!node.closest("#ruleview-panel");
  }

  /**
   * Add the highlighters overlay to the view. This will start tracking mouse events
   * and display highlighters when needed.
   *
   * @param  {CssRuleView|CssComputedView|LayoutView} view
   *         Either the rule-view or computed-view panel to add the highlighters overlay.
   */
  addToView(view) {
    const el = view.element;
    el.addEventListener("click", this.onClick, true);
    el.addEventListener("mousemove", this.onMouseMove);
    el.addEventListener("mouseout", this.onMouseOut);
    el.ownerDocument.defaultView.addEventListener("mouseout", this.onMouseOut);
  }

  /**
   * Remove the overlay from the given view. This will stop tracking mouse movement and
   * showing highlighters.
   *
   * @param  {CssRuleView|CssComputedView|LayoutView} view
   *         Either the rule-view or computed-view panel to remove the highlighters
   *         overlay.
   */
  removeFromView(view) {
    const el = view.element;
    el.removeEventListener("click", this.onClick, true);
    el.removeEventListener("mousemove", this.onMouseMove);
    el.removeEventListener("mouseout", this.onMouseOut);
  }

  /**
   * Toggle the shapes highlighter for the given node.
   *
   * @param  {NodeFront} node
   *         The NodeFront of the element with a shape to highlight.
   * @param  {Object} options
   *         Object used for passing options to the shapes highlighter.
   * @param {TextProperty} textProperty
   *        TextProperty where to write changes.
   */
  async toggleShapesHighlighter(node, options, textProperty) {
    const shapesEditor = await this.getInContextEditor("shapesEditor");
    if (!shapesEditor) {
      return;
    }
    shapesEditor.toggle(node, options, textProperty);
  }

  /**
   * Show the shapes highlighter for the given node.
   * This method delegates to the in-context shapes editor.
   *
   * @param  {NodeFront} node
   *         The NodeFront of the element with a shape to highlight.
   * @param  {Object} options
   *         Object used for passing options to the shapes highlighter.
   */
  async showShapesHighlighter(node, options) {
    const shapesEditor = await this.getInContextEditor("shapesEditor");
    if (!shapesEditor) {
      return;
    }
    shapesEditor.show(node, options);
  }

  /**
   * Called after the shape highlighter was shown.
   *
   * @param  {Object} data
   *         Data associated with the event.
   *         Contains:
   *         - {NodeFront} node: The NodeFront of the element that is highlighted.
   *         - {Object} options: Options that were passed to ShapesHighlighter.show()
   */
  onShapesHighlighterShown(data) {
    const { node, options } = data;
    this.shapesHighlighterShown = node;
    this.state.shapes.options = options;
    this.emit("shapes-highlighter-shown", node, options);
  }

  /**
   * Hide the shapes highlighter if visible.
   * This method delegates the to the in-context shapes editor which wraps
   * the shapes highlighter with additional functionality.
   */
  async hideShapesHighlighter() {
    const shapesEditor = await this.getInContextEditor("shapesEditor");
    if (!shapesEditor) {
      return;
    }
    shapesEditor.hide();
  }

  /**
   * Called after the shapes highlighter was hidden.
   *
   * @param  {Object} data
   *         Data associated with the event.
   *         Contains:
   *         - {NodeFront} node: The NodeFront of the element that was highlighted.
   */
  onShapesHighlighterHidden(data) {
    this.emit("shapes-highlighter-hidden", this.shapesHighlighterShown,
      this.state.shapes.options);
    this.shapesHighlighterShown = null;
    this.state.shapes = {};
  }

  /**
   * Show the shapes highlighter for the given element, with the given point highlighted.
   *
   * @param {NodeFront} node
   *        The NodeFront of the element to highlight.
   * @param {String} point
   *        The point to highlight in the shapes highlighter.
   */
  async hoverPointShapesHighlighter(node, point) {
    if (node == this.shapesHighlighterShown) {
      const options = Object.assign({}, this.state.shapes.options);
      options.hoverPoint = point;
      await this.showShapesHighlighter(node, options);
    }
  }

  /**
   * Create a flexbox highlighter settings object from the Redux store containing any
   * highlighter options that should be passed into the highlighter.
   */
  getFlexboxHighlighterSettings() {
    const { flexbox } = this.store.getState();
    const color = flexbox.color;
    return { color };
  }

  /**
   * Toggle the flexbox highlighter for the given flexbox container element.
   *
   * @param  {NodeFront} node
   *         The NodeFront of the flexbox container element to highlight.
   * @param  {Object} options
   *         Object used for passing options to the flexbox highlighter.
   */
  async toggleFlexboxHighlighter(node, options = {}) {
    if (node == this.flexboxHighlighterShown) {
      await this.hideFlexboxHighlighter(node);
      return;
    }

    await this.showFlexboxHighlighter(node, options);
  }

  /**
   * Show the flexbox highlighter for the given flexbox container element.
   *
   * @param  {NodeFront} node
   *         The NodeFront of the flexbox container element to highlight.
   * @param  {Object} options
   *         Object used for passing options to the flexbox highlighter.
   */
  async showFlexboxHighlighter(node, options) {
    const highlighter = await this._getHighlighter("FlexboxHighlighter");
    if (!highlighter) {
      return;
    }

    options = Object.assign({}, options, this.getFlexboxHighlighterSettings(node));

    const isShown = await highlighter.show(node, options);
    if (!isShown) {
      return;
    }

    this._toggleRuleViewIcon(node, true, ".ruleview-flex");

    try {
      // Save flexbox highlighter state.
      const { url } = this.inspector.target;
      const selector = await node.getUniqueSelector();
      this.state.flexbox = { selector, options, url };
      this.flexboxHighlighterShown = node;

      // Emit the NodeFront of the flexbox container element that the flexbox highlighter
      // was shown for.
      this.emit("flexbox-highlighter-shown", node, options);
    } catch (e) {
      this._handleRejection(e);
    }
  }

  /**
   * Hide the flexbox highlighter for the given flexbox container element.
   *
   * @param  {NodeFront} node
   *         The NodeFront of the flexbox container element to unhighlight.
   */
  async hideFlexboxHighlighter(node) {
    if (!this.flexboxHighlighterShown || !this.highlighters.FlexboxHighlighter) {
      return;
    }

    this._toggleRuleViewIcon(node, false, ".ruleview-flex");

    await this.highlighters.FlexboxHighlighter.hide();

    // Emit the NodeFront of the flexbox container element that the flexbox highlighter
    // was hidden for.
    const nodeFront = this.flexboxHighlighterShown;
    this.flexboxHighlighterShown = null;
    this.emit("flexbox-highlighter-hidden", nodeFront);

    // Erase flexbox highlighter state.
    this.state.flexbox = null;
  }

  /**
   * Toggle the flex item highlighter for the given flex item element.
   *
   * @param  {NodeFront} node
   *         The NodeFront of the flex item element to highlight.
   * @param  {Object} options
   *         Object used for passing options to the flex item highlighter.
   */
  async toggleFlexItemHighlighter(node, options = {}) {
    if (node == this.flexItemHighlighterShown) {
      await this.hideFlexItemHighlighter(node);
      return;
    }

    await this.showFlexItemHighlighter(node, options);
  }

  /**
   * Show the flex item highlighter for the given flex item element.
   *
   * @param  {NodeFront} node
   *         The NodeFront of the flex item element to highlight.
   * @param  {Object} options
   *         Object used for passing options to the flex item highlighter.
   */
  async showFlexItemHighlighter(node, options) {
    const highlighter = await this._getHighlighter("FlexItemHighlighter");
    if (!highlighter) {
      return;
    }

    options = Object.assign({}, options, this.getFlexboxHighlighterSettings());

    const isShown = await highlighter.show(node, options);
    if (!isShown) {
      return;
    }

    this.flexItemHighlighterShown = node;
  }

  /**
   * Hide the flex item highlighter for the given flex item element.
   *
   * @param  {NodeFront} node
   *         The NodeFront of the flex item element to unhighlight.
   */
  async hideFlexItemHighlighter(node) {
    if (!this.flexItemHighlighterShown || !this.highlighters.FlexItemHighlighter) {
      return;
    }

    await this.highlighters.FlexItemHighlighter.hide();
    this.flexItemHighlighterShown = null;
  }

  /**
   * Create a grid highlighter settings object for the provided nodeFront.
   *
   * @param  {NodeFront} nodeFront
   *         The NodeFront for which we need highlighter settings.
   */
  getGridHighlighterSettings(nodeFront) {
    const { grids, highlighterSettings } = this.store.getState();
    const grid = grids.find(g => g.nodeFront === nodeFront);
    const color = grid ? grid.color : DEFAULT_GRID_COLOR;
    return Object.assign({}, highlighterSettings, { color });
  }

  /**
   * Toggle the grid highlighter for the given grid container element.
   *
   * @param  {NodeFront} node
   *         The NodeFront of the grid container element to highlight.
   * @param. {String|null} trigger
   *         String name matching "grid" or "rule" to indicate where the
   *         grid highlighter was toggled on from. "grid" represents the grid view
   *         "rule" represents the rule view.
   */
  async toggleGridHighlighter(node, trigger) {
    if (this.gridHighlighters.has(node)) {
      await this.hideGridHighlighter(node);
      return;
    }

    await this.showGridHighlighter(node, {}, trigger);
  }

  /**
   * Show the grid highlighter for the given grid container element.
   *
   * @param  {NodeFront} node
   *         The NodeFront of the grid container element to highlight.
   * @param  {Object} options
   *         Object used for passing options to the grid highlighter.
   * @param. {String|null} trigger
   *         String name matching "grid" or "rule" to indicate where the
   *         grid highlighter was toggled on from. "grid" represents the grid view
   *         "rule" represents the rule view.
   */
  async showGridHighlighter(node, options, trigger) {
    // When the grid highlighter has the given node, it is probably called with new
    // highlighting options, so skip any extra grid highlighter handling.
    if (!this.gridHighlighters.has(node)) {
      if (this.maxGridHighlighters === 1) {
        // Only one grid highlighter can be shown at a time. Hides any instantiated
        // grid highlighters.
        for (const nodeFront of this.gridHighlighters.keys()) {
          await this.hideGridHighlighter(nodeFront);
        }
      } else if (this.gridHighlighters.size === this.maxGridHighlighters) {
        // The maximum number of grid highlighters shown have been reached. Don't show
        // any additional grid highlighters.
        return;
      }
    }

    const highlighter = await this._getGridHighlighter(node);
    if (!highlighter) {
      return;
    }

    options = Object.assign({}, options, this.getGridHighlighterSettings(node));

    const isShown = await highlighter.show(node, options);
    if (!isShown) {
      return;
    }

    this._toggleRuleViewIcon(node, true, ".ruleview-grid");

    if (trigger == "grid") {
      this.telemetry.scalarAdd("devtools.grid.gridinspector.opened", 1);
    } else if (trigger == "rule") {
      this.telemetry.scalarAdd("devtools.rules.gridinspector.opened", 1);
    }

    try {
      // Save grid highlighter state.
      const { url } = this.inspector.target;
      const selector = await node.getUniqueSelector();
      this.state.grids.set(node, { selector, options, url });

      // Emit the NodeFront of the grid container element that the grid highlighter was
      // shown for, and its options for testing the highlighter setting options.
      this.emit("grid-highlighter-shown", node, options);
    } catch (e) {
      this._handleRejection(e);
    }
  }

  /**
   * Hide the grid highlighter for the given grid container element.
   *
   * @param  {NodeFront} node
   *         The NodeFront of the grid container element to unhighlight.
   */
  async hideGridHighlighter(node) {
    if (!this.gridHighlighters.has(node)) {
      return;
    }

    // Hide the highlighter and put it in the pool of extra grid highlighters
    // so that it can be reused.
    const highlighter = this.gridHighlighters.get(node);
    await highlighter.hide();
    this.extraGridHighlighterPool.push(highlighter);

    this.state.grids.delete(node);
    this.gridHighlighters.delete(node);

    this._toggleRuleViewIcon(node, false, ".ruleview-grid");

    // Emit the NodeFront of the grid container element that the grid highlighter was
    // hidden for.
    this.emit("grid-highlighter-hidden", node);
  }

  /**
   * Show the box model highlighter for the given node.
   *
   * @param  {NodeFront} node
   *         The NodeFront of the element to highlight.
   * @param  {Object} options
   *         Object used for passing options to the box model highlighter.
   */
  async showBoxModelHighlighter(node, options) {
    const highlighter = await this._getHighlighter("BoxModelHighlighter");
    if (!highlighter) {
      return;
    }

    const isShown = await highlighter.show(node, options);
    if (!isShown) {
      return;
    }

    this.boxModelHighlighterShown = node;
    this.emit("box-model-highlighter-shown", node);
  }

  /**
   * Hide the box model highlighter.
   */
  async hideBoxModelHighlighter() {
    if (!this.boxModelHighlighterShown || !this.highlighters.BoxModelHighlighter) {
      return;
    }

    await this.highlighters.BoxModelHighlighter.hide();
    const node = this.boxModelHighlighterShown;
    this.boxModelHighlighterShown = null;
    this.emit("box-model-highlighter-hidden", node);
  }

  /**
   * Toggle the geometry editor highlighter for the given element.
   *
   * @param {NodeFront} node
   *        The NodeFront of the element to highlight.
   */
  async toggleGeometryHighlighter(node) {
    if (node == this.geometryEditorHighlighterShown) {
      await this.hideGeometryEditor();
      return;
    }

    await this.showGeometryEditor(node);
  }

  /**
   * Show the geometry editor highlightor for the given element.
   *
   * @param {NodeFront} node
   *        THe NodeFront of the element to highlight.
   */
  async showGeometryEditor(node) {
    const highlighter = await this._getHighlighter("GeometryEditorHighlighter");
    if (!highlighter) {
      return;
    }

    const isShown = await highlighter.show(node);
    if (!isShown) {
      return;
    }

    this.emit("geometry-editor-highlighter-shown");
    this.geometryEditorHighlighterShown = node;
  }

  /**
   * Hide the geometry editor highlighter.
   */
  async hideGeometryEditor() {
    if (!this.geometryEditorHighlighterShown ||
        !this.highlighters.GeometryEditorHighlighter) {
      return;
    }

    await this.highlighters.GeometryEditorHighlighter.hide();

    this.emit("geometry-editor-highlighter-hidden");
    this.geometryEditorHighlighterShown = null;
  }

  /**
   * Restores the saved flexbox highlighter state.
   */
  async restoreFlexboxState() {
    try {
      await this.restoreState("flexbox", this.state.flexbox, this.showFlexboxHighlighter);
    } catch (e) {
      this._handleRejection(e);
    }
  }

  /**
   * Restores the saved grid highlighter state.
   */
  async restoreGridState() {
    // The NodeFronts that are used as the keys in the grid state Map are no longer in the
    // tree after a reload. To clean up the grid state, we create a copy of the values of
    // the grid state before restoring and clear it.
    const values = [...this.state.grids.values()];
    this.state.grids.clear();

    try {
      for (const gridState of values) {
        await this.restoreState("grid", gridState, this.showGridHighlighter);
      }
    } catch (e) {
      this._handleRejection(e);
    }
  }

  /**
   * Helper function called by restoreFlexboxState, restoreGridState.
   * Restores the saved highlighter state for the given highlighter
   * and their state.
   *
   * @param  {String} name
   *         The name of the highlighter to be restored
   * @param  {Object} state
   *         The state of the highlighter to be restored
   * @param  {Function} showFunction
   *         The function that shows the highlighter
   * @return {Promise} that resolves when the highlighter state was restored, and the
   *         expected highlighters are displayed.
   */
  async restoreState(name, state, showFunction) {
    const { selector, options, url } = state;

    if (!selector || url !== this.inspector.target.url) {
      // Bail out if no selector was saved, or if we are on a different page.
      this.emit(`${name}-state-restored`, { restored: false });
      return;
    }

    const walker = this.inspector.walker;
    const rootNode = await walker.getRootNode();
    const nodeFront = await walker.querySelector(rootNode, selector);

    if (nodeFront) {
      if (options.hoverPoint) {
        options.hoverPoint = null;
      }

      await showFunction(nodeFront, options);
      this.emit(`${name}-state-restored`, { restored: true });
    } else {
      this.emit(`${name}-state-restored`, { restored: false });
    }
  }

  /**
   * Get an instance of an in-context editor for the given type.
   *
   * In-context editors behave like highlighters but with added editing capabilities which
   * need to write value changes back to something, like to properties in the Rule view.
   * They typically exist in the context of the page, like the ShapesInContextEditor.
   *
   * @param  {String} type
   *         Type of in-context editor. Currently supported: "shapesEditor"
   * @return {Object|null}
   *         Reference to instance for given type of in-context editor or null.
   */
  async getInContextEditor(type) {
    if (this.editors[type]) {
      return this.editors[type];
    }

    let editor;

    switch (type) {
      case "shapesEditor":
        const highlighter = await this._getHighlighter("ShapesHighlighter");
        if (!highlighter) {
          return null;
        }
        const ShapesInContextEditor = require("devtools/client/shared/widgets/ShapesInContextEditor");

        editor = new ShapesInContextEditor(highlighter, this.inspector, this.state);
        editor.on("show", this.onShapesHighlighterShown);
        editor.on("hide", this.onShapesHighlighterHidden);
        break;
      default:
        throw new Error(`Unsupported in-context editor '${name}'`);
    }

    this.editors[type] = editor;

    return editor;
  }

  /**
   * Get a highlighter front given a type. It will only be initialized once.
   *
   * @param  {String} type
   *         The highlighter type. One of this.highlighters.
   * @return {Promise} that resolves to the highlighter
   */
  async _getHighlighter(type) {
    if (this.highlighters[type]) {
      return this.highlighters[type];
    }

    let highlighter;

    try {
      highlighter = await this.highlighterUtils.getHighlighterByType(type);
    } catch (e) {
      this._handleRejection(e);
    }

    if (!highlighter) {
      return null;
    }

    this.highlighters[type] = highlighter;
    return highlighter;
  }

  /**
   * Get a grid highlighter front for a given node.
   *
   * @param  {NodeFront} node
   *         The NodeFront of the grid container element to highlight.
   * @return {Promise} that resolves to the grid highlighter front.
   */
  async _getGridHighlighter(node) {
    if (this.gridHighlighters.has(node)) {
      return this.gridHighlighters.get(node);
    }

    let highlighter;

    // Attempt to use any grid highlighters already instantiated in the extra pool,
    // otherwise, initialize a new grid highlighter.
    if (this.extraGridHighlighterPool.length > 0) {
      highlighter = this.extraGridHighlighterPool.pop();
    } else {
      try {
        highlighter = await this.highlighterUtils.getHighlighterByType(
          "CssGridHighlighter");
      } catch (e) {
        this._handleRejection(e);
      }
    }

    if (!highlighter) {
      return null;
    }

    this.gridHighlighters.set(node, highlighter);
    return highlighter;
  }

  _handleRejection(error) {
    if (!this.destroyed) {
      console.error(error);
    }
  }

  /**
   * Toggle all the icons with the given selector in the rule view if the current
   * inspector selection is the highlighted node.
   *
   * @param  {NodeFront} node
   *         The NodeFront of the element with a shape to highlight.
   * @param  {Boolean} active
   *         Whether or not the shape icon should be active.
   * @param  {String} selector
   *         The selector of the rule view icon to toggle.
   */
  _toggleRuleViewIcon(node, active, selector) {
    const ruleViewEl = this.inspector.getPanel("ruleview").view.element;

    if (this.inspector.selection.nodeFront !== node) {
      if (selector === ".ruleview-grid") {
        for (const icon of ruleViewEl.querySelectorAll(selector)) {
          if (this.canGridHighlighterToggle(this.inspector.selection.nodeFront)) {
            icon.removeAttribute("disabled");
          } else {
            icon.setAttribute("disabled", true);
          }
        }
      }

      return;
    }

    for (const icon of ruleViewEl.querySelectorAll(selector)) {
      icon.classList.toggle("active", active);
    }
  }

  /**
   * Toggle the class "active" on the given shape point in the rule view if the current
   * inspector selection is highlighted by the shapes highlighter.
   *
   * @param {NodeFront} node
   *        The NodeFront of the shape point to toggle
   * @param {Boolean} active
   *        Whether the shape point should be active
   */
  _toggleShapePointActive(node, active) {
    if (this.inspector.selection.nodeFront != this.shapesHighlighterShown) {
      return;
    }

    node.classList.toggle("active", active);
  }

  /**
   * Hide the currently shown hovered highlighter.
   */
  _hideHoveredHighlighter() {
    if (!this.hoveredHighlighterShown ||
        !this.highlighters[this.hoveredHighlighterShown]) {
      return;
    }

    // For some reason, the call to highlighter.hide doesn't always return a
    // promise. This causes some tests to fail when trying to install a
    // rejection handler on the result of the call. To avoid this, check
    // whether the result is truthy before installing the handler.
    const onHidden = this.highlighters[this.hoveredHighlighterShown].hide();
    if (onHidden) {
      onHidden.catch(console.error);
    }

    this.hoveredHighlighterShown = null;
    this.emit("highlighter-hidden");
  }

  /**
   * Given a node front and a function that hides the given node's highlighter, hides
   * the highlighter if the node front is no longer in the DOM tree. This is called
   * from the "markupmutation" event handler.
   *
   * @param  {NodeFront} node
   *         The NodeFront of a highlighted DOM node.
   * @param  {Function} hideHighlighter
   *         The function that will hide the highlighter of the highlighted node.
   */
  async _hideHighlighterIfDeadNode(node, hideHighlighter) {
    if (!node) {
      return;
    }

    try {
      const isInTree = await this.inspector.walker.isInDOMTree(node);
      if (!isInTree) {
        hideHighlighter(node);
      }
    } catch (e) {
      this._handleRejection(e);
    }
  }

  /**
   * Is the current hovered node a css transform property value in the
   * computed-view.
   *
   * @param  {Object} nodeInfo
   * @return {Boolean}
   */
  _isComputedViewTransform(nodeInfo) {
    if (nodeInfo.view != "computed") {
      return false;
    }
    return nodeInfo.type === VIEW_NODE_VALUE_TYPE &&
           nodeInfo.value.property === "transform";
  }

  /**
   * Is the current clicked node a flex display property value in the
   * rule-view.
   *
   * @param  {DOMNode} node
   * @return {Boolean}
   */
  _isRuleViewDisplayFlex(node) {
    return this.isRuleView(node) && node.classList.contains("ruleview-flex");
  }

  /**
   * Is the current clicked node a grid display property value in the
   * rule-view.
   *
   * @param  {DOMNode} node
   * @return {Boolean}
   */
  _isRuleViewDisplayGrid(node) {
    return this.isRuleView(node) && node.classList.contains("ruleview-grid");
  }

  /**
   * Does the current clicked node have the shapes highlighter toggle in the
   * rule-view.
   *
   * @param  {DOMNode} node
   * @return {Boolean}
   */
  _isRuleViewShapeSwatch(node) {
    return this.isRuleView(node) && node.classList.contains("ruleview-shapeswatch");
  }

  /**
   * Is the current hovered node a css transform property value in the rule-view.
   *
   * @param  {Object} nodeInfo
   * @return {Boolean}
   */
  _isRuleViewTransform(nodeInfo) {
    if (nodeInfo.view != "rule") {
      return false;
    }
    const isTransform = nodeInfo.type === VIEW_NODE_VALUE_TYPE &&
                      nodeInfo.value.property === "transform";
    const isEnabled = nodeInfo.value.enabled &&
                    !nodeInfo.value.overridden &&
                    !nodeInfo.value.pseudoElement;
    return isTransform && isEnabled;
  }

  /**
   * Is the current hovered node a highlightable shape point in the rule-view.
   *
   * @param  {Object} nodeInfo
   * @return {Boolean}
   */
  isRuleViewShapePoint(nodeInfo) {
    if (nodeInfo.view != "rule") {
      return false;
    }
    const isShape = nodeInfo.type === VIEW_NODE_SHAPE_POINT_TYPE &&
                  (nodeInfo.value.property === "clip-path" ||
                  nodeInfo.value.property === "shape-outside");
    const isEnabled = nodeInfo.value.enabled &&
                    !nodeInfo.value.overridden &&
                    !nodeInfo.value.pseudoElement;
    return isShape && isEnabled && nodeInfo.value.toggleActive &&
           !this.state.shapes.options.transformMode;
  }

  onClick(event) {
    if (this._isRuleViewDisplayGrid(event.target)) {
      event.stopPropagation();
      this.toggleGridHighlighter(this.inspector.selection.nodeFront, "rule");
    }

    if (this._isRuleViewDisplayFlex(event.target)) {
      event.stopPropagation();
      this.toggleFlexboxHighlighter(this.inspector.selection.nodeFront);
    }

    if (this._isRuleViewShapeSwatch(event.target)) {
      event.stopPropagation();

      const view = this.inspector.getPanel("ruleview").view;
      const nodeInfo = view.getNodeInfo(event.target);

      this.toggleShapesHighlighter(this.inspector.selection.nodeFront, {
        mode: event.target.dataset.mode,
        transformMode: event.metaKey || event.ctrlKey,
      }, nodeInfo.value.textProperty);
    }
  }

  onMouseMove(event) {
    // Bail out if the target is the same as for the last mousemove.
    if (event.target === this._lastHovered) {
      return;
    }

    // Only one highlighter can be displayed at a time, hide the currently shown.
    this._hideHoveredHighlighter();

    this._lastHovered = event.target;

    const view = this.isRuleView(this._lastHovered) ?
      this.inspector.getPanel("ruleview").view :
      this.inspector.getPanel("computedview").computedView;
    const nodeInfo = view.getNodeInfo(event.target);
    if (!nodeInfo) {
      return;
    }

    if (this.isRuleViewShapePoint(nodeInfo)) {
      const { point } = nodeInfo.value;
      this.hoverPointShapesHighlighter(this.inspector.selection.nodeFront, point);
      return;
    }

    // Choose the type of highlighter required for the hovered node.
    let type;
    if (this._isRuleViewTransform(nodeInfo) ||
        this._isComputedViewTransform(nodeInfo)) {
      type = "CssTransformHighlighter";
    }

    if (type) {
      this.hoveredHighlighterShown = type;
      const node = this.inspector.selection.nodeFront;
      this._getHighlighter(type)
          .then(highlighter => highlighter.show(node))
          .then(shown => {
            if (shown) {
              this.emit("highlighter-shown");
            }
          });
    }
  }

  onMouseOut(event) {
    // Only hide the highlighter if the mouse leaves the currently hovered node.
    if (!this._lastHovered ||
        (event && this._lastHovered.contains(event.relatedTarget))) {
      return;
    }

    // Otherwise, hide the highlighter.
    const view = this.isRuleView(this._lastHovered) ?
      this.inspector.getPanel("ruleview").view :
      this.inspector.getPanel("computedview").computedView;
    const nodeInfo = view.getNodeInfo(this._lastHovered);
    if (nodeInfo && this.isRuleViewShapePoint(nodeInfo)) {
      this.hoverPointShapesHighlighter(this.inspector.selection.nodeFront, null);
    }
    this._lastHovered = null;
    this._hideHoveredHighlighter();
  }

  /**
   * Handler function for "markupmutation" events. Hides the flexbox/grid/shapes
   * highlighter if the flexbox/grid/shapes container is no longer in the DOM tree.
   */
  async onMarkupMutation(mutations) {
    const hasInterestingMutation = mutations.some(mut => mut.type === "childList");
    if (!hasInterestingMutation) {
      // Bail out if the mutations did not remove nodes, or if no grid highlighter is
      // displayed.
      return;
    }

    for (const node of this.gridHighlighters.keys()) {
      await this._hideHighlighterIfDeadNode(node, this.hideGridHighlighter);
    }

    await this._hideHighlighterIfDeadNode(this.flexboxHighlighterShown,
      this.hideFlexboxHighlighter);
    await this._hideHighlighterIfDeadNode(this.flexItemHighlighterShown,
      this.hideFlexItemHighlighter);
    await this._hideHighlighterIfDeadNode(this.shapesHighlighterShown,
      this.hideShapesHighlighter);
  }

  /**
   * Clear saved highlighter shown properties on will-navigate.
   */
  async onWillNavigate() {
    this.destroyEditors();

    // Store all the grid highlighters into the pool of reusable grid highlighters, and
    // clear the Map of grid highlighters.
    for (const highlighter of this.gridHighlighters.values()) {
      this.extraGridHighlighterPool.push(highlighter);
    }

    this.gridHighlighters.clear();

    this.boxModelHighlighterShown = null;
    this.flexboxHighlighterShown = null;
    this.flexItemHighlighterShown = null;
    this.geometryEditorHighlighterShown = null;
    this.hoveredHighlighterShown = null;
    this.selectorHighlighterShown = null;
    this.shapesHighlighterShown = null;
  }

  /**
   * Destroy and clean-up all instances of in-context editors.
   */
  destroyEditors() {
    for (const type in this.editors) {
      this.editors[type].off("show");
      this.editors[type].off("hide");
      this.editors[type].destroy();
    }

    this.editors = {};
  }

  /**
   * Destroy all instances of the grid highlighters.
   */
  destroyGridHighlighters() {
    for (const highlighter of this.gridHighlighters.values()) {
      highlighter.finalize();
    }

    for (const highlighter of this.extraGridHighlighterPool) {
      highlighter.finalize();
    }

    this.gridHighlighters.clear();

    this.gridHighlighters = null;
    this.extraGridHighlighterPool = null;
  }

  /**
   * Destroy and clean-up all instances of highlighters.
   */
  destroyHighlighters() {
    for (const type in this.highlighters) {
      if (this.highlighters[type]) {
        this.highlighters[type].finalize();
        this.highlighters[type] = null;
      }
    }

    this.highlighters = null;
  }

  /**
   * Destroy this overlay instance, removing it from the view and destroying
   * all initialized highlighters.
   */
  destroy() {
    this.inspector.off("markupmutation", this.onMarkupMutation);
    this.inspector.target.off("will-navigate", this.onWillNavigate);

    this.destroyEditors();
    this.destroyGridHighlighters();
    this.destroyHighlighters();

    this._lastHovered = null;

    this.inspector = null;
    this.highlighterUtils = null;
    this.state = null;
    this.store = null;

    this.boxModelHighlighterShown = null;
    this.flexboxHighlighterShown = null;
    this.flexItemHighlighterShown = null;
    this.geometryEditorHighlighterShown = null;
    this.hoveredHighlighterShown = null;
    this.selectorHighlighterShown = null;
    this.shapesHighlighterShown = null;

    this.destroyed = true;
  }
}

module.exports = HighlightersOverlay;
