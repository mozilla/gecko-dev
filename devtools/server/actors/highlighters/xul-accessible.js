/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { getBounds, XULWindowInfobar } = require("./utils/accessibility");
const { createNode, isNodeValid } = require("./utils/markup");
const { getCurrentZoom, loadSheet } = require("devtools/shared/layout/utils");
const { TEXT_NODE } = require("devtools/shared/dom-node-constants");

/**
 * Stylesheet used for highlighter styling of accessible objects in chrome. It
 * is consistent with the styling of an in-content accessible highlighter.
 */
const ACCESSIBLE_BOUNDS_SHEET = "data:text/css;charset=utf-8," + encodeURIComponent(`
  .highlighter-container {
    --highlighter-bubble-background-color: hsl(214, 13%, 24%);
    --highlighter-bubble-border-color: rgba(255, 255, 255, 0.2);
    --highlighter-bubble-arrow-size: 8px;
  }

  .accessible-bounds {
    position: fixed;
    pointer-events: none;
    z-index: 10;
    display: block;
    background-color: #6a5acd!important;
    opacity: 0.6;
  }

  .accessible-infobar-container {
    position: fixed;
    max-width: 90%;
    z-index: 11;
  }

  .accessible-infobar {
    position: relative;
    left: -50%;
    background-color: var(--highlighter-bubble-background-color);
    min-width: 75px;
    border: 1px solid var(--highlighter-bubble-border-color);
    border-radius: 3px;
    padding: 5px;
  }

  .accessible-arrow {
    position: absolute;
    width: 0;
    height: 0;
    border-left: var(--highlighter-bubble-arrow-size) solid transparent;
    border-right: var(--highlighter-bubble-arrow-size) solid transparent;
    left: calc(50% - var(--highlighter-bubble-arrow-size));
  }

  .top {
    border-bottom: var(--highlighter-bubble-arrow-size) solid
      var(--highlighter-bubble-background-color);
    top: calc(-1 * var(--highlighter-bubble-arrow-size));
  }

  .bottom {
    border-top: var(--highlighter-bubble-arrow-size) solid
      var(--highlighter-bubble-background-color);
    bottom: calc(-1 * var(--highlighter-bubble-arrow-size));
  }

  .accessible-infobar-text {
    overflow: hidden;
    white-space: nowrap;
    display: flex;
    justify-content: center;
  }

  .accessible-infobar-name,
  .accessible-infobar-audit {
    color: hsl(210, 30%, 85%);
    max-width: 90%;
  }

  .accessible-infobar-audit .accessible-contrast-ratio:not(:empty).AA:after,
  .accessible-infobar-audit .accessible-contrast-ratio:not(:empty).AAA:after {
    color: #90E274;
  }

  .accessible-infobar-audit .accessible-contrast-ratio:not(:empty).fail:after {
    color: #E57180;
    content: " ⚠️";
  }

  .accessible-infobar-audit .accessible-contrast-ratio:not(:empty).AA:after {
    content: " AA\u2713";
  }

  .accessible-infobar-audit .accessible-contrast-ratio:not(:empty).AAA:after {
    content: " AAA\u2713";
  }

  .accessible-infobar-name:not(:empty),
  .accessible-infobar-audit:not(:empty) {
    border-inline-start: 1px solid #5a6169;
    margin-inline-start: 6px;
    padding-inline-start: 6px;
  }

  .accessible-infobar-role {
    color: #9CDCFE;
  }`);

/**
 * The XULWindowAccessibleHighlighter is a class that has the same API as the
 * AccessibleHighlighter, and by extension other highlighters that implement
 * auto-refresh highlighter, but instead of drawing in canvas frame anonymous
 * content (that is not available for chrome accessible highlighting) it adds a
 * transparrent inactionable element with the same position and bounds as the
 * accessible object highlighted. Unlike SimpleOutlineHighlighter, we can't use
 * element (that corresponds to accessible object) itself because the accessible
 * position and bounds are calculated differently.
 *
 * It is used when canvasframe-based AccessibleHighlighter can't be used. This
 * is the case for XUL windows.
 */
class XULWindowAccessibleHighlighter {
  constructor(highlighterEnv) {
    this.ID_CLASS_PREFIX = "accessible-";
    this.highlighterEnv = highlighterEnv;
    this.win = highlighterEnv.window;
    this.accessibleInfobar = new XULWindowInfobar(this);
  }

  /**
   * Static getter that indicates that XULWindowAccessibleHighlighter supports
   * highlighting in XUL windows.
   */
  static get XULSupported() {
    return true;
  }

