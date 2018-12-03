/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Cu } = require("chrome");
const { Actor, ActorClassWithSpec } = require("devtools/shared/protocol");
const {
  flexboxSpec,
  flexItemSpec,
  gridSpec,
  layoutSpec,
} = require("devtools/shared/specs/layout");
const { SHOW_ELEMENT } = require("devtools/shared/dom-node-filter-constants");
const { getStringifiableFragments } =
  require("devtools/server/actors/utils/css-grid-utils");

loader.lazyRequireGetter(this, "CssLogic", "devtools/server/actors/inspector/css-logic", true);
loader.lazyRequireGetter(this, "getCSSStyleRules", "devtools/shared/inspector/css-logic", true);
loader.lazyRequireGetter(this, "isCssPropertyKnown", "devtools/server/actors/css-properties", true);
loader.lazyRequireGetter(this, "parseDeclarations", "devtools/shared/css/parsing-utils", true);
loader.lazyRequireGetter(this, "nodeConstants", "devtools/shared/dom-node-constants");

/**
 * Set of actors the expose the CSS layout information to the devtools protocol clients.
 *
 * The |Layout| actor is the main entry point. It is used to get various CSS
 * layout-related information from the document.
 *
 * The |Flexbox| actor provides the container node information to inspect the flexbox
 * container. It is also used to return an array of |FlexItem| actors which provide the
 * flex item information.
 *
 * The |Grid| actor provides the grid fragment information to inspect the grid container.
 */

const FlexboxActor = ActorClassWithSpec(flexboxSpec, {
  /**
   * @param  {LayoutActor} layoutActor
   *         The LayoutActor instance.
   * @param  {DOMNode} containerEl
   *         The flex container element.
   */
  initialize(layoutActor, containerEl) {
    Actor.prototype.initialize.call(this, layoutActor.conn);

    this.containerEl = containerEl;
    this.walker = layoutActor.walker;
  },

  destroy() {
    Actor.prototype.destroy.call(this);

    this.containerEl = null;
    this.walker = null;
  },

  form(detail) {
    if (detail === "actorid") {
      return this.actorID;
    }

    const styles = CssLogic.getComputedStyle(this.containerEl);

    const form = {
      actor: this.actorID,
      // The computed style properties of the flex container.
      properties: {
        "align-content": styles.alignContent,
        "align-items": styles.alignItems,
        "flex-direction": styles.flexDirection,
        "flex-wrap": styles.flexWrap,
        "justify-content": styles.justifyContent,
      },
    };

    // If the WalkerActor already knows the container element, then also return its
    // ActorID so we avoid the client from doing another round trip to get it in many
    // cases.
    if (this.walker.hasNode(this.containerEl)) {
      form.containerNodeActorID = this.walker.getNode(this.containerEl).actorID;
    }

    return form;
  },

  /**
   * Returns an array of FlexItemActor objects for all the flex item elements contained
   * in the flex container element.
   *
   * @return {Array} An array of FlexItemActor objects.
   */
  getFlexItems() {
    if (isNodeDead(this.containerEl)) {
      return [];
    }

    const flex = this.containerEl.getAsFlexContainer();
    if (!flex) {
      return [];
    }

    const flexItemActors = [];
    const { crossAxisDirection, mainAxisDirection } = flex;

    for (const line of flex.getLines()) {
      for (const item of line.getItems()) {
        flexItemActors.push(new FlexItemActor(this, item.node, {
          crossAxisDirection,
          mainAxisDirection,
          crossMaxSize: item.crossMaxSize,
          crossMinSize: item.crossMinSize,
          mainBaseSize: item.mainBaseSize,
          mainDeltaSize: item.mainDeltaSize,
          mainMaxSize: item.mainMaxSize,
          mainMinSize: item.mainMinSize,
          lineGrowthState: line.growthState,
          clampState: item.clampState,
        }));
      }
    }

    return flexItemActors;
  },
});

/**
 * The FlexItemActor provides information about a flex items' data.
 */
