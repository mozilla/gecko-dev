/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html } from "../vendor/lit.all.mjs";
import { MozLitElement } from "../lit-utils.mjs";

const REORDER_EVENT = "reorder";
const DRAGSTART_EVENT = "dragstarted";
const DRAGEND_EVENT = "dragended";
const DRAG_DATA_TYPE_PREFIX = "text/reorderable-item/";
const REORDER_PROP = "__mozReorderableIndex";

/**
 * A wrapper element that allows its children to be reordered by dragging and
 * dropping. The element emits the custom `reorder` event when an item is
 * dropped in a new position, which you can use to perform the actual
 * reordering.
 *
 * The detail object of the `reorder` event contains the following properties:
 *
 * - `draggedElement`: The element that was dragged.
 * - `targetElement`: The element over which the dragged element was dropped.
 * - `position`: The position of the drop relative to the target element. -1
 *   means before, 0 means after.
 *
 * Which children are reorderable is determined by the `itemSelector` property.
 *
 * Things to keep in mind when using this element:
 *
 * - Preserve the focus when reordering items.
 * - Check that the reordering shortcuts are not in conflict with other
 *   shortcuts.
 * - Make sure that reordering is picked up by screen readers. Usually DOM
 *   updates cause the reordered element to be read out again, which is
 *   sufficient.
 *
 * @tagname moz-reorderable-list
 * @property {string} itemSelector - Selector for elements that should be
 *   reorderable.
 * @fires reorder - Fired when an item is dropped in a new position.
 * @fires dragstarted - Fired when an item is dragged.
 * @fires dragended - Fired when an item is dropped.
 */
export default class MozReorderableList extends MozLitElement {
  static queries = {
    slotEl: "slot",
    indicatorEl: ".indicator",
  };

  static properties = {
    itemSelector: { type: String },
  };

  #draggedElement = null;
  #dropTargetInfo = null;
  #mutationObserver = null;
  #items = [];

  isXULElement(element) {
    return window.XULElement?.isInstance?.(element);
  }

  getBounds(element) {
    return (
      window.windowUtils?.getBoundsWithoutFlushing?.(element) ||
      element.getBoundingClientRect()
    );
  }