  /**
   * Build highlighter markup.
   */
  _buildMarkup() {
    const doc = this.win.document;
    loadSheet(doc.ownerGlobal, ACCESSIBLE_BOUNDS_SHEET);

    this.container = createNode(this.win, {
      parent: doc.body || doc.documentElement,
      attributes: {
        "class": "highlighter-container",
        "aria-hidden": "true",
      },
    });

    this.bounds = createNode(this.win, {
      parent: this.container,
      attributes: {
        "class": "accessible-bounds",
      },
    });

    this.accessibleInfobar.buildMarkup(this.container);
  }

  /**
   * Get current accessible bounds.
   *
   * @return {Object|null} Returns, if available, positioning and bounds
   *                       information for the accessible object.
   */
  get _bounds() {
    // Zoom level for the top level browser window does not change and only inner frames
    // do. So we need to get the zoom level of the current node's parent window.
    const zoom = getCurrentZoom(this.currentNode);

    return getBounds(this.win, { ...this.options, zoom });
  }

  /**
   * Show the highlighter on a given accessible.
   *
   * @param {DOMNode} node
   *        A dom node that corresponds to the accessible object.
   * @param {Object} options
   *        Object used for passing options. Available options:
   *         - {Number} x
   *           x coordinate of the top left corner of the accessible object
   *         - {Number} y
   *           y coordinate of the top left corner of the accessible object
   *         - {Number} w
   *           width of the the accessible object
   *         - {Number} h
   *           height of the the accessible object
   *         - duration {Number}
   *                    Duration of time that the highlighter should be shown.
   *         - {String|null} name
   *           name of the the accessible object
   *         - {String} role
   *           role of the the accessible object
   *
   * @return {Boolean} True if accessible is highlighted, false otherwise.
   */
  show(node, options = {}) {
    const isSameNode = node === this.currentNode;
    const hasBounds = options && typeof options.x == "number" &&
                               typeof options.y == "number" &&
                               typeof options.w == "number" &&
                               typeof options.h == "number";
    if (!hasBounds || !this._isNodeValid(node) || isSameNode) {
      return false;
    }

    this.options = options;
    this.currentNode = node;

    return this._show();
  }

  /**
   * Internal show method that updates bounds and tracks duration based
   * highlighting.
   *
   * @return {Boolean} True if accessible is highlighted, false otherwise.
   */
  _show() {
    if (this._highlightTimer) {
      clearTimeout(this._highlightTimer);
      this._highlightTimer = null;
    }

    const shown = this._update();
    const { duration } = this.options;
    if (shown && duration) {
      this._highlightTimer = setTimeout(() => {
        this._hideAccessibleBounds();
      }, duration);
    }
    return shown;
  }

  /**
   * Update accessible bounds for a current accessible. Re-draw highlighter
   * markup.
   *
   * @return {Boolean} True if accessible is highlighted, false otherwise.
   */
  _update() {
    this._hideAccessibleBounds();
    const bounds = this._bounds;
    if (!bounds) {
      return false;
    }

    let boundsEl = this.bounds;

    if (!boundsEl) {
      this._buildMarkup();
      boundsEl = this.bounds;
    }

    const { left, top, width, height } = bounds;
    boundsEl.style.top = `${top}px`;
    boundsEl.style.left = `${left}px`;
    boundsEl.style.width = `${width}px`;
    boundsEl.style.height = `${height}px`;

    this._showAccessibleBounds();
    this.accessibleInfobar.show();

    return true;
  }

  /**
   * Hide the highlighter
   */
  hide() {
    if (!this.currentNode || !this.highlighterEnv.window) {
      return;
    }

    this._hideAccessibleBounds();
    this.accessibleInfobar.hide();

    this.currentNode = null;
    this.options = null;
  }

  /**
   * Check if node is a valid element or text node.
   *
   * @param  {DOMNode} node
   *         The node to highlight.
   * @return {Boolean} whether or not node is valid.
   */
  _isNodeValid(node) {
    return isNodeValid(node) || isNodeValid(node, TEXT_NODE);
  }

  /**
   * Show accessible bounds highlighter.
   */
  _showAccessibleBounds() {
    if (this.container) {
      this.container.removeAttribute("hidden");
    }
  }

  /**
   * Hide accessible bounds highlighter.
   */
  _hideAccessibleBounds() {
    if (this.container) {
      this.container.setAttribute("hidden", "true");
    }
  }

  /**
   * Hide accessible highlighter, clean up and remove the markup.
   */
  destroy() {
    if (this._highlightTimer) {
      clearTimeout(this._highlightTimer);
      this._highlightTimer = null;
    }

    this.hide();
    if (this.container) {
      this.container.remove();
    }

    this.accessibleInfobar.destroy();

    this.accessibleInfobar = null;
    this.win = null;
  }
}

exports.XULWindowAccessibleHighlighter = XULWindowAccessibleHighlighter;