const FlexItemActor = ActorClassWithSpec(flexItemSpec, {
  /**
   * @param  {FlexboxActor} flexboxActor
   *         The FlexboxActor instance.
   * @param  {DOMNode} element
   *         The flex item element.
   * @param  {Object} flexItemSizing
   *         The flex item sizing data.
   */
  initialize(flexboxActor, element, flexItemSizing) {
    Actor.prototype.initialize.call(this, flexboxActor.conn);

    this.containerEl = flexboxActor.containerEl;
    this.element = element;
    this.flexItemSizing = flexItemSizing;
    this.walker = flexboxActor.walker;
  },

  destroy() {
    Actor.prototype.destroy.call(this);

    this.containerEl = null;
    this.element = null;
    this.flexItemSizing = null;
    this.walker = null;
  },

  form(detail) {
    if (detail === "actorid") {
      return this.actorID;
    }

    const { mainAxisDirection } = this.flexItemSizing;
    const dimension = mainAxisDirection.startsWith("horizontal") ? "width" : "height";

    // Find the authored sizing properties for this item.
    const properties = {
      "flex-basis": "",
      "flex-grow": "",
      "flex-shrink": "",
      [`min-${dimension}`]: "",
      [`max-${dimension}`]: "",
      [dimension]: "",
    };

    const isElementNode = this.element.nodeType === this.element.ELEMENT_NODE;

    if (isElementNode) {
      for (const name in properties) {
        const values = [];
        const cssRules = getCSSStyleRules(this.element);

        for (const rule of cssRules) {
        // For each rule, go through *all* properties, because there may be several of
        // them in the same rule and some with !important flags (which would be more
        // important even if placed before another property with the same name)
          const declarations = parseDeclarations(isCssPropertyKnown, rule.style.cssText);

          for (const declaration of declarations) {
            if (declaration.name === name && declaration.value !== "auto") {
              values.push({ value: declaration.value, priority: declaration.priority });
            }
          }
        }

        // Then go through the element style because it's usually more important, but
        // might not be if there is a prior !important property
        if (this.element.style && this.element.style[name] &&
          this.element.style[name] !== "auto") {
          values.push({
            value: this.element.style.getPropertyValue(name),
            priority: this.element.style.getPropertyPriority(name),
          });
        }

        // Now that we have a list of all the property's rule values, go through all the
        // values and show the property value with the highest priority. Therefore, show
        // the last !important value. Otherwise, show the last value stored.
        let rulePropertyValue = "";

        if (values.length) {
          const lastValueIndex = values.length - 1;
          rulePropertyValue = values[lastValueIndex].value;

          for (const { priority, value } of values) {
            if (priority === "important") {
              rulePropertyValue = `${value} !important`;
            }
          }
        }

        properties[name] = rulePropertyValue;
      }
    }

    // Also find some computed sizing properties that will be useful for this item.
    const { flexGrow, flexShrink } = isElementNode
      ? CssLogic.getComputedStyle(this.element)
      : { flexGrow: null, flexShrink: null };
    const computedStyle = { flexGrow, flexShrink };

    const form = {
      actor: this.actorID,
      // The flex item sizing data.
      flexItemSizing: this.flexItemSizing,
      // The authored style properties of the flex item.
      properties,
      // The computed style properties of the flex item.
      computedStyle,
    };

    // If the WalkerActor already knows the flex item element, then also return its
    // ActorID so we avoid the client from doing another round trip to get it in many
    // cases.
    if (this.walker.hasNode(this.element)) {
      form.nodeActorID = this.walker.getNode(this.element).actorID;
    }

    return form;
  },
});

/**
 * The GridActor provides information about a given grid's fragment data.
 */
