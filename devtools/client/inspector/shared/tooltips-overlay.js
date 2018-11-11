/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * The tooltip overlays are tooltips that appear when hovering over property values and
 * editor tooltips that appear when clicking swatch based editors.
 */

const Services = require("Services");
const flags = require("devtools/shared/flags");
const {
  VIEW_NODE_VALUE_TYPE,
  VIEW_NODE_FONT_TYPE,
  VIEW_NODE_IMAGE_URL_TYPE,
  VIEW_NODE_VARIABLE_TYPE,
} = require("devtools/client/inspector/shared/node-types");

loader.lazyRequireGetter(this, "getColor",
  "devtools/client/shared/theme", true);
loader.lazyRequireGetter(this, "HTMLTooltip",
  "devtools/client/shared/widgets/tooltip/HTMLTooltip", true);
loader.lazyRequireGetter(this, "getImageDimensions",
  "devtools/client/shared/widgets/tooltip/ImageTooltipHelper", true);
loader.lazyRequireGetter(this, "setImageTooltip",
  "devtools/client/shared/widgets/tooltip/ImageTooltipHelper", true);
loader.lazyRequireGetter(this, "setBrokenImageTooltip",
  "devtools/client/shared/widgets/tooltip/ImageTooltipHelper", true);
loader.lazyRequireGetter(this, "setVariableTooltip",
  "devtools/client/shared/widgets/tooltip/VariableTooltipHelper", true);

const PREF_IMAGE_TOOLTIP_SIZE = "devtools.inspector.imagePreviewTooltipSize";

// Types of existing tooltips
const TOOLTIP_IMAGE_TYPE = "image";
const TOOLTIP_FONTFAMILY_TYPE = "font-family";
const TOOLTIP_VARIABLE_TYPE = "variable";

/**
 * Manages all tooltips in the style-inspector.
 *
 * @param {CssRuleView|CssComputedView} view
 *        Either the rule-view or computed-view panel
 */
function TooltipsOverlay(view) {
  this.view = view;
  this._instances = new Map();

  this._onNewSelection = this._onNewSelection.bind(this);
  this.view.inspector.selection.on("new-node-front", this._onNewSelection);

  this.addToView();
}

