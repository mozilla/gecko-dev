/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * A cache of AreaPositionManagers weakly mapped to customization area nodes.
 *
 * @type {WeakMap<DOMNode, AreaPositionManager>}
 */
var gManagers = new WeakMap();

const kPaletteId = "customization-palette";

/**
 * An AreaPositionManager is used to power the animated drag-and-drop grid
 * behaviour of each customizable area (toolbars, the palette, the overflow
 * panel) while in customize mode. Each customizable area has its own
 * AreaPositionManager, per browser window.
 */
class AreaPositionManager {
  /**
   * True if the container is oriented from right-to-left.
   *
   * @type {boolean}
   */
  #rtl = false;

  /**
   * A DOMRectReadOnly for the bounding client rect for the container,
   * collected once during construction.
   *
   * @type {DOMRectReadOnly|null}
   */
  #containerInfo = null;

  /**
   * The calculated horizontal distance between the first two visible child
   * nodes of the container.
   *
   * @type {number}
   */
  #horizontalDistance = 0;

  /**
   * The ratio of the width of the container and the height of the first
   * visible child node. This is used in the weighted cartesian distance
   * calculation used in the AreaPositionManager.find method.
   *
   * @see AreaPositionManager.find
   * @type {number}
   */
  #heightToWidthFactor = 0;