const GridActor = ActorClassWithSpec(gridSpec, {
  /**
   * @param  {LayoutActor} layoutActor
   *         The LayoutActor instance.
   * @param  {DOMNode} containerEl
   *         The grid container element.
   */
  initialize(layoutActor, containerEl) {
    Actor.prototype.initialize.call(this, layoutActor.conn);

    this.containerEl = containerEl;
    this.walker = layoutActor.walker;
  },

  destroy() {
    Actor.prototype.destroy.call(this);

    this.containerEl = null;
    this.gridFragments = null;
    this.walker = null;
  },

  form(detail) {
    if (detail === "actorid") {
      return this.actorID;
    }

    // Seralize the grid fragment data into JSON so protocol.js knows how to write
    // and read the data.
    const gridFragments = this.containerEl.getGridFragments();
    this.gridFragments = getStringifiableFragments(gridFragments);

    // Record writing mode and text direction for use by the grid outline.
    const { direction, writingMode } = CssLogic.getComputedStyle(this.containerEl);

    const form = {
      actor: this.actorID,
      direction,
      gridFragments: this.gridFragments,
      writingMode,
    };

    // If the WalkerActor already knows the container element, then also return its
    // ActorID so we avoid the client from doing another round trip to get it in many
    // cases.
    if (this.walker.hasNode(this.containerEl)) {
      form.containerNodeActorID = this.walker.getNode(this.containerEl).actorID;
    }

    return form;
  },
});

/**
 * The CSS layout actor provides layout information for the given document.
 */