  constructor() {
    super();
    this.itemSelector = "li";
    this.addEventListener("dragstart", this.onDragStart);
    this.addEventListener("dragover", this.onDragOver);
    this.addEventListener("dragleave", this.onDragLeave);
    this.addEventListener("dragend", this.onDragEnd);
    this.addEventListener("drop", this.onDrop);
    this.#mutationObserver = new MutationObserver((...args) =>
      this.onMutation(...args)
    );
  }

  firstUpdated() {
    super.firstUpdated();
    this.getItems();
    this.addDraggableAttribute();
  }

  connectedCallback() {
    super.connectedCallback();
    this.#mutationObserver.observe(this, {
      childList: true,
      subtree: true,
    });
  }

  disconnectedCallback() {
    super.disconnectedCallback();
    this.#mutationObserver.disconnect();
  }

  onMutation(mutationList) {
    let needsUpdate = false;

    for (const mutation of mutationList) {
      if (mutation.addedNodes.length || mutation.removedNodes.length) {
        needsUpdate = true;
      }

      for (const addedNode of mutation.addedNodes) {
        if (addedNode.nodeType === Node.ELEMENT_NODE) {
          this.addDraggableAttribute(addedNode);
        }
      }
    }

    if (needsUpdate) {
      this.getItems();
    }
  }

  /**
   * Add the draggable attribute to all items that match the selector.
   *
   * @see getItems for information about the root parameter.
   */
  addDraggableAttribute(root) {
    let items = root
      ? this.getAssignedElementsBySelector(this.itemSelector, root)
      : this.#items;
    for (const item of items) {
      // Unlike XUL elements, HTML elements are not draggable by default.
      // So we need to set the draggable attribute on all items that match the selector.
      if (!this.isXULElement(item)) {
        item.draggable = true;
      }
    }
  }

  onDragStart(event) {
    let draggedElement = event.target.closest(this.itemSelector);
    if (!draggedElement) {
      return;
    }

    const dragIndex = this.getItemIndex(draggedElement);
    if (dragIndex === -1) {
      return;
    }

    event.stopPropagation();

    this.emitEvent(DRAGSTART_EVENT, {
      draggedElement,
    });

    // XUL elements need dataTransfer values to be set for drag and drop to work.
    if (this.isXULElement(draggedElement)) {
      let documentId = draggedElement.ownerDocument.documentElement.id;
      event.dataTransfer.mozSetDataAt(
        `${DRAG_DATA_TYPE_PREFIX}${documentId}`,
        draggedElement.id,
        0
      );
      event.dataTransfer.addElement(draggedElement);
      event.dataTransfer.effectAllowed = "move";
    }

    this.#draggedElement = draggedElement;
  }

  onDragOver(event) {
    this.#dropTargetInfo = this.getDropTargetInfo(event);
    if (!this.#dropTargetInfo) {
      this.indicatorEl.hidden = true;
      return;
    }
    event.preventDefault();
    event.stopPropagation();
    const { targetIndex, position } = this.#dropTargetInfo;
    const items = this.#items;
    const item = items[targetIndex];

    if (!item) {
      this.indicatorEl.hidden = true;
      return;
    }

    const containerRect = this.getBounds(this);
    const itemRect = this.getBounds(item);

    this.indicatorEl.hidden = false;
    if (position < 0) {
      this.indicatorEl.style.top = `${itemRect.top - containerRect.top}px`;
    } else {
      this.indicatorEl.style.top = `${itemRect.bottom - containerRect.top}px`;
    }
  }

  onDragLeave(event) {
    if (!event.target.matches(this.itemSelector)) {
      return;
    }
    let target = event.relatedTarget;
    while (target && target !== this) {
      target = target.parentNode;
    }
    if (target !== this) {
      this.indicatorEl.hidden = true;
    }
  }

  onDrop(event) {
    this.#dropTargetInfo = this.getDropTargetInfo(event);
    if (!this.#draggedElement || !this.#dropTargetInfo) {
      return;
    }

    // Don't emit the reorder event if the dragged element is dropped on itself
    if (this.#draggedElement === this.#dropTargetInfo.targetElement) {
      this.onDragEnd();
      return;
    }

    // Don't emit the reorder event if inserting after the previous element
    // or before the next element (no actual reordering needed)
    const draggedIndex = this.getItemIndex(this.#draggedElement);
    const targetIndex = this.#dropTargetInfo.targetIndex;
    const position = this.#dropTargetInfo.position;

    if (
      (position === 0 && targetIndex === draggedIndex - 1) || // Inserting after previous element
      (position === -1 && targetIndex === draggedIndex + 1) // Inserting before next element
    ) {
      this.onDragEnd();
      return;
    }

    event.preventDefault();
    event.stopPropagation();
    this.emitEvent(REORDER_EVENT, {
      draggedElement: this.#draggedElement,
      targetElement: this.#dropTargetInfo.targetElement,
      position: this.#dropTargetInfo.position,
      draggedIndex,
      targetIndex,
    });
    this.onDragEnd();
  }

  onDragEnd() {
    // Sometimes dragend is not fired when the element is dropped. To ensure that
    // we clean up, onDragEnd is also called from onDrop; so it might be called
    // multiple times.
    if (this.#draggedElement == null) {
      return;
    }
    this.emitEvent(DRAGEND_EVENT, {
      draggedElement: this.#draggedElement,
    });
    this.indicatorEl.hidden = true;
    this.#draggedElement = null;
  }

  evaluateKeyDownEvent(event) {
    const direction = isReorderKeyboardEvent(event);
    if (direction == 0) {
      return undefined;
    }
    const fromEl = this.getTargetItemFromEvent(event);
    if (!fromEl) {
      return undefined;
    }
    const fromIndex = this.getItemIndex(fromEl);
    if (fromIndex === -1) {
      return undefined;
    }

    // if index is 0 and direction is -1, or index is last and direction is 1, do nothing
    const items = this.#items;
    if (
      (fromIndex === 0 && direction === -1) ||
      (fromIndex === items.length - 1 && direction === 1)
    ) {
      return undefined;
    }

    return {
      draggedElement: fromEl,
      targetElement: items[fromIndex + direction],
      position: Math.min(direction, 0),
    };
  }

  /**
   * Creates a CustomEvent and dispatches it on the element.
   *
   * @param {string} eventName The name of the event
   * @param {Object} [detail] The detail object to pass to the event
   */
  emitEvent(eventName, detail) {
    const customEvent = new CustomEvent(eventName, {
      detail,
    });
    this.dispatchEvent(customEvent);
  }

  /**
   * Returns all draggable items based on the itemSelector
   *
   * @see getAssignedElementsBySelector for parameters
   */
  getItems() {
    let items = this.getAssignedElementsBySelector(this.itemSelector);
    items.forEach((item, i) => (item[REORDER_PROP] = i));
    this.#items = items;
  }

  /**
   * Returns all elements for the given selector, including the elements
   * themselves, matching the selector, regardless of nesting
   *
   * @param {string} selector The selector to match
   * @param {HTMLElement | HTMLElement[]} [root] The elements to start
   *   searching for items. Defaults to the slot.
   */
  getAssignedElementsBySelector(selector, root) {
    if (!root) {
      root = this.slotEl.assignedElements();
    } else if (!Array.isArray(root)) {
      root = [root];
    }

    return root.reduce((acc, item) => {
      if (item.matches(selector)) {
        acc.push(item);
      } else {
        acc.push(...item.querySelectorAll(selector));
      }
      return acc;
    }, []);
  }

  /**
   * Returns the drop target based on the current mouse position relative to
   * the item it hovers over
   */
  getDropTargetInfo(event) {
    const targetItem = this.getTargetItemFromEvent(event);
    if (!targetItem) {
      return null;
    }

    const targetIndex = this.getItemIndex(targetItem);
    if (targetIndex === -1) {
      return null;
    }

    const rect = targetItem.getBoundingClientRect();

    const threshold = rect.height * 0.5;
    const position = event.clientY < rect.top + threshold ? -1 : 0;
    return {
      targetElement: targetItem,
      targetIndex,
      position,
    };
  }

  /**
   * Returns the index of the given item element out of all items within the
   * slot
   */
  getItemIndex(item) {
    return item[REORDER_PROP] ?? -1;
  }

  /**
   * Returns the item element that is the closest parent of the given event
   * target
   */
  getTargetItemFromEvent(event) {
    const target = event.target;
    const targetItem = target.closest(this.itemSelector);
    return targetItem;
  }

  render() {
    return html`
      <link
        rel="stylesheet"
        href="chrome://global/content/elements/moz-reorderable-list.css"
      />
      <div class="indicator" hidden="" aria-hidden="true"></div>
      <slot @slotchange=${this.getItems}></slot>
    `;
  }
}

/**
 * Checks if the given keyboard event is a reorder keyboard event
 * (ctrl+shift+up/down).
 *
 * Can be used instead of the automatic reorder keyboard event handling by the
 * moz-reorderable-list component.
 *
 * @param {KeyboardEvent} event - The keyboard event to check
 * @returns {0 | -1 | 1} - 0 if the event is not a reorder keyboard event, -1
 *   if the event is a reorder up event, 1 if the event is a reorder down
 *   event
 */
export function isReorderKeyboardEvent(event) {
  if (event.code != "ArrowUp" && event.code != "ArrowDown") {
    return 0;
  }
  if (!event.ctrlKey || !event.shiftKey || event.altKey || event.metaKey) {
    return 0;
  }
  return event.code == "ArrowUp" ? -1 : 1;
}

customElements.define("moz-reorderable-list", MozReorderableList);