  /**
   * Constructs an instance of AreaPositionManager for a customizable area.
   *
   * @param {DOMNode} aContainer
   *   The customizable area container node for which drag-and-drop animations
   *   are to be calculated for.
   */
  constructor(aContainer) {
    // Caching the direction and bounds of the container for quick access later:
    this.#rtl = aContainer.ownerGlobal.RTL_UI;
    this.#containerInfo = DOMRectReadOnly.fromRect(
      aContainer.getBoundingClientRect()
    );
    this.update(aContainer);
  }

  /**
   * A cache of container child node size and position data.
   *
   * @type {WeakMap<DOMNode, DOMRectReadOnly>}
   */
  #nodePositionStore = new WeakMap();

  /**
   * The child node immediately after the most recently placed placeholder. May
   * be null if no placeholder has been inserted yet, or if the placeholder is
   * at the end of the container.
   *
   * @type {DOMNode|null}
   */
  #lastPlaceholderInsertion = null;

  /**
   * Iterates the visible children of the container, sampling their bounding
   * client rects and storing them in a local cache. Also collects and stores
   * metrics like the horizontal distance between the first two children,
   * the height of the first item, and a ratio between the width of the
   * container and the height of the first child item.
   *
   * @param {DOMNode} aContainer
   *   The container node to collect the measurements for.
   */
  update(aContainer) {
    let last = null;
    let singleItemHeight;
    for (let child of aContainer.children) {
      if (child.hidden) {
        continue;
      }
      let coordinates = this.#lazyStoreGet(child);
      // We keep a baseline horizontal distance between nodes around
      // for use when we can't compare with previous/next nodes
      if (!this.#horizontalDistance && last) {
        this.#horizontalDistance = coordinates.left - last.left;
      }
      // We also keep the basic height of items for use below:
      if (!singleItemHeight) {
        singleItemHeight = coordinates.height;
      }
      last = coordinates;
    }
    this.#heightToWidthFactor = this.#containerInfo.width / singleItemHeight;
  }

  /**
   * Find the closest node in the container given the coordinates.
   * "Closest" is defined in a somewhat strange manner: we prefer nodes
   * which are in the same row over nodes that are in a different row.
   * In order to implement this, we use a weighted cartesian distance
   * where dy is more heavily weighted by a factor corresponding to the
   * ratio between the container's width and the height of its elements.
   *
   * @param {DOMNode} aContainer
   *   The container element that contains one or more rows of child elements
   *   in some kind of grid formation.
   * @param {number} aX
   *   The X horizontal coordinate that we're finding the closest child node
   *   for.
   * @param {number} aY
   *   The Y vertical coordinate that we're finding the closest child node
   *   for.
   * @returns {DOMNode|null}
   *   The closest node to the aX and aY coordinates, preferring child nodes
   *   in the same row of the grid. This may also return the container itself,
   *   if the coordinates are on the outside edge of the last node in the
   *   container.
   */
  find(aContainer, aX, aY) {
    let closest = null;
    let minCartesian = Number.MAX_VALUE;
    let containerX = this.#containerInfo.left;
    let containerY = this.#containerInfo.top;

    // First, iterate through all children and find the closest child to the
    // aX and aY coordinates (preferring children in the same row as the aX
    // and aY coordinates).
    for (let node of aContainer.children) {
      let coordinates = this.#lazyStoreGet(node);
      let offsetX = coordinates.x - containerX;
      let offsetY = coordinates.y - containerY;
      let hDiff = offsetX - aX;
      let vDiff = offsetY - aY;
      // Then compensate for the height/width ratio so that we prefer items
      // which are in the same row:
      hDiff /= this.#heightToWidthFactor;

      let cartesianDiff = hDiff * hDiff + vDiff * vDiff;
      if (cartesianDiff < minCartesian) {
        minCartesian = cartesianDiff;
        closest = node;
      }
    }

    // Now refine our result based on whether or not we're closer to the outside
    // edge of the closest node. If we are, we actually want to return the
    // closest node's sibling, because this is the one we'll style to indicate
    // the drop position.
    if (closest) {
      let targetBounds = this.#lazyStoreGet(closest);
      let farSide = this.#rtl ? "left" : "right";
      let outsideX = targetBounds[farSide];
      // Check if we're closer to the next target than to this one:
      // Only move if we're not targeting a node in a different row:
      if (aY > targetBounds.top && aY < targetBounds.bottom) {
        if ((!this.#rtl && aX > outsideX) || (this.#rtl && aX < outsideX)) {
          return closest.nextElementSibling || aContainer;
        }
      }
    }
    return closest;
  }

  /**
   * "Insert" a "placeholder" by shifting the subsequent children out of the
   * way. We go through all the children, and shift them based on the position
   * they would have if we had inserted something before aBefore. We use CSS
   * transforms for this, which are CSS transitioned.
   *
   * @param {DOMNode} aContainer
   *   The container of the nodes for which we are inserting the placeholder
   *   and shifting the child nodes.
   * @param {DOMNode} aBefore
   *   The child node before which we are inserting the placeholder.
   * @param {DOMRectReadOnly} aSize
   *   The size of the placeholder to create.
   * @param {boolean} aIsFromThisArea
   *   True if the node being dragged happens to be from this container, as
   *   opposed to some other container (like a toolbar, for instance).
   */
  insertPlaceholder(aContainer, aBefore, aSize, aIsFromThisArea) {
    let isShifted = false;
    for (let child of aContainer.children) {
      // Don't need to shift hidden nodes:
      if (child.hidden) {
        continue;
      }
      // If this is the node before which we're inserting, start shifting
      // everything that comes after. One exception is inserting at the end
      // of the menupanel, in which case we do not shift the placeholders:
      if (child == aBefore) {
        isShifted = true;
      }
      if (isShifted) {
        if (aIsFromThisArea && !this.#lastPlaceholderInsertion) {
          child.setAttribute("notransition", "true");
        }
        // Determine the CSS transform based on the next node and apply it.
        child.style.transform = this.#diffWithNext(child, aSize);
      } else {
        // If we're not shifting this node, reset the transform
        child.style.transform = "";
      }
    }

    // Bug 959848: without this routine, when we start the drag of an item in
    // the customization palette, we'd take the dragged item out of the flow of
    // the document, and _then_ insert the placeholder, creating a lot of motion
    // on the initial drag. We mask this case by removing the item and inserting
    // the placeholder for the dragged item in a single shot without animation.
    if (
      aContainer.lastElementChild &&
      aIsFromThisArea &&
      !this.#lastPlaceholderInsertion
    ) {
      // Flush layout to force the snap transition.
      aContainer.lastElementChild.getBoundingClientRect();
      // then remove all the [notransition]
      for (let child of aContainer.children) {
        child.removeAttribute("notransition");
      }
    }
    this.#lastPlaceholderInsertion = aBefore;
  }

  /**
   * Reset all the transforms in this container, optionally without
   * transitioning them.
   *
   * @param {DOMNode} aContainer
   *   The container in which to reset the transforms.
   * @param {boolean} aNoTransition
   *   If truthy, adds a notransition attribute to the node while resetting the
   *   transform. It is assumed that a CSS rule will interpret the notransition
   *   attribute as a directive to skip transition animations.
   */
  clearPlaceholders(aContainer, aNoTransition) {
    for (let child of aContainer.children) {
      if (aNoTransition) {
        child.setAttribute("notransition", true);
      }
      child.style.transform = "";
      if (aNoTransition) {
        // Need to force a reflow otherwise this won't work. :(
        child.getBoundingClientRect();
        child.removeAttribute("notransition");
      }
    }
    // We snapped back, so we can assume there's no more
    // "last" placeholder insertion point to keep track of.
    if (aNoTransition) {
      this.#lastPlaceholderInsertion = null;
    }
  }

  /**
   * Determines the transform rule to apply to aNode to reposition it to
   * accommodate a placeholder drop target for a dragged node of aSize.
   *
   * @param {DOMNode} aNode
   *   The node to calculate the transform rule for.
   * @param {DOMRectReadOnly} aSize
   *   The size of the placeholder drop target that was inserted which then
   *   requires us to reposition this node.
   * @returns {string}
   *   The CSS transform rule to apply to aNode.
   */
  #diffWithNext(aNode, aSize) {
    let xDiff;
    let yDiff = null;
    let nodeBounds = this.#lazyStoreGet(aNode);
    let side = this.#rtl ? "right" : "left";
    let next = this.#getVisibleSiblingForDirection(aNode, "next");
    // First we determine the transform along the x axis.
    // Usually, there will be a next node to base this on:
    if (next) {
      let otherBounds = this.#lazyStoreGet(next);
      xDiff = otherBounds[side] - nodeBounds[side];
      // We set this explicitly because otherwise some strange difference
      // between the height and the actual difference between line creeps in
      // and messes with alignments
      yDiff = otherBounds.top - nodeBounds.top;
    } else {
      // We don't have a sibling whose position we can use. First, let's see
      // if we're also the first item (which complicates things):
      let firstNode = this.#firstInRow(aNode);
      if (aNode == firstNode) {
        // Maybe we stored the horizontal distance between nodes,
        // if not, we'll use the width of the incoming node as a proxy:
        xDiff = this.#horizontalDistance || (this.#rtl ? -1 : 1) * aSize.width;
      } else {
        // If not, we should be able to get the distance to the previous node
        // and use the inverse, unless there's no room for another node (ie we
        // are the last node and there's no room for another one)
        xDiff = this.#moveNextBasedOnPrevious(aNode, nodeBounds, firstNode);
      }
    }

    // If we've not determined the vertical difference yet, check it here
    if (yDiff === null) {
      // If the next node is behind rather than in front, we must have moved
      // vertically:
      if ((xDiff > 0 && this.#rtl) || (xDiff < 0 && !this.#rtl)) {
        yDiff = aSize.height;
      } else {
        // Otherwise, we haven't
        yDiff = 0;
      }
    }
    return "translate(" + xDiff + "px, " + yDiff + "px)";
  }

  /**
   * Helper function to find the horizontal transform value for a node if there
   * isn't a next node to base that on.
   *
   * @param {DOMNode} aNode
   *   The node to have the transform applied to.
   * @param {DOMRectReadOnly} aNodeBounds
   *   The bounding rect info of aNode.
   * @param {DOMNode} aFirstNodeInRow
   *   The first node in aNode's row in the container grid.
   * @returns {number}
   *   The horizontal distance to transform aNode.
   */
  #moveNextBasedOnPrevious(aNode, aNodeBounds, aFirstNodeInRow) {
    let next = this.#getVisibleSiblingForDirection(aNode, "previous");
    let otherBounds = this.#lazyStoreGet(next);
    let side = this.#rtl ? "right" : "left";
    let xDiff = aNodeBounds[side] - otherBounds[side];
    // If, however, this means we move outside the container's box
    // (i.e. the row in which this item is placed is full)
    // we should move it to align with the first item in the next row instead
    let bound = this.#containerInfo[this.#rtl ? "left" : "right"];
    if (
      (!this.#rtl && xDiff + aNodeBounds.right > bound) ||
      (this.#rtl && xDiff + aNodeBounds.left < bound)
    ) {
      xDiff = this.#lazyStoreGet(aFirstNodeInRow)[side] - aNodeBounds[side];
    }
    return xDiff;
  }

  /**
   * Get the DOMRectReadOnly for a node from our cache. If the rect is not yet
   * cached, calculate that rect and cache it now.
   *
   * @param {DOMNode} aNode
   *   The node whose DOMRectReadOnly that we want.
   * @returns {DOMRectReadOnly}
   *   The size and position of aNode that was either just calculated, or
   *   previously calculated during the lifetime of this AreaPositionManager and
   *   cached.
   */
  #lazyStoreGet(aNode) {
    let rect = this.#nodePositionStore.get(aNode);
    if (!rect) {
      // getBoundingClientRect() returns a DOMRect that is live, meaning that
      // as the element moves around, the rects values change. We don't want
      // that - we want a snapshot of what the rect values are right at this
      // moment, and nothing else. So we have to clone the values as a
      // DOMRectReadOnly.
      rect = DOMRectReadOnly.fromRect(aNode.getBoundingClientRect());
      this.#nodePositionStore.set(aNode, rect);
    }
    return rect;
  }

  /**
   * Returns the first node in aNode's row in the container grid.
   *
   * @param {DOMNode} aNode
   *   The node in the row for which we want to find the first node.
   * @returns {DOMNode}
   */
  #firstInRow(aNode) {
    // XXXmconley: I'm not entirely sure why we need to take the floor of these
    // values - it looks like, periodically, we're getting fractional pixels back
    // from lazyStoreGet. I've filed bug 994247 to investigate.
    let bound = Math.floor(this.#lazyStoreGet(aNode).top);
    let rv = aNode;
    let prev;
    while (rv && (prev = this.#getVisibleSiblingForDirection(rv, "previous"))) {
      if (Math.floor(this.#lazyStoreGet(prev).bottom) <= bound) {
        return rv;
      }
      rv = prev;
    }
    return rv;
  }

  /**
   * Returns the next visible sibling DOMNode to aNode in the direction
   * aDirection.
   *
   * @param {DOMNode} aNode
   *   The node to get the next visible sibling for.
   * @param {string} aDirection
   *   One of either "previous" or "next". Any other value will probably throw.
   * @returns {DOMNode}
   */
  #getVisibleSiblingForDirection(aNode, aDirection) {
    let rv = aNode;
    do {
      rv = rv[aDirection + "ElementSibling"];
    } while (rv && rv.hidden);
    return rv;
  }
}

/**
 * DragPositionManager manages the AreaPositionManagers for all of the
 * grid-like customizable areas. These days, that's just the customization
 * palette.
 */
export var DragPositionManager = {
  /**
   * Starts CustomizeMode drag position management for a window aWindow.
   *
   * @param {DOMWindow} aWindow
   *   The browser window to start drag position management in.
   */
  start(aWindow) {
    let paletteArea = aWindow.document.getElementById(kPaletteId);
    let positionManager = gManagers.get(paletteArea);
    if (positionManager) {
      positionManager.update(paletteArea);
    } else {
      // This gManagers WeakMap may have made more sense when we had the
      // menu panel also acting as a grid. It's maybe superfluous at this point.
      gManagers.set(paletteArea, new AreaPositionManager(paletteArea));
    }
  },

  /**
   * Stops CustomizeMode drag position management for all windows.
   */
  stop() {
    gManagers = new WeakMap();
  },

  /**
   * Returns the AreaPositionManager instance for a particular aArea DOMNode,
   * if one has been created.
   *
   * @param {DOMNode} aArea
   * @returns {AreaPositionManager|null}
   */
  getManagerForArea(aArea) {
    return gManagers.get(aArea);
  },
};

Object.freeze(DragPositionManager);