const LayoutActor = ActorClassWithSpec(layoutSpec, {
  initialize(conn, targetActor, walker) {
    Actor.prototype.initialize.call(this, conn);

    this.targetActor = targetActor;
    this.walker = walker;
  },

  destroy() {
    Actor.prototype.destroy.call(this);

    this.targetActor = null;
    this.walker = null;
  },

  /**
   * Helper function for getAsFlexItem, getCurrentGrid and getCurrentFlexbox. Returns the
   * grid or flex container (whichever is requested) found by iterating on the given
   * selected node. The current node can be a grid/flex container or grid/flex item.
   * If it is a grid/flex item, returns the parent grid/flex container. Otherwise, returns
   * null if the current or parent node is not a grid/flex container.
   *
   * @param  {Node|NodeActor} node
   *         The node to start iterating at.
   * @param  {String} type
   *         Can be "grid" or "flex", the display type we are searching for.
   * @param  {Node|null} container
   *         The container of the current node.
   * @return {GridActor|FlexboxActor|null} The GridActor or FlexboxActor of the
   * grid/flex container of the give node. Otherwise, returns null.
   */
  getCurrentDisplay(node, type, container) {
    if (isNodeDead(node)) {
      return null;
    }

    // Given node can either be a Node or a NodeActor.
    if (node.rawNode) {
      node = node.rawNode;
    }

    const treeWalker = this.walker.getDocumentWalker(node, SHOW_ELEMENT);
    let currentNode = treeWalker.currentNode;
    let displayType = this.walker.getNode(currentNode).displayType;

    // If the node is an element, check first if it is itself a flex or a grid.
    if (currentNode.nodeType === currentNode.ELEMENT_NODE) {
      if (!displayType) {
        return null;
      }

      if (type == "flex") {
        // If only the current node is being considered when finding its display, then
        // return it as only a flex container.
        if ((displayType == "inline-flex" || displayType == "flex") &&
              !container) {
          return new FlexboxActor(this, currentNode);

        // If considering the current node's container, then we are trying to determine
        // the current node's flex item status.
        } else if (container) {
          if (this.isNodeFlexItemInContainer(currentNode, container)) {
            return new FlexboxActor(this, container);
          }
        }
      } else if (type == "grid" &&
                 (displayType == "inline-grid" || displayType == "grid")) {
        return new GridActor(this, currentNode);
      }
    }

    // Otherwise, check if this is a flex/grid item or the parent node is a flex/grid
    // container.
    // Note that text nodes that are children of flex/grid containers are wrapped in
    // anonymous containers, so even if their displayType getter returns null we still
    // want to walk up the chain to find their container.
    while ((currentNode = treeWalker.parentNode())) {
      if (!currentNode) {
        break;
      }

      displayType = this.walker.getNode(currentNode).displayType;

      if (type == "flex" &&
          (displayType == "inline-flex" || displayType == "flex")) {
        if (this.isNodeFlexItemInContainer(node, currentNode)) {
          return new FlexboxActor(this, currentNode);
        }
      } else if (type == "grid" &&
                 (displayType == "inline-grid" || displayType == "grid")) {
        return new GridActor(this, currentNode);
      } else if (displayType == "contents") {
        // Continue walking up the tree since the parent node is a content element.
        continue;
      }

      break;
    }

    return null;
  },

  /**
   * Returns the grid container found by iterating on the given selected node. The current
   * node can be a grid container or grid item. If it is a grid item, returns the parent
   * grid container. Otherwise, return null if the current or parent node is not a grid
   * container.
   *
   * @param  {Node|NodeActor} node
   *         The node to start iterating at.
   * @return {GridActor|null} The GridActor of the grid container of the given node.
   * Otherwise, returns null.
   */
  getCurrentGrid(node) {
    return this.getCurrentDisplay(node, "grid");
  },

  /**
   * Returns the flex container found by iterating on the given selected node. The current
   * node can be a flex container or flex item. If it is a flex item, returns the parent
   * flex container. Otherwise, return null if the current or parent node is not a flex
   * container.
   *
   * @param  {Node|NodeActor} node
   *         The node to start iterating at.
   * @param  {Boolean|null} onlyLookAtParents
   *         Whether or not to only consider the parent node of the given node.
   * @return {FlexboxActor|null} The FlexboxActor of the flex container of the given node.
   * Otherwise, returns null.
   */
  getCurrentFlexbox(node, onlyLookAtParents) {
    let container = null;

    if (onlyLookAtParents) {
      container = node.rawNode.parentNode;
    }

    return this.getCurrentDisplay(node, "flex", container);
  },

  /**
   * Returns an array of GridActor objects for all the grid elements contained in the
   * given root node.
   *
   * @param  {Node|NodeActor} node
   *         The root node for grid elements
   * @return {Array} An array of GridActor objects.
   */
  getGrids(node) {
    if (isNodeDead(node)) {
      return [];
    }

    // Root node can either be a Node or a NodeActor.
    if (node.rawNode) {
      node = node.rawNode;
    }

    // Root node can be a #document object, which does not support getElementsWithGrid.
    if (node.nodeType === nodeConstants.DOCUMENT_NODE) {
      node = node.documentElement;
    }

    const gridElements = node.getElementsWithGrid();
    let gridActors = gridElements.map(n => new GridActor(this, n));

    const frames = node.querySelectorAll("iframe, frame");
    for (const frame of frames) {
      gridActors = gridActors.concat(this.getGrids(frame.contentDocument));
    }

    return gridActors;
  },

  /**
   * Returns whether or not the given node is actually considered a flex item by its
   * container.
   *
   * @param  {Node|NodeActor} supposedItem
   *         The node that might be a flex item of its container.
   * @param  {Node} container
   *         The node's container.
   * @return {Boolean} Whether or not the node we are looking at is a flex item of its
   * container.
   */
  isNodeFlexItemInContainer(supposedItem, container) {
    const containerDisplayType = this.walker.getNode(container).displayType;

    if (containerDisplayType == "inline-flex" || containerDisplayType == "flex") {
      const containerFlex = container.getAsFlexContainer();

      for (const line of containerFlex.getLines()) {
        for (const item of line.getItems()) {
          if (item.node === supposedItem) {
            return true;
          }
        }
      }
    }

    return false;
  },
});

function isNodeDead(node) {
  return !node || (node.rawNode && Cu.isDeadWrapper(node.rawNode));
}

exports.FlexboxActor = FlexboxActor;
exports.FlexItemActor = FlexItemActor;
exports.GridActor = GridActor;
exports.LayoutActor = LayoutActor;