TooltipsOverlay.prototype = {
  get _cssProperties() {
    return this.view.inspector.cssProperties;
  },

  get isEditing() {
    for (const [, tooltip] of this._instances) {
      if (typeof (tooltip.isEditing) == "function" && tooltip.isEditing()) {
        return true;
      }
    }
    return false;
  },

  /**
   * Add the tooltips overlay to the view. This will start tracking mouse
   * movements and display tooltips when needed
   */
  addToView: function() {
    if (this._isStarted || this._isDestroyed) {
      return;
    }

    this._isStarted = true;

    // Instantiate the preview tooltip when the rule/computed view is hovered over in
    // order to call tooltip.starTogglingOnHover. This will allow the preview tooltip
    // to be shown when an appropriate element is hovered over.
    if (flags.testing) {
      this.getTooltip("previewTooltip");
    } else {
      // Lazily get the preview tooltip to avoid loading HTMLTooltip.
      this.view.element.addEventListener("mousemove", () => {
        this.getTooltip("previewTooltip");
      }, { once: true });
    }
  },

  /**
   * Lazily fetch and initialize the different tooltips that are used in the inspector.
   * These tooltips are attached to the toolbox document if they require a popup panel.
   * Otherwise, it is attached to the inspector panel document if it is an inline editor.
   *
   * @param {String} name
   *        Identifier name for the tooltip
   */
  getTooltip: function(name) {
    let tooltip = this._instances.get(name);
    if (tooltip) {
      return tooltip;
    }
    const { doc } = this.view.inspector.toolbox;
    switch (name) {
      case "colorPicker":
        const SwatchColorPickerTooltip =
          require("devtools/client/shared/widgets/tooltip/SwatchColorPickerTooltip");
        tooltip = new SwatchColorPickerTooltip(doc, this.view.inspector,
          this._cssProperties);
        break;
      case "cubicBezier":
        const SwatchCubicBezierTooltip =
          require("devtools/client/shared/widgets/tooltip/SwatchCubicBezierTooltip");
        tooltip = new SwatchCubicBezierTooltip(doc);
        break;
      case "filterEditor":
        const SwatchFilterTooltip =
          require("devtools/client/shared/widgets/tooltip/SwatchFilterTooltip");
        tooltip = new SwatchFilterTooltip(doc,
          this._cssProperties.getValidityChecker(this.view.inspector.panelDoc));
        break;
      case "previewTooltip":
        tooltip = new HTMLTooltip(doc, {
          type: "arrow",
          useXulWrapper: true,
        });
        tooltip.startTogglingOnHover(this.view.element,
          this._onPreviewTooltipTargetHover.bind(this));
        break;
      default:
        throw new Error(`Unsupported tooltip '${name}'`);
    }
    this._instances.set(name, tooltip);
    return tooltip;
  },

  /**
   * Remove the tooltips overlay from the view. This will stop tracking mouse
   * movements and displaying tooltips
   */
  removeFromView: function() {
    if (!this._isStarted || this._isDestroyed) {
      return;
    }

    for (const [, tooltip] of this._instances) {
      tooltip.destroy();
    }

    this._isStarted = false;
  },

  /**
   * Given a hovered node info, find out which type of tooltip should be shown,
   * if any
   *
   * @param {Object} nodeInfo
   * @return {String} The tooltip type to be shown, or null
   */
  _getTooltipType: function({type, value: prop}) {
    let tooltipType = null;

    // Image preview tooltip
    if (type === VIEW_NODE_IMAGE_URL_TYPE) {
      tooltipType = TOOLTIP_IMAGE_TYPE;
    }

    // Font preview tooltip
    if ((type === VIEW_NODE_VALUE_TYPE && prop.property === "font-family") ||
        (type === VIEW_NODE_FONT_TYPE)) {
      const value = prop.value.toLowerCase();
      if (value !== "inherit" && value !== "unset" && value !== "initial") {
        tooltipType = TOOLTIP_FONTFAMILY_TYPE;
      }
    }

    // Variable preview tooltip
    if (type === VIEW_NODE_VARIABLE_TYPE) {
      tooltipType = TOOLTIP_VARIABLE_TYPE;
    }

    return tooltipType;
  },

  /**
   * Executed by the tooltip when the pointer hovers over an element of the
   * view. Used to decide whether the tooltip should be shown or not and to
   * actually put content in it.
   * Checks if the hovered target is a css value we support tooltips for.
   *
   * @param {DOMNode} target The currently hovered node
   * @return {Promise}
   */
  async _onPreviewTooltipTargetHover(target) {
    const nodeInfo = this.view.getNodeInfo(target);
    if (!nodeInfo) {
      // The hovered node isn't something we care about
      return false;
    }

    const type = this._getTooltipType(nodeInfo);
    if (!type) {
      // There is no tooltip type defined for the hovered node
      return false;
    }

    for (const [, tooltip] of this._instances) {
      if (tooltip.isVisible()) {
        tooltip.revert();
        tooltip.hide();
      }
    }

    const inspector = this.view.inspector;

    if (type === TOOLTIP_IMAGE_TYPE) {
      try {
        await this._setImagePreviewTooltip(nodeInfo.value.url);
      } catch (e) {
        await setBrokenImageTooltip(this.getTooltip("previewTooltip"),
          this.view.inspector.panelDoc);
      }
      return true;
    }

    if (type === TOOLTIP_FONTFAMILY_TYPE) {
      const font = nodeInfo.value.value;
      const nodeFront = inspector.selection.nodeFront;
      await this._setFontPreviewTooltip(font, nodeFront);

      if (nodeInfo.type === VIEW_NODE_FONT_TYPE) {
        // If the hovered element is on the font family span, anchor
        // the tooltip on the whole property value instead.
        return target.parentNode;
      }
      return true;
    }

    if (type === TOOLTIP_VARIABLE_TYPE && nodeInfo.value.value.startsWith("--")) {
      const variable = nodeInfo.value.variable;
      await this._setVariablePreviewTooltip(variable);
      return true;
    }

    return false;
  },

  /**
   * Set the content of the preview tooltip to display an image preview. The image URL can
   * be relative, a call will be made to the debuggee to retrieve the image content as an
   * imageData URI.
   *
   * @param {String} imageUrl
   *        The image url value (may be relative or absolute).
   * @return {Promise} A promise that resolves when the preview tooltip content is ready
   */
  async _setImagePreviewTooltip(imageUrl) {
    const doc = this.view.inspector.panelDoc;
    const maxDim = Services.prefs.getIntPref(PREF_IMAGE_TOOLTIP_SIZE);

    let naturalWidth, naturalHeight;
    if (imageUrl.startsWith("data:")) {
      // If the imageUrl already is a data-url, save ourselves a round-trip
      const size = await getImageDimensions(doc, imageUrl);
      naturalWidth = size.naturalWidth;
      naturalHeight = size.naturalHeight;
    } else {
      const inspectorFront = this.view.inspector.inspector;
      const {data, size} = await inspectorFront.getImageDataFromURL(imageUrl, maxDim);
      imageUrl = await data.string();
      naturalWidth = size.naturalWidth;
      naturalHeight = size.naturalHeight;
    }

    await setImageTooltip(this.getTooltip("previewTooltip"), doc, imageUrl,
      {maxDim, naturalWidth, naturalHeight});
  },

  /**
   * Set the content of the preview tooltip to display a font family preview.
   *
   * @param {String} font
   *        The font family value.
   * @param {object} nodeFront
   *        The NodeActor that will used to retrieve the dataURL for the font
   *        family tooltip contents.
   * @return {Promise} A promise that resolves when the preview tooltip content is ready
   */
  async _setFontPreviewTooltip(font, nodeFront) {
    if (!font || !nodeFront || typeof nodeFront.getFontFamilyDataURL !== "function") {
      throw new Error("Unable to create font preview tooltip content.");
    }

    font = font.replace(/"/g, "'");
    font = font.replace("!important", "");
    font = font.trim();

    const fillStyle = getColor("body-color");
    const {data, size: maxDim} = await nodeFront.getFontFamilyDataURL(font, fillStyle);

    const imageUrl = await data.string();
    const doc = this.view.inspector.panelDoc;
    const {naturalWidth, naturalHeight} = await getImageDimensions(doc, imageUrl);

    await setImageTooltip(this.getTooltip("previewTooltip"), doc, imageUrl,
      {hideDimensionLabel: true, hideCheckeredBackground: true,
       maxDim, naturalWidth, naturalHeight});
  },

  /**
   * Set the content of the preview tooltip to display a variable preview.
   *
   * @param {String} text
   *        The text to display for the variable tooltip
   * @return {Promise} A promise that resolves when the preview tooltip content is ready
   */
  async _setVariablePreviewTooltip(text) {
    const doc = this.view.inspector.panelDoc;
    await setVariableTooltip(this.getTooltip("previewTooltip"), doc, text);
  },

  _onNewSelection: function() {
    for (const [, tooltip] of this._instances) {
      tooltip.hide();
    }
  },

  /**
   * Destroy this overlay instance, removing it from the view
   */
  destroy: function() {
    this.removeFromView();

    this.view.inspector.selection.off("new-node-front", this._onNewSelection);
    this.view = null;

    this._isDestroyed = true;
  },
};

module.exports = TooltipsOverlay;
