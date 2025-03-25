/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-disable mozilla/no-aArgs */

const DBG_STRINGS_URI = "devtools/client/locales/debugger.properties";
const LAZY_EMPTY_DELAY = 150; // ms
const SCROLL_PAGE_SIZE_DEFAULT = 0;
const PAGE_SIZE_SCROLL_HEIGHT_RATIO = 100;
const PAGE_SIZE_MAX_JUMPS = 30;
const SEARCH_ACTION_MAX_DELAY = 300; // ms

import { require } from "resource://devtools/shared/loader/Loader.sys.mjs";

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const EventEmitter = require("resource://devtools/shared/event-emitter.js");
const DevToolsUtils = require("resource://devtools/shared/DevToolsUtils.js");
const {
  getSourceNames,
} = require("resource://devtools/client/shared/source-utils.js");
const { extend } = require("resource://devtools/shared/extend.js");
const {
  ViewHelpers,
  setNamedTimeout,
} = require("resource://devtools/client/shared/widgets/view-helpers.js");
const nodeConstants = require("resource://devtools/shared/dom-node-constants.js");
const { KeyCodes } = require("resource://devtools/client/shared/keycodes.js");
const { PluralForm } = require("resource://devtools/shared/plural-form.js");
const {
  LocalizationHelper,
  ELLIPSIS,
} = require("resource://devtools/shared/l10n.js");

const L10N = new LocalizationHelper(DBG_STRINGS_URI);
const HTML_NS = "http://www.w3.org/1999/xhtml";

const lazy = {};

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "clipboardHelper",
  "@mozilla.org/widget/clipboardhelper;1",
  "nsIClipboardHelper"
);

/**
 * A tree view for inspecting scopes, objects and properties.
 * Iterable via "for (let [id, scope] of instance) { }".
 * Requires the devtools common.css and debugger.css skin stylesheets.
 *
 * @param Node aParentNode
 *        The parent node to hold this view.
 * @param object aFlags [optional]
 *        An object contaning initialization options for this view.
 *        e.g. { lazyEmpty: true, searchEnabled: true ... }
 */
export function VariablesView(aParentNode, aFlags = {}) {
  this._store = []; // Can't use a Map because Scope names needn't be unique.
  this._itemsByElement = new WeakMap();

  // Note: The hierarchy is only used for an assertion in a test at the moment,
  // to easily check the tree structure.
  this._testOnlyHierarchy = new Map();

  this._parent = aParentNode;
  this._parent.classList.add("variables-view-container");
  this._parent.classList.add("theme-body");
  this._appendEmptyNotice();

  this._onSearchboxInput = this._onSearchboxInput.bind(this);
  this._onSearchboxKeyDown = this._onSearchboxKeyDown.bind(this);
  this._onViewKeyDown = this._onViewKeyDown.bind(this);

  // Create an internal scrollbox container.
  this._list = this.document.createXULElement("scrollbox");
  this._list.setAttribute("orient", "vertical");
  this._list.addEventListener("keydown", this._onViewKeyDown);
  this._parent.appendChild(this._list);

  for (const name in aFlags) {
    this[name] = aFlags[name];
  }

  EventEmitter.decorate(this);
}

VariablesView.prototype = {
  /**
   * Helper setter for populating this container with a raw object.
   *
   * @param object aObject
   *        The raw object to display. You can only provide this object
   *        if you want the variables view to work in sync mode.
   */
  set rawObject(aObject) {
    this.empty();
    this.addScope()
      .addItem(undefined, { enumerable: true })
      .populate(aObject, { sorted: true });
  },

  /**
   * Adds a scope to contain any inspected variables.
   *
   * This new scope will be considered the parent of any other scope
   * added afterwards.
   *
   * @param string l10nId
   *        The scope localized string id.
   * @param string aCustomClass
   *        An additional class name for the containing element.
   * @return Scope
   *         The newly created Scope instance.
   */
  addScope(l10nId = "", aCustomClass = "") {
    this._removeEmptyNotice();
    this._toggleSearchVisibility(true);

    const scope = new Scope(this, l10nId, { customClass: aCustomClass });
    this._store.push(scope);
    this._itemsByElement.set(scope._target, scope);
    this._testOnlyHierarchy.set(l10nId, scope);
    scope.header = !!l10nId;

    return scope;
  },

  /**
   * Removes all items from this container.
   *
   * @param number aTimeout [optional]
   *        The number of milliseconds to delay the operation if
   *        lazy emptying of this container is enabled.
   */
  empty(aTimeout = this.lazyEmptyDelay) {
    // If there are no items in this container, emptying is useless.
    if (!this._store.length) {
      return;
    }

    this._store.length = 0;
    this._itemsByElement = new WeakMap();
    this._testOnlyHierarchy = new Map();

    // Check if this empty operation may be executed lazily.
    if (this.lazyEmpty && aTimeout > 0) {
      this._emptySoon(aTimeout);
      return;
    }

    while (this._list.hasChildNodes()) {
      this._list.firstChild.remove();
    }

    this._appendEmptyNotice();
    this._toggleSearchVisibility(false);
  },

  /**
   * Emptying this container and rebuilding it immediately afterwards would
   * result in a brief redraw flicker, because the previously expanded nodes
   * may get asynchronously re-expanded, after fetching the prototype and
   * properties from a server.
   *
   * To avoid such behaviour, a normal container list is rebuild, but not
   * immediately attached to the parent container. The old container list
   * is kept around for a short period of time, hopefully accounting for the
   * data fetching delay. In the meantime, any operations can be executed
   * normally.
   *
   * @see VariablesView.empty
   */
  _emptySoon(aTimeout) {
    const prevList = this._list;
    const currList = (this._list = this.document.createXULElement("scrollbox"));

    this.window.setTimeout(() => {
      prevList.removeEventListener("keydown", this._onViewKeyDown);
      currList.addEventListener("keydown", this._onViewKeyDown);
      currList.setAttribute("orient", "vertical");

      this._parent.removeChild(prevList);
      this._parent.appendChild(currList);

      if (!this._store.length) {
        this._appendEmptyNotice();
        this._toggleSearchVisibility(false);
      }
    }, aTimeout);
  },

  /**
   * The amount of time (in milliseconds) it takes to empty this view lazily.
   */
  lazyEmptyDelay: LAZY_EMPTY_DELAY,

  /**
   * Specifies if this view may be emptied lazily.
   * @see VariablesView.prototype.empty
   */
  lazyEmpty: false,

  /**
   * The number of elements in this container to jump when Page Up or Page Down
   * keys are pressed. If falsy, then the page size will be based on the
   * container height.
   */
  scrollPageSize: SCROLL_PAGE_SIZE_DEFAULT,

  /**
   * Specifies the context menu attribute set on variables and properties.
   *
   * This flag is applied recursively onto each scope in this view and
   * affects only the child nodes when they're created.
   */
  contextMenuId: "",

  /**
   * The separator label between the variables or properties name and value.
   *
   * This flag is applied recursively onto each scope in this view and
   * affects only the child nodes when they're created.
   */
  separatorStr: L10N.getStr("variablesSeparatorLabel"),

  /**
   * Specifies if enumerable properties and variables should be displayed.
   * These variables and properties are visible by default.
   * @param boolean aFlag
   */
  set enumVisible(aFlag) {
    this._enumVisible = aFlag;

    for (const scope of this._store) {
      scope._enumVisible = aFlag;
    }
  },

  /**
   * Specifies if non-enumerable properties and variables should be displayed.
   * These variables and properties are visible by default.
   * @param boolean aFlag
   */
  set nonEnumVisible(aFlag) {
    this._nonEnumVisible = aFlag;

    for (const scope of this._store) {
      scope._nonEnumVisible = aFlag;
    }
  },

  /**
   * Specifies if only enumerable properties and variables should be displayed.
   * Both types of these variables and properties are visible by default.
   * @param boolean aFlag
   */
  set onlyEnumVisible(aFlag) {
    if (aFlag) {
      this.enumVisible = true;
      this.nonEnumVisible = false;
    } else {
      this.enumVisible = true;
      this.nonEnumVisible = true;
    }
  },

  /**
   * Sets if the variable and property searching is enabled.
   * @param boolean aFlag
   */
  set searchEnabled(aFlag) {
    aFlag ? this._enableSearch() : this._disableSearch();
  },

  /**
   * Gets if the variable and property searching is enabled.
   * @return boolean
   */
  get searchEnabled() {
    return !!this._searchboxContainer;
  },

  /**
   * Enables variable and property searching in this view.
   * Use the "searchEnabled" setter to enable searching.
   */
  _enableSearch() {
    // If searching was already enabled, no need to re-enable it again.
    if (this._searchboxContainer) {
      return;
    }
    const document = this.document;
    const ownerNode = this._parent.parentNode;

    const container = (this._searchboxContainer =
      document.createXULElement("hbox"));
    container.className = "devtools-toolbar devtools-input-toolbar";

    // Hide the variables searchbox container if there are no variables or
    // properties to display.
    container.hidden = !this._store.length;

    const searchbox = (this._searchboxNode = document.createElementNS(
      HTML_NS,
      "input"
    ));
    searchbox.className = "variables-view-searchinput devtools-filterinput";
    document.l10n.setAttributes(searchbox, "storage-variable-view-search-box");
    searchbox.addEventListener("input", this._onSearchboxInput);
    searchbox.addEventListener("keydown", this._onSearchboxKeyDown);

    container.appendChild(searchbox);
    ownerNode.insertBefore(container, this._parent);
  },

  /**
   * Disables variable and property searching in this view.
   * Use the "searchEnabled" setter to disable searching.
   */
  _disableSearch() {
    // If searching was already disabled, no need to re-disable it again.
    if (!this._searchboxContainer) {
      return;
    }
    this._searchboxContainer.remove();
    this._searchboxNode.removeEventListener("input", this._onSearchboxInput);
    this._searchboxNode.removeEventListener(
      "keydown",
      this._onSearchboxKeyDown
    );

    this._searchboxContainer = null;
    this._searchboxNode = null;
  },

  /**
   * Sets the variables searchbox container hidden or visible.
   * It's hidden by default.
   *
   * @param boolean aVisibleFlag
   *        Specifies the intended visibility.
   */
  _toggleSearchVisibility(aVisibleFlag) {
    // If searching was already disabled, there's no need to hide it.
    if (!this._searchboxContainer) {
      return;
    }
    this._searchboxContainer.hidden = !aVisibleFlag;
  },

  /**
   * Listener handling the searchbox input event.
   */
  _onSearchboxInput() {
    this.scheduleSearch(this._searchboxNode.value);
  },

  /**
   * Listener handling the searchbox keydown event.
   */
  _onSearchboxKeyDown(e) {
    switch (e.keyCode) {
      case KeyCodes.DOM_VK_RETURN:
        this._onSearchboxInput();
        return;
      case KeyCodes.DOM_VK_ESCAPE:
        this._searchboxNode.value = "";
        this._onSearchboxInput();
    }
  },

  /**
   * Schedules searching for variables or properties matching the query.
   *
   * @param string aToken
   *        The variable or property to search for.
   * @param number aWait
   *        The amount of milliseconds to wait until draining.
   */
  scheduleSearch(aToken, aWait) {
    // The amount of time to wait for the requests to settle.
    const maxDelay = SEARCH_ACTION_MAX_DELAY;
    const delay = aWait === undefined ? maxDelay / aToken.length : aWait;

    // Allow requests to settle down first.
    setNamedTimeout("vview-search", delay, () => this._doSearch(aToken));
  },

  /**
   * Performs a case insensitive search for variables or properties matching
   * the query, and hides non-matched items.
   *
   * If aToken is falsy, then all the scopes are unhidden and expanded,
   * while the available variables and properties inside those scopes are
   * just unhidden.
   *
   * @param string aToken
   *        The variable or property to search for.
   */
  _doSearch(aToken) {
    for (const scope of this._store) {
      switch (aToken) {
        case "":
        case null:
        case undefined:
          scope.expand();
          scope._performSearch("");
          break;
        default:
          scope._performSearch(aToken.toLowerCase());
          break;
      }
    }
  },

  /**
   * Find the first item in the tree of visible items in this container that
   * matches the predicate. Searches in visual order (the order seen by the
   * user). Descends into each scope to check the scope and its children.
   *
   * @param function aPredicate
   *        A function that returns true when a match is found.
   * @return Scope | Variable | Property
   *         The first visible scope, variable or property, or null if nothing
   *         is found.
   */
  _findInVisibleItems(aPredicate) {
    for (const scope of this._store) {
      const result = scope._findInVisibleItems(aPredicate);
      if (result) {
        return result;
      }
    }
    return null;
  },

  /**
   * Find the last item in the tree of visible items in this container that
   * matches the predicate. Searches in reverse visual order (opposite of the
   * order seen by the user). Descends into each scope to check the scope and
   * its children.
   *
   * @param function aPredicate
   *        A function that returns true when a match is found.
   * @return Scope | Variable | Property
   *         The last visible scope, variable or property, or null if nothing
   *         is found.
   */
  _findInVisibleItemsReverse(aPredicate) {
    for (let i = this._store.length - 1; i >= 0; i--) {
      const scope = this._store[i];
      const result = scope._findInVisibleItemsReverse(aPredicate);
      if (result) {
        return result;
      }
    }
    return null;
  },

  /**
   * Gets the scope at the specified index.
   *
   * @param number aIndex
   *        The scope's index.
   * @return Scope
   *         The scope if found, undefined if not.
   */
  getScopeAtIndex(aIndex) {
    return this._store[aIndex];
  },

  /**
   * Recursively searches this container for the scope, variable or property
   * displayed by the specified node.
   *
   * @param Node aNode
   *        The node to search for.
   * @return Scope | Variable | Property
   *         The matched scope, variable or property, or null if nothing is found.
   */
  getItemForNode(aNode) {
    return this._itemsByElement.get(aNode);
  },

  /**
   * Gets the scope owning a Variable or Property.
   *
   * @param Variable | Property
   *        The variable or property to retrieven the owner scope for.
   * @return Scope
   *         The owner scope.
   */
  getOwnerScopeForVariableOrProperty(aItem) {
    if (!aItem) {
      return null;
    }
    // If this is a Scope, return it.
    if (!(aItem instanceof Variable)) {
      return aItem;
    }
    // If this is a Variable or Property, find its owner scope.
    if (aItem instanceof Variable && aItem.ownerView) {
      return this.getOwnerScopeForVariableOrProperty(aItem.ownerView);
    }
    return null;
  },

  /**
   * Gets the parent scopes for a specified Variable or Property.
   * The returned list will not include the owner scope.
   *
   * @param Variable | Property
   *        The variable or property for which to find the parent scopes.
   * @return array
   *         A list of parent Scopes.
   */
  getParentScopesForVariableOrProperty(aItem) {
    const scope = this.getOwnerScopeForVariableOrProperty(aItem);
    return this._store.slice(0, Math.max(this._store.indexOf(scope), 0));
  },

  /**
   * Gets the currently focused scope, variable or property in this view.
   *
   * @return Scope | Variable | Property
   *         The focused scope, variable or property, or null if nothing is found.
   */
  getFocusedItem() {
    const focused = this.document.commandDispatcher.focusedElement;
    return this.getItemForNode(focused);
  },

  /**
   * Focuses the first visible scope, variable, or property in this container.
   */
  focusFirstVisibleItem() {
    const focusableItem = this._findInVisibleItems(item => item.focusable);
    if (focusableItem) {
      this._focusItem(focusableItem);
    }
    this._parent.scrollTop = 0;
    this._parent.scrollLeft = 0;
  },

  /**
   * Focuses the last visible scope, variable, or property in this container.
   */
  focusLastVisibleItem() {
    const focusableItem = this._findInVisibleItemsReverse(
      item => item.focusable
    );
    if (focusableItem) {
      this._focusItem(focusableItem);
    }
    this._parent.scrollTop = this._parent.scrollHeight;
    this._parent.scrollLeft = 0;
  },

  /**
   * Focuses the next scope, variable or property in this view.
   */
  focusNextItem() {
    this.focusItemAtDelta(+1);
  },

  /**
   * Focuses the previous scope, variable or property in this view.
   */
  focusPrevItem() {
    this.focusItemAtDelta(-1);
  },

  /**
   * Focuses another scope, variable or property in this view, based on
   * the index distance from the currently focused item.
   *
   * @param number aDelta
   *        A scalar specifying by how many items should the selection change.
   */
  focusItemAtDelta(aDelta) {
    const direction = aDelta > 0 ? "advanceFocus" : "rewindFocus";
    let distance = Math.abs(Math[aDelta > 0 ? "ceil" : "floor"](aDelta));
    while (distance--) {
      if (!this._focusChange(direction)) {
        break; // Out of bounds.
      }
    }
  },

  /**
   * Focuses the next or previous scope, variable or property in this view.
   *
   * @param string aDirection
   *        Either "advanceFocus" or "rewindFocus".
   * @return boolean
   *         False if the focus went out of bounds and the first or last element
   *         in this view was focused instead.
   */
  _focusChange(aDirection) {
    const commandDispatcher = this.document.commandDispatcher;
    const prevFocusedElement = commandDispatcher.focusedElement;
    let currFocusedItem = null;

    do {
      commandDispatcher[aDirection]();

      // Make sure the newly focused item is a part of this view.
      // If the focus goes out of bounds, revert the previously focused item.
      if (!(currFocusedItem = this.getFocusedItem())) {
        prevFocusedElement.focus();
        return false;
      }
    } while (!currFocusedItem.focusable);

    // Focus remained within bounds.
    return true;
  },

  /**
   * Focuses a scope, variable or property and makes sure it's visible.
   *
   * @param aItem Scope | Variable | Property
   *        The item to focus.
   * @param boolean aCollapseFlag
   *        True if the focused item should also be collapsed.
   * @return boolean
   *         True if the item was successfully focused.
   */
  _focusItem(aItem, aCollapseFlag) {
    if (!aItem.focusable) {
      return false;
    }
    if (aCollapseFlag) {
      aItem.collapse();
    }
    aItem._target.focus();
    aItem._arrow.scrollIntoView({ block: "nearest" });
    return true;
  },

  /**
   * Copy current selection to clipboard.
   */
  _copyItem() {
    const item = this.getFocusedItem();
    lazy.clipboardHelper.copyString(
      item._nameString + item.separatorStr + item._valueString
    );
  },

  /**
   * Listener handling a key down event on the view.
   */
  // eslint-disable-next-line complexity
  _onViewKeyDown(e) {
    const item = this.getFocusedItem();

    // Prevent scrolling when pressing navigation keys.
    ViewHelpers.preventScrolling(e);

    switch (e.keyCode) {
      case KeyCodes.DOM_VK_C:
        if (e.ctrlKey || e.metaKey) {
          this._copyItem();
        }
        return;

      case KeyCodes.DOM_VK_UP:
        // Always rewind focus.
        this.focusPrevItem(true);
        return;

      case KeyCodes.DOM_VK_DOWN:
        // Always advance focus.
        this.focusNextItem(true);
        return;

      case KeyCodes.DOM_VK_LEFT:
        // Collapse scopes, variables and properties before rewinding focus.
        if (item._isExpanded && item._isArrowVisible) {
          item.collapse();
        } else {
          this._focusItem(item.ownerView);
        }
        return;

      case KeyCodes.DOM_VK_RIGHT:
        // Nothing to do here if this item never expands.
        if (!item._isArrowVisible) {
          return;
        }
        // Expand scopes, variables and properties before advancing focus.
        if (!item._isExpanded) {
          item.expand();
        } else {
          this.focusNextItem(true);
        }
        return;

      case KeyCodes.DOM_VK_PAGE_UP:
        // Rewind a certain number of elements based on the container height.
        this.focusItemAtDelta(
          -(
            this.scrollPageSize ||
            Math.min(
              Math.floor(
                this._list.scrollHeight / PAGE_SIZE_SCROLL_HEIGHT_RATIO
              ),
              PAGE_SIZE_MAX_JUMPS
            )
          )
        );
        return;

      case KeyCodes.DOM_VK_PAGE_DOWN:
        // Advance a certain number of elements based on the container height.
        this.focusItemAtDelta(
          +(
            this.scrollPageSize ||
            Math.min(
              Math.floor(
                this._list.scrollHeight / PAGE_SIZE_SCROLL_HEIGHT_RATIO
              ),
              PAGE_SIZE_MAX_JUMPS
            )
          )
        );
        return;

      case KeyCodes.DOM_VK_HOME:
        this.focusFirstVisibleItem();
        return;

      case KeyCodes.DOM_VK_END:
        this.focusLastVisibleItem();
    }
  },

  /**
   * Sets the text displayed in this container when there are no available items.
   * @param string aValue
   */
  set emptyText(aValue) {
    if (this._emptyTextNode) {
      this._emptyTextNode.setAttribute("value", aValue);
    }
    this._emptyTextValue = aValue;
    this._appendEmptyNotice();
  },

  /**
   * Creates and appends a label signaling that this container is empty.
   */
  _appendEmptyNotice() {
    if (this._emptyTextNode || !this._emptyTextValue) {
      return;
    }

    const label = this.document.createXULElement("label");
    label.className = "variables-view-empty-notice";
    label.setAttribute("value", this._emptyTextValue);

    this._parent.appendChild(label);
    this._emptyTextNode = label;
  },

  /**
   * Removes the label signaling that this container is empty.
   */
  _removeEmptyNotice() {
    if (!this._emptyTextNode) {
      return;
    }

    this._parent.removeChild(this._emptyTextNode);
    this._emptyTextNode = null;
  },

  /**
   * Gets if all values should be aligned together.
   * @return boolean
   */
  get alignedValues() {
    return this._alignedValues;
  },

  /**
   * Sets if all values should be aligned together.
   * @param boolean aFlag
   */
  set alignedValues(aFlag) {
    this._alignedValues = aFlag;
    if (aFlag) {
      this._parent.setAttribute("aligned-values", "");
    } else {
      this._parent.removeAttribute("aligned-values");
    }
  },

  /**
   * Gets if action buttons (like delete) should be placed at the beginning or
   * end of a line.
   * @return boolean
   */
  get actionsFirst() {
    return this._actionsFirst;
  },

  /**
   * Sets if action buttons (like delete) should be placed at the beginning or
   * end of a line.
   * @param boolean aFlag
   */
  set actionsFirst(aFlag) {
    this._actionsFirst = aFlag;
    if (aFlag) {
      this._parent.setAttribute("actions-first", "");
    } else {
      this._parent.removeAttribute("actions-first");
    }
  },

  /**
   * Gets the parent node holding this view.
   * @return Node
   */
  get parentNode() {
    return this._parent;
  },

  /**
   * Gets the owner document holding this view.
   * @return HTMLDocument
   */
  get document() {
    return this._document || (this._document = this._parent.ownerDocument);
  },

  /**
   * Gets the default window holding this view.
   * @return nsIDOMWindow
   */
  get window() {
    return this._window || (this._window = this.document.defaultView);
  },

  _document: null,
  _window: null,

  _store: null,
  _itemsByElement: null,
  _testOnlyHierarchy: null,

  _enumVisible: true,
  _nonEnumVisible: true,
  _alignedValues: false,
  _actionsFirst: false,

  _parent: null,
  _list: null,
  _searchboxNode: null,
  _searchboxContainer: null,
  _emptyTextNode: null,
  _emptyTextValue: "",
};

VariablesView.NON_SORTABLE_CLASSES = [
  "Array",
  "Int8Array",
  "Uint8Array",
  "Uint8ClampedArray",
  "Int16Array",
  "Uint16Array",
  "Int32Array",
  "Uint32Array",
  "Float32Array",
  "Float64Array",
  "NodeList",
];

/**
 * Determine whether an object's properties should be sorted based on its class.
 *
 * @param string aClassName
 *        The class of the object.
 */
VariablesView.isSortable = function (aClassName) {
  return !VariablesView.NON_SORTABLE_CLASSES.includes(aClassName);
};

/**
 * A Scope is an object holding Variable instances.
 * Iterable via "for (let [name, variable] of instance) { }".
 *
 * @param VariablesView aView
 *        The view to contain this scope.
 * @param string l10nId
 *        The scope localized string id.
 * @param object aFlags [optional]
 *        Additional options or flags for this scope.
 */
function Scope(aView, l10nId, aFlags = {}) {
  this.ownerView = aView;

  this._onClick = this._onClick.bind(this);
  this._openEnum = this._openEnum.bind(this);
  this._openNonEnum = this._openNonEnum.bind(this);

  // Inherit properties and flags from the parent view. You can override
  // each of these directly onto any scope, variable or property instance.
  this.scrollPageSize = aView.scrollPageSize;
  this.contextMenuId = aView.contextMenuId;
  this.separatorStr = aView.separatorStr;

  this._init(l10nId, aFlags);
}

Scope.prototype = {
  /**
   * Whether this Scope should be prefetched when it is remoted.
   */
  shouldPrefetch: true,

  /**
   * Whether this Scope should paginate its contents.
   */
  allowPaginate: false,

  /**
   * The class name applied to this scope's target element.
   */
  targetClassName: "variables-view-scope",

  /**
   * Create a new Variable that is a child of this Scope.
   *
   * @param string aName
   *        The name of the new Property.
   * @param object aDescriptor
   *        The variable's descriptor.
   * @param object aOptions
   *        Options of the form accepted by addItem.
   * @return Variable
   *         The newly created child Variable.
   */
  _createChild(aName, aDescriptor, aOptions) {
    return new Variable(this, aName, aDescriptor, aOptions);
  },

  /**
   * Adds a child to contain any inspected properties.
   *
   * @param string aName
   *        The child's name.
   * @param object aDescriptor
   *        Specifies the value and/or type & class of the child,
   *        or 'get' & 'set' accessor properties. If the type is implicit,
   *        it will be inferred from the value. If this parameter is omitted,
   *        a property without a value will be added (useful for branch nodes).
   *        e.g. - { value: 42 }
   *             - { value: true }
   *             - { value: "nasu" }
   *             - { value: { type: "undefined" } }
   *             - { value: { type: "null" } }
   *             - { value: { type: "object", class: "Object" } }
   *             - { get: { type: "object", class: "Function" },
   *                 set: { type: "undefined" } }
   * @param object aOptions
   *        Specifies some options affecting the new variable.
   *        Recognized properties are
   *        * boolean relaxed  true if name duplicates should be allowed.
   *                           You probably shouldn't do it. Use this
   *                           with caution.
   *        * boolean internalItem  true if the item is internally generated.
   *                           This is used for special variables
   *                           like <return> or <exception> and distinguishes
   *                           them from ordinary properties that happen
   *                           to have the same name
   * @return Variable
   *         The newly created Variable instance, null if it already exists.
   */
  addItem(aName, aDescriptor = {}, aOptions = {}) {
    const { relaxed } = aOptions;
    if (this._store.has(aName) && !relaxed) {
      return this._store.get(aName);
    }

    const child = this._createChild(aName, aDescriptor, aOptions);
    this._store.set(aName, child);
    this._variablesView._itemsByElement.set(child._target, child);
    this._variablesView._testOnlyHierarchy.set(child.absoluteName, child);
    child.header = aName !== undefined;

    return child;
  },

  /**
   * Adds items for this variable.
   *
   * @param object aItems
   *        An object containing some { name: descriptor } data properties,
   *        specifying the value and/or type & class of the variable,
   *        or 'get' & 'set' accessor properties. If the type is implicit,
   *        it will be inferred from the value.
   *        e.g. - { someProp0: { value: 42 },
   *                 someProp1: { value: true },
   *                 someProp2: { value: "nasu" },
   *                 someProp3: { value: { type: "undefined" } },
   *                 someProp4: { value: { type: "null" } },
   *                 someProp5: { value: { type: "object", class: "Object" } },
   *                 someProp6: { get: { type: "object", class: "Function" },
   *                              set: { type: "undefined" } } }
   * @param object aOptions [optional]
   *        Additional options for adding the properties. Supported options:
   *        - sorted: true to sort all the properties before adding them
   *        - callback: function invoked after each item is added
   */
  addItems(aItems, aOptions = {}) {
    const names = Object.keys(aItems);

    // Sort all of the properties before adding them, if preferred.
    if (aOptions.sorted) {
      names.sort(this._naturalSort);
    }

    // Add the properties to the current scope.
    for (const name of names) {
      const descriptor = aItems[name];
      const item = this.addItem(name, descriptor);

      if (aOptions.callback) {
        aOptions.callback(item, descriptor && descriptor.value);
      }
    }
  },

  /**
   * Remove this Scope from its parent and remove all children recursively.
   */
  remove() {
    const view = this._variablesView;
    view._store.splice(view._store.indexOf(this), 1);
    view._itemsByElement.delete(this._target);
    view._testOnlyHierarchy.delete(this._nameString);

    this._target.remove();

    for (const variable of this._store.values()) {
      variable.remove();
    }
  },

  /**
   * Gets the variable in this container having the specified name.
   *
   * @param string aName
   *        The name of the variable to get.
   * @return Variable
   *         The matched variable, or null if nothing is found.
   */
  get(aName) {
    return this._store.get(aName);
  },

  /**
   * Recursively searches for the variable or property in this container
   * displayed by the specified node.
   *
   * @param Node aNode
   *        The node to search for.
   * @return Variable | Property
   *         The matched variable or property, or null if nothing is found.
   */
  find(aNode) {
    for (const [, variable] of this._store) {
      let match;
      if (variable._target == aNode) {
        match = variable;
      } else {
        match = variable.find(aNode);
      }
      if (match) {
        return match;
      }
    }
    return null;
  },

  /**
   * Determines if this scope is a direct child of a parent variables view,
   * scope, variable or property.
   *
   * @param VariablesView | Scope | Variable | Property
   *        The parent to check.
   * @return boolean
   *         True if the specified item is a direct child, false otherwise.
   */
  isChildOf(aParent) {
    return this.ownerView == aParent;
  },

  /**
   * Determines if this scope is a descendant of a parent variables view,
   * scope, variable or property.
   *
   * @param VariablesView | Scope | Variable | Property
   *        The parent to check.
   * @return boolean
   *         True if the specified item is a descendant, false otherwise.
   */
  isDescendantOf(aParent) {
    if (this.isChildOf(aParent)) {
      return true;
    }

    // Recurse to parent if it is a Scope, Variable, or Property.
    if (this.ownerView instanceof Scope) {
      return this.ownerView.isDescendantOf(aParent);
    }

    return false;
  },

  /**
   * Shows the scope.
   */
  show() {
    this._target.hidden = false;
    this._isContentVisible = true;

    if (this.onshow) {
      this.onshow(this);
    }
  },

  /**
   * Hides the scope.
   */
  hide() {
    this._target.hidden = true;
    this._isContentVisible = false;

    if (this.onhide) {
      this.onhide(this);
    }
  },

  /**
   * Expands the scope, showing all the added details.
   */
  async expand() {
    if (this._isExpanded) {
      return;
    }
    if (this._variablesView._enumVisible) {
      this._openEnum();
    }
    if (this._variablesView._nonEnumVisible) {
      Services.tm.dispatchToMainThread({ run: this._openNonEnum });
    }
    this._isExpanded = true;

    if (this.onexpand) {
      // We return onexpand as it sometimes returns a promise
      // (up to the user of VariableView to do it)
      // that can indicate when the view is done expanding
      // and attributes are available. (Mostly used for tests)
      await this.onexpand(this);
    }
  },

  /**
   * Collapses the scope, hiding all the added details.
   */
  collapse() {
    if (!this._isExpanded) {
      return;
    }
    this._arrow.removeAttribute("open");
    this._enum.removeAttribute("open");
    this._nonenum.removeAttribute("open");
    this._isExpanded = false;

    if (this.oncollapse) {
      this.oncollapse(this);
    }
  },

  /**
   * Toggles between the scope's collapsed and expanded state.
   */
  toggle(e) {
    if (e && e.button != 0) {
      // Only allow left-click to trigger this event.
      return;
    }
    this.expanded ^= 1;

    // Make sure the scope and its contents are visibile.
    for (const [, variable] of this._store) {
      variable.header = true;
      variable._matched = true;
    }
    if (this.ontoggle) {
      this.ontoggle(this);
    }
  },

  /**
   * Shows the scope's title header.
   */
  showHeader() {
    if (this._isHeaderVisible || !this._nameString) {
      return;
    }
    this._target.removeAttribute("untitled");
    this._isHeaderVisible = true;
  },

  /**
   * Hides the scope's title header.
   * This action will automatically expand the scope.
   */
  hideHeader() {
    if (!this._isHeaderVisible) {
      return;
    }
    this.expand();
    this._target.setAttribute("untitled", "");
    this._isHeaderVisible = false;
  },

  /**
   * Sort in ascending order
   * This only needs to compare non-numbers since it is dealing with an array
   * which numeric-based indices are placed in order.
   *
   * @param string a
   * @param string b
   * @return number
   *         -1 if a is less than b, 0 if no change in order, +1 if a is greater than 0
   */
  _naturalSort(a, b) {
    if (isNaN(parseFloat(a)) && isNaN(parseFloat(b))) {
      return a < b ? -1 : 1;
    }
    return 0;
  },

  /**
   * Shows the scope's expand/collapse arrow.
   */
  showArrow() {
    if (this._isArrowVisible) {
      return;
    }
    this._arrow.removeAttribute("invisible");
    this._isArrowVisible = true;
  },

  /**
   * Hides the scope's expand/collapse arrow.
   */
  hideArrow() {
    if (!this._isArrowVisible) {
      return;
    }
    this._arrow.setAttribute("invisible", "");
    this._isArrowVisible = false;
  },

  /**
   * Gets the visibility state.
   * @return boolean
   */
  get visible() {
    return this._isContentVisible;
  },

  /**
   * Gets the expanded state.
   * @return boolean
   */
  get expanded() {
    return this._isExpanded;
  },

  /**
   * Gets the header visibility state.
   * @return boolean
   */
  get header() {
    return this._isHeaderVisible;
  },

  /**
   * Gets the twisty visibility state.
   * @return boolean
   */
  get twisty() {
    return this._isArrowVisible;
  },
  /**
   * Sets the visibility state.
   * @param boolean aFlag
   */
  set visible(aFlag) {
    aFlag ? this.show() : this.hide();
  },

  /**
   * Sets the expanded state.
   * @param boolean aFlag
   */
  set expanded(aFlag) {
    aFlag ? this.expand() : this.collapse();
  },

  /**
   * Sets the header visibility state.
   * @param boolean aFlag
   */
  set header(aFlag) {
    aFlag ? this.showHeader() : this.hideHeader();
  },

  /**
   * Sets the twisty visibility state.
   * @param boolean aFlag
   */
  set twisty(aFlag) {
    aFlag ? this.showArrow() : this.hideArrow();
  },

  /**
   * Specifies if this target node may be focused.
   * @return boolean
   */
  get focusable() {
    // Check if this target node is actually visibile.
    if (
      !this._nameString ||
      !this._isContentVisible ||
      !this._isHeaderVisible ||
      !this._isMatch
    ) {
      return false;
    }
    // Check if all parent objects are expanded.
    let item = this;

    // Recurse while parent is a Scope, Variable, or Property
    while ((item = item.ownerView) && item instanceof Scope) {
      if (!item._isExpanded) {
        return false;
      }
    }
    return true;
  },

  /**
   * Focus this scope.
   */
  focus() {
    this._variablesView._focusItem(this);
  },

  /**
   * Adds an event listener for a certain event on this scope's title.
   * @param string aName
   * @param function aCallback
   * @param boolean aCapture
   */
  addEventListener(aName, aCallback, aCapture) {
    this._title.addEventListener(aName, aCallback, aCapture);
  },

  /**
   * Removes an event listener for a certain event on this scope's title.
   * @param string aName
   * @param function aCallback
   * @param boolean aCapture
   */
  removeEventListener(aName, aCallback, aCapture) {
    this._title.removeEventListener(aName, aCallback, aCapture);
  },

  /**
   * Gets the id associated with this item.
   * @return string
   */
  get id() {
    return this._idString;
  },

  /**
   * Gets the name associated with this item.
   * @return string
   */
  get name() {
    return this._nameString;
  },

  /**
   * Gets the displayed value for this item.
   * @return string
   */
  get displayValue() {
    return this._valueString;
  },

  /**
   * Gets the class names used for the displayed value.
   * @return string
   */
  get displayValueClassName() {
    return this._valueClassName;
  },

  /**
   * Gets the element associated with this item.
   * @return Node
   */
  get target() {
    return this._target;
  },

  /**
   * Initializes this scope's id, view and binds event listeners.
   *
   * @param string l10nId
   *        The scope localized string id.
   * @param object aFlags [optional]
   *        Additional options or flags for this scope.
   */
  _init(l10nId, aFlags) {
    this._idString = generateId((this._nameString = l10nId));
    this._displayScope({
      l10nId,
      targetClassName: `${this.targetClassName} ${aFlags.customClass}`,
      titleClassName: "devtools-toolbar",
    });
    this._addEventListeners();
    this.parentNode.appendChild(this._target);
  },

  /**
   * Creates the necessary nodes for this scope.
   *
   * @param Object options
   * @param string options.l10nId [optional]
   *        The scope localized string id.
   * @param string options.value [optional]
   *        The scope's name. Either this or l10nId need to be passed
   * @param string options.targetClassName
   *        A custom class name for this scope's target element.
   * @param string options.titleClassName [optional]
   *        A custom class name for this scope's title element.
   */
  _displayScope({ l10nId, value, targetClassName, titleClassName = "" }) {
    const document = this.document;

    const element = (this._target = document.createXULElement("vbox"));
    element.id = this._idString;
    element.className = targetClassName;

    const arrow = (this._arrow = document.createXULElement("hbox"));
    arrow.className = "arrow theme-twisty";

    const name = (this._name = document.createXULElement("label"));
    name.className = "name";
    if (l10nId) {
      document.l10n.setAttributes(name, l10nId);
    } else {
      name.setAttribute("value", value);
    }
    name.setAttribute("crop", "end");

    const title = (this._title = document.createXULElement("hbox"));
    title.className = "title " + titleClassName;
    title.setAttribute("align", "center");

    const enumerable = (this._enum = document.createXULElement("vbox"));
    const nonenum = (this._nonenum = document.createXULElement("vbox"));
    enumerable.className = "variables-view-element-details enum";
    nonenum.className = "variables-view-element-details nonenum";

    title.appendChild(arrow);
    title.appendChild(name);

    element.appendChild(title);
    element.appendChild(enumerable);
    element.appendChild(nonenum);
  },

  /**
   * Adds the necessary event listeners for this scope.
   */
  _addEventListeners() {
    this._title.addEventListener("mousedown", this._onClick);
  },

  /**
   * The click listener for this scope's title.
   */
  _onClick(e) {
    if (e.button != 0) {
      return;
    }
    this.toggle();
    this.focus();
  },

  /**
   * Opens the enumerable items container.
   */
  _openEnum() {
    this._arrow.setAttribute("open", "");
    this._enum.setAttribute("open", "");
  },

  /**
   * Opens the non-enumerable items container.
   */
  _openNonEnum() {
    this._nonenum.setAttribute("open", "");
  },

  /**
   * Specifies if enumerable properties and variables should be displayed.
   * @param boolean aFlag
   */
  set _enumVisible(aFlag) {
    for (const [, variable] of this._store) {
      variable._enumVisible = aFlag;

      if (!this._isExpanded) {
        continue;
      }
      if (aFlag) {
        this._enum.setAttribute("open", "");
      } else {
        this._enum.removeAttribute("open");
      }
    }
  },

  /**
   * Specifies if non-enumerable properties and variables should be displayed.
   * @param boolean aFlag
   */
  set _nonEnumVisible(aFlag) {
    for (const [, variable] of this._store) {
      variable._nonEnumVisible = aFlag;

      if (!this._isExpanded) {
        continue;
      }
      if (aFlag) {
        this._nonenum.setAttribute("open", "");
      } else {
        this._nonenum.removeAttribute("open");
      }
    }
  },

  /**
   * Performs a case insensitive search for variables or properties matching
   * the query, and hides non-matched items.
   *
   * @param string aLowerCaseQuery
   *        The lowercased name of the variable or property to search for.
   */
  _performSearch(aLowerCaseQuery) {
    for (let [, variable] of this._store) {
      const currentObject = variable;
      const lowerCaseName = variable._nameString.toLowerCase();
      const lowerCaseValue = variable._valueString.toLowerCase();

      // Non-matched variables or properties require a corresponding attribute.
      if (
        !lowerCaseName.includes(aLowerCaseQuery) &&
        !lowerCaseValue.includes(aLowerCaseQuery)
      ) {
        variable._matched = false;
      } else {
        // Variable or property is matched.
        variable._matched = true;

        // If the variable was ever expanded, there's a possibility it may
        // contain some matched properties, so make sure they're visible
        // ("expand downwards").
        if (variable._store.size) {
          variable.expand();
        }

        // If the variable is contained in another Scope, Variable, or Property,
        // the parent may not be a match, thus hidden. It should be visible
        // ("expand upwards").
        while ((variable = variable.ownerView) && variable instanceof Scope) {
          variable._matched = true;
          variable.expand();
        }
      }

      // Proceed with the search recursively inside this variable or property.
      if (
        currentObject._store.size ||
        currentObject.getter ||
        currentObject.setter
      ) {
        currentObject._performSearch(aLowerCaseQuery);
      }
    }
  },

  /**
   * Sets if this object instance is a matched or non-matched item.
   * @param boolean aStatus
   */
  set _matched(aStatus) {
    if (this._isMatch == aStatus) {
      return;
    }
    if (aStatus) {
      this._isMatch = true;
      this.target.removeAttribute("unmatched");
    } else {
      this._isMatch = false;
      this.target.setAttribute("unmatched", "");
    }
  },

  /**
   * Find the first item in the tree of visible items in this item that matches
   * the predicate. Searches in visual order (the order seen by the user).
   * Tests itself, then descends into first the enumerable children and then
   * the non-enumerable children (since they are presented in separate groups).
   *
   * @param function aPredicate
   *        A function that returns true when a match is found.
   * @return Scope | Variable | Property
   *         The first visible scope, variable or property, or null if nothing
   *         is found.
   */
  _findInVisibleItems(aPredicate) {
    if (aPredicate(this)) {
      return this;
    }

    if (this._isExpanded) {
      if (this._variablesView._enumVisible) {
        for (const item of this._enumItems) {
          const result = item._findInVisibleItems(aPredicate);
          if (result) {
            return result;
          }
        }
      }

      if (this._variablesView._nonEnumVisible) {
        for (const item of this._nonEnumItems) {
          const result = item._findInVisibleItems(aPredicate);
          if (result) {
            return result;
          }
        }
      }
    }

    return null;
  },

  /**
   * Find the last item in the tree of visible items in this item that matches
   * the predicate. Searches in reverse visual order (opposite of the order
   * seen by the user). Descends into first the non-enumerable children, then
   * the enumerable children (since they are presented in separate groups), and
   * finally tests itself.
   *
   * @param function aPredicate
   *        A function that returns true when a match is found.
   * @return Scope | Variable | Property
   *         The last visible scope, variable or property, or null if nothing
   *         is found.
   */
  _findInVisibleItemsReverse(aPredicate) {
    if (this._isExpanded) {
      if (this._variablesView._nonEnumVisible) {
        for (let i = this._nonEnumItems.length - 1; i >= 0; i--) {
          const item = this._nonEnumItems[i];
          const result = item._findInVisibleItemsReverse(aPredicate);
          if (result) {
            return result;
          }
        }
      }

      if (this._variablesView._enumVisible) {
        for (let i = this._enumItems.length - 1; i >= 0; i--) {
          const item = this._enumItems[i];
          const result = item._findInVisibleItemsReverse(aPredicate);
          if (result) {
            return result;
          }
        }
      }
    }

    if (aPredicate(this)) {
      return this;
    }

    return null;
  },

  /**
   * Gets top level variables view instance.
   * @return VariablesView
   */
  get _variablesView() {
    return (
      this._topView ||
      (this._topView = (() => {
        let parentView = this.ownerView;
        let topView;

        while ((topView = parentView.ownerView)) {
          parentView = topView;
        }
        return parentView;
      })())
    );
  },

  /**
   * Gets the parent node holding this scope.
   * @return Node
   */
  get parentNode() {
    return this.ownerView._list;
  },

  /**
   * Gets the owner document holding this scope.
   * @return HTMLDocument
   */
  get document() {
    return this._document || (this._document = this.ownerView.document);
  },

  /**
   * Gets the default window holding this scope.
   * @return nsIDOMWindow
   */
  get window() {
    return this._window || (this._window = this.ownerView.window);
  },

  _topView: null,
  _document: null,
  _window: null,

  ownerView: null,
  contextMenuId: "",
  separatorStr: "",

  _store: null,
  _enumItems: null,
  _nonEnumItems: null,
  _fetched: false,
  _isExpanded: false,
  _isContentVisible: true,
  _isHeaderVisible: true,
  _isArrowVisible: true,
  _isMatch: true,
  _idString: "",
  _nameString: "",
  _target: null,
  _arrow: null,
  _name: null,
  _title: null,
  _enum: null,
  _nonenum: null,
};

// Creating maps and arrays thousands of times for variables or properties
// with a large number of children fills up a lot of memory. Make sure
// these are instantiated only if needed.
DevToolsUtils.defineLazyPrototypeGetter(
  Scope.prototype,
  "_store",
  () => new Map()
);
DevToolsUtils.defineLazyPrototypeGetter(Scope.prototype, "_enumItems", Array);
DevToolsUtils.defineLazyPrototypeGetter(
  Scope.prototype,
  "_nonEnumItems",
  Array
);

/**
 * A Variable is a Scope holding Property instances.
 * Iterable via "for (let [name, property] of instance) { }".
 *
 * @param Scope aScope
 *        The scope to contain this variable.
 * @param string aName
 *        The variable's name.
 * @param object aDescriptor
 *        The variable's descriptor.
 * @param object aOptions
 *        Options of the form accepted by Scope.addItem
 */
function Variable(aScope, aName, aDescriptor, aOptions) {
  this._internalItem = aOptions.internalItem;

  // Treat safe getter descriptors as descriptors with a value.
  if ("getterValue" in aDescriptor) {
    aDescriptor.value = aDescriptor.getterValue;
    delete aDescriptor.get;
    delete aDescriptor.set;
  }

  Scope.call(this, aScope, aName, (this._initialDescriptor = aDescriptor));
  this.setGrip(aDescriptor.value);
}

Variable.prototype = extend(Scope.prototype, {
  /**
   * Whether this Variable should be prefetched when it is remoted.
   */
  get shouldPrefetch() {
    return this.name == "window" || this.name == "this";
  },

  /**
   * Whether this Variable should paginate its contents.
   */
  get allowPaginate() {
    return this.name != "window" && this.name != "this";
  },

  /**
   * The class name applied to this variable's target element.
   */
  targetClassName: "variables-view-variable variable-or-property",

  /**
   * Create a new Property that is a child of Variable.
   *
   * @param string aName
   *        The name of the new Property.
   * @param object aDescriptor
   *        The property's descriptor.
   * @param object aOptions
   *        Options of the form accepted by Scope.addItem
   * @return Property
   *         The newly created child Property.
   */
  _createChild(aName, aDescriptor, aOptions) {
    return new Property(this, aName, aDescriptor, aOptions);
  },

  /**
   * Remove this Variable from its parent and remove all children recursively.
   */
  remove() {
    this.ownerView._store.delete(this._nameString);
    this._variablesView._itemsByElement.delete(this._target);
    this._variablesView._testOnlyHierarchy.delete(this.absoluteName);

    this._target.remove();

    for (const property of this._store.values()) {
      property.remove();
    }
  },

  /**
   * Populates this variable to contain all the properties of an object.
   *
   * @param object aObject
   *        The raw object you want to display.
   * @param object aOptions [optional]
   *        Additional options for adding the properties. Supported options:
   *        - sorted: true to sort all the properties before adding them
   *        - expanded: true to expand all the properties after adding them
   */
  populate(aObject, aOptions = {}) {
    // Retrieve the properties only once.
    if (this._fetched) {
      return;
    }
    this._fetched = true;

    const propertyNames = Object.getOwnPropertyNames(aObject);
    const prototype = Object.getPrototypeOf(aObject);

    // Sort all of the properties before adding them, if preferred.
    if (aOptions.sorted) {
      propertyNames.sort(this._naturalSort);
    }

    // Add all the variable properties.
    for (const name of propertyNames) {
      const descriptor = Object.getOwnPropertyDescriptor(aObject, name);
      if (descriptor.get || descriptor.set) {
        const prop = this._addRawNonValueProperty(name, descriptor);
        if (aOptions.expanded) {
          prop.expanded = true;
        }
      } else {
        const prop = this._addRawValueProperty(name, descriptor, aObject[name]);
        if (aOptions.expanded) {
          prop.expanded = true;
        }
      }
    }
    // Add the variable's __proto__.
    if (prototype) {
      this._addRawValueProperty("__proto__", {}, prototype);
    }
  },

  /**
   * Populates a specific variable or property instance to contain all the
   * properties of an object
   *
   * @param Variable | Property aVar
   *        The target variable to populate.
   * @param object aObject [optional]
   *        The raw object you want to display. If unspecified, the object is
   *        assumed to be defined in a _sourceValue property on the target.
   */
  _populateTarget(aVar, aObject = aVar._sourceValue) {
    aVar.populate(aObject);
  },

  /**
   * Adds a property for this variable based on a raw value descriptor.
   *
   * @param string aName
   *        The property's name.
   * @param object aDescriptor
   *        Specifies the exact property descriptor as returned by a call to
   *        Object.getOwnPropertyDescriptor.
   * @param object aValue
   *        The raw property value you want to display.
   * @return Property
   *         The newly added property instance.
   */
  _addRawValueProperty(aName, aDescriptor, aValue) {
    const descriptor = Object.create(aDescriptor);
    descriptor.value = VariablesView.getGrip(aValue);

    const propertyItem = this.addItem(aName, descriptor);
    propertyItem._sourceValue = aValue;

    // Add an 'onexpand' callback for the property, lazily handling
    // the addition of new child properties.
    if (!VariablesView.isPrimitive(descriptor)) {
      propertyItem.onexpand = this._populateTarget;
    }
    return propertyItem;
  },

  /**
   * Adds a property for this variable based on a getter/setter descriptor.
   *
   * @param string aName
   *        The property's name.
   * @param object aDescriptor
   *        Specifies the exact property descriptor as returned by a call to
   *        Object.getOwnPropertyDescriptor.
   * @return Property
   *         The newly added property instance.
   */
  _addRawNonValueProperty(aName, aDescriptor) {
    const descriptor = Object.create(aDescriptor);
    descriptor.get = VariablesView.getGrip(aDescriptor.get);
    descriptor.set = VariablesView.getGrip(aDescriptor.set);

    return this.addItem(aName, descriptor);
  },

  /**
   * Gets this variable's path to the topmost scope in the form of a string
   * meant for use via eval() or a similar approach.
   * For example, a symbolic name may look like "arguments['0']['foo']['bar']".
   * @return string
   */
  get symbolicName() {
    return this._nameString || "";
  },

  /**
   * Gets full path to this variable, including name of the scope.
   * @return string
   */
  get absoluteName() {
    if (this._absoluteName) {
      return this._absoluteName;
    }

    this._absoluteName =
      this.ownerView._nameString + "[" + escapeString(this._nameString) + "]";
    return this._absoluteName;
  },

  /**
   * Gets this variable's symbolic path to the topmost scope.
   * @return array
   * @see Variable._buildSymbolicPath
   */
  get symbolicPath() {
    if (this._symbolicPath) {
      return this._symbolicPath;
    }
    this._symbolicPath = this._buildSymbolicPath();
    return this._symbolicPath;
  },

  /**
   * Build this variable's path to the topmost scope in form of an array of
   * strings, one for each segment of the path.
   * For example, a symbolic path may look like ["0", "foo", "bar"].
   * @return array
   */
  _buildSymbolicPath(path = []) {
    if (this.name) {
      path.unshift(this.name);
      if (this.ownerView instanceof Variable) {
        return this.ownerView._buildSymbolicPath(path);
      }
    }
    return path;
  },

  /**
   * Returns this variable's value from the descriptor if available.
   * @return any
   */
  get value() {
    return this._initialDescriptor.value;
  },

  /**
   * Returns this variable's getter from the descriptor if available.
   * @return object
   */
  get getter() {
    return this._initialDescriptor.get;
  },

  /**
   * Returns this variable's getter from the descriptor if available.
   * @return object
   */
  get setter() {
    return this._initialDescriptor.set;
  },

  /**
   * Sets the specific grip for this variable (applies the text content and
   * class name to the value label).
   *
   * The grip should contain the value or the type & class, as defined in the
   * remote debugger protocol. For convenience, undefined and null are
   * both considered types.
   *
   * @param any aGrip
   *        Specifies the value and/or type & class of the variable.
   *        e.g. - 42
   *             - true
   *             - "nasu"
   *             - { type: "undefined" }
   *             - { type: "null" }
   *             - { type: "object", class: "Object" }
   */
  setGrip(aGrip) {
    // Don't allow displaying grip information if there's no name available
    // or the grip is malformed.
    if (
      this._nameString === undefined ||
      aGrip === undefined ||
      aGrip === null
    ) {
      return;
    }
    // Getters and setters should display grip information in sub-properties.
    if (this.getter || this.setter) {
      return;
    }

    const prevGrip = this._valueGrip;
    if (prevGrip) {
      this._valueLabel.classList.remove(VariablesView.getClass(prevGrip));
    }
    this._valueGrip = aGrip;

    if (
      aGrip &&
      (aGrip.optimizedOut || aGrip.uninitialized || aGrip.missingArguments)
    ) {
      if (aGrip.optimizedOut) {
        this._valueString = L10N.getStr("variablesViewOptimizedOut");
      } else if (aGrip.uninitialized) {
        this._valueString = L10N.getStr("variablesViewUninitialized");
      } else if (aGrip.missingArguments) {
        this._valueString = L10N.getStr("variablesViewMissingArgs");
      }
      this.eval = null;
    } else {
      this._valueString = VariablesView.getString(aGrip, {
        concise: true,
        noEllipsis: true,
      });
      this.eval = this.ownerView.eval;
    }

    this._valueClassName = VariablesView.getClass(aGrip);

    this._valueLabel.classList.add(this._valueClassName);
    this._valueLabel.setAttribute("value", this._valueString);
    this._separatorLabel.hidden = false;
  },

  /**
   * Initializes this variable's id, view and binds event listeners.
   *
   * @param string aName
   *        The variable's name.
   */
  _init(aName) {
    this._idString = generateId((this._nameString = aName));
    this._displayScope({ value: aName, targetClassName: this.targetClassName });
    this._displayVariable();
    this._addEventListeners();

    if (
      this._initialDescriptor.enumerable ||
      this._nameString == "this" ||
      this._internalItem
    ) {
      this.ownerView._enum.appendChild(this._target);
      this.ownerView._enumItems.push(this);
    } else {
      this.ownerView._nonenum.appendChild(this._target);
      this.ownerView._nonEnumItems.push(this);
    }
  },

  /**
   * Creates the necessary nodes for this variable.
   */
  _displayVariable() {
    const document = this.document;
    const descriptor = this._initialDescriptor;

    const separatorLabel = (this._separatorLabel =
      document.createXULElement("label"));
    separatorLabel.className = "separator";
    separatorLabel.setAttribute("value", this.separatorStr + " ");

    const valueLabel = (this._valueLabel = document.createXULElement("label"));
    valueLabel.className = "value";
    valueLabel.setAttribute("flex", "1");
    valueLabel.setAttribute("crop", "center");

    this._title.appendChild(separatorLabel);
    this._title.appendChild(valueLabel);

    if (VariablesView.isPrimitive(descriptor)) {
      this.hideArrow();
    }

    // If no value will be displayed, we don't need the separator.
    if (!descriptor.get && !descriptor.set && !("value" in descriptor)) {
      separatorLabel.hidden = true;
    }

    // If this is a getter/setter property, create two child pseudo-properties
    // called "get" and "set" that display the corresponding functions.
    if (descriptor.get || descriptor.set) {
      separatorLabel.hidden = true;
      valueLabel.hidden = true;

      const getter = this.addItem("get", { value: descriptor.get });
      const setter = this.addItem("set", { value: descriptor.set });

      getter.hideArrow();
      setter.hideArrow();
      this.expand();
    }
  },

  /**
   * Adds the necessary event listeners for this variable.
   */
  _addEventListeners() {
    this._title.addEventListener("mousedown", this._onClick);
  },

  _symbolicName: null,
  _symbolicPath: null,
  _absoluteName: null,
  _initialDescriptor: null,
  _separatorLabel: null,
  _valueLabel: null,
  _spacer: null,
  _valueGrip: null,
  _valueString: "",
  _valueClassName: "",
  _prevExpandable: false,
  _prevExpanded: false,
});

/**
 * A Property is a Variable holding additional child Property instances.
 * Iterable via "for (let [name, property] of instance) { }".
 *
 * @param Variable aVar
 *        The variable to contain this property.
 * @param string aName
 *        The property's name.
 * @param object aDescriptor
 *        The property's descriptor.
 * @param object aOptions
 *        Options of the form accepted by Scope.addItem
 */
function Property(aVar, aName, aDescriptor, aOptions) {
  Variable.call(this, aVar, aName, aDescriptor, aOptions);
}

Property.prototype = extend(Variable.prototype, {
  /**
   * The class name applied to this property's target element.
   */
  targetClassName: "variables-view-property variable-or-property",

  /**
   * @see Variable.symbolicName
   * @return string
   */
  get symbolicName() {
    if (this._symbolicName) {
      return this._symbolicName;
    }

    this._symbolicName =
      this.ownerView.symbolicName + "[" + escapeString(this._nameString) + "]";
    return this._symbolicName;
  },

  /**
   * @see Variable.absoluteName
   * @return string
   */
  get absoluteName() {
    if (this._absoluteName) {
      return this._absoluteName;
    }

    this._absoluteName =
      this.ownerView.absoluteName + "[" + escapeString(this._nameString) + "]";
    return this._absoluteName;
  },
});

/**
 * A generator-iterator over the VariablesView, Scopes, Variables and Properties.
 */
VariablesView.prototype[Symbol.iterator] =
  Scope.prototype[Symbol.iterator] =
  Variable.prototype[Symbol.iterator] =
  Property.prototype[Symbol.iterator] =
    function* () {
      yield* this._store;
    };

/**
 * Returns true if the descriptor represents an undefined, null or
 * primitive value.
 *
 * @param object aDescriptor
 *        The variable's descriptor.
 */
VariablesView.isPrimitive = function (aDescriptor) {
  // For accessor property descriptors, the getter and setter need to be
  // contained in 'get' and 'set' properties.
  const getter = aDescriptor.get;
  const setter = aDescriptor.set;
  if (getter || setter) {
    return false;
  }

  // As described in the remote debugger protocol, the value grip
  // must be contained in a 'value' property.
  const grip = aDescriptor.value;
  if (typeof grip != "object") {
    return true;
  }

  // For convenience, undefined, null, Infinity, -Infinity, NaN, -0, and long
  // strings are considered types.
  const type = grip.type;
  if (
    type == "undefined" ||
    type == "null" ||
    type == "Infinity" ||
    type == "-Infinity" ||
    type == "NaN" ||
    type == "-0" ||
    type == "symbol" ||
    type == "longString"
  ) {
    return true;
  }

  return false;
};

/**
 * Returns true if the descriptor represents an undefined value.
 *
 * @param object aDescriptor
 *        The variable's descriptor.
 */
VariablesView.isUndefined = function (aDescriptor) {
  // For accessor property descriptors, the getter and setter need to be
  // contained in 'get' and 'set' properties.
  const getter = aDescriptor.get;
  const setter = aDescriptor.set;
  if (
    typeof getter == "object" &&
    getter.type == "undefined" &&
    typeof setter == "object" &&
    setter.type == "undefined"
  ) {
    return true;
  }

  // As described in the remote debugger protocol, the value grip
  // must be contained in a 'value' property.
  const grip = aDescriptor.value;
  if (typeof grip == "object" && grip.type == "undefined") {
    return true;
  }

  return false;
};

/**
 * Returns true if the descriptor represents a falsy value.
 *
 * @param object aDescriptor
 *        The variable's descriptor.
 */
VariablesView.isFalsy = function (aDescriptor) {
  // As described in the remote debugger protocol, the value grip
  // must be contained in a 'value' property.
  const grip = aDescriptor.value;
  if (typeof grip != "object") {
    return !grip;
  }

  // For convenience, undefined, null, NaN, and -0 are all considered types.
  const type = grip.type;
  if (type == "undefined" || type == "null" || type == "NaN" || type == "-0") {
    return true;
  }

  return false;
};

/**
 * Returns true if the value is an instance of Variable or Property.
 *
 * @param any aValue
 *        The value to test.
 */
VariablesView.isVariable = function (aValue) {
  return aValue instanceof Variable;
};

/**
 * Returns a standard grip for a value.
 *
 * @param any aValue
 *        The raw value to get a grip for.
 * @return any
 *         The value's grip.
 */
VariablesView.getGrip = function (aValue) {
  switch (typeof aValue) {
    case "boolean":
    case "string":
      return aValue;
    case "number":
      if (aValue === Infinity) {
        return { type: "Infinity" };
      } else if (aValue === -Infinity) {
        return { type: "-Infinity" };
      } else if (Number.isNaN(aValue)) {
        return { type: "NaN" };
      } else if (1 / aValue === -Infinity) {
        return { type: "-0" };
      }
      return aValue;
    case "undefined":
      // document.all is also "undefined"
      if (aValue === undefined) {
        return { type: "undefined" };
      }
    // fall through
    case "object":
      if (aValue === null) {
        return { type: "null" };
      }
    // fall through
    case "function":
      return { type: "object", class: getObjectClassName(aValue) };
    default:
      console.error(
        "Failed to provide a grip for value of " + typeof value + ": " + aValue
      );
      return null;
  }
};

// Match the function name from the result of toString() or toSource().
//
// Examples:
// (function foobar(a, b) { ...
// function foobar2(a) { ...
// function() { ...
const REGEX_MATCH_FUNCTION_NAME = /^\(?function\s+([^(\s]+)\s*\(/;

/**
 * Helper function to deduce the name of the provided function.
 *
 * @param function function
 *        The function whose name will be returned.
 * @return string
 *         Function name.
 */
function getFunctionName(func) {
  let name = null;
  if (func.name) {
    name = func.name;
  } else {
    let desc;
    try {
      desc = func.getOwnPropertyDescriptor("displayName");
    } catch (ex) {
      // Ignore.
    }
    if (desc && typeof desc.value == "string") {
      name = desc.value;
    }
  }
  if (!name) {
    try {
      const str = (func.toString() || func.toSource()) + "";
      name = (str.match(REGEX_MATCH_FUNCTION_NAME) || [])[1];
    } catch (ex) {
      // Ignore.
    }
  }
  return name;
}

/**
 * Get the object class name. For example, the |window| object has the Window
 * class name (based on [object Window]).
 *
 * @param object object
 *        The object you want to get the class name for.
 * @return string
 *         The object class name.
 */
function getObjectClassName(object) {
  if (object === null) {
    return "null";
  }
  if (object === undefined) {
    return "undefined";
  }

  const type = typeof object;
  if (type != "object") {
    // Grip class names should start with an uppercase letter.
    return type.charAt(0).toUpperCase() + type.substr(1);
  }

  let className;

  try {
    className = ((object + "").match(/^\[object (\S+)\]$/) || [])[1];
    if (!className) {
      className = ((object.constructor + "").match(/^\[object (\S+)\]$/) ||
        [])[1];
    }
    if (!className && typeof object.constructor == "function") {
      className = getFunctionName(object.constructor);
    }
  } catch (ex) {
    // Ignore.
  }

  return className;
}

/**
 * Returns a custom formatted property string for a grip.
 *
 * @param any aGrip
 *        @see Variable.setGrip
 * @param object aOptions
 *        Options:
 *        - concise: boolean that tells you want a concisely formatted string.
 *        - noStringQuotes: boolean that tells to not quote strings.
 *        - noEllipsis: boolean that tells to not add an ellipsis after the
 *        initial text of a longString.
 * @return string
 *         The formatted property string.
 */
VariablesView.getString = function (aGrip, aOptions = {}) {
  if (aGrip && typeof aGrip == "object") {
    switch (aGrip.type) {
      case "undefined":
      case "null":
      case "NaN":
      case "Infinity":
      case "-Infinity":
      case "-0":
        return aGrip.type;
      default: {
        const stringifier = VariablesView.stringifiers.byType[aGrip.type];
        if (stringifier) {
          const result = stringifier(aGrip, aOptions);
          if (result != null) {
            return result;
          }
        }

        if (aGrip.displayString) {
          return VariablesView.getString(aGrip.displayString, aOptions);
        }

        if (aGrip.type == "object" && aOptions.concise) {
          return aGrip.class;
        }

        return "[" + aGrip.type + " " + aGrip.class + "]";
      }
    }
  }

  switch (typeof aGrip) {
    case "string":
      return VariablesView.stringifiers.byType.string(aGrip, aOptions);
    case "boolean":
      return aGrip ? "true" : "false";
    case "number":
      if (!aGrip && 1 / aGrip === -Infinity) {
        return "-0";
      }
    // fall through
    default:
      return aGrip + "";
  }
};

/**
 * The VariablesView stringifiers are used by VariablesView.getString(). These
 * are organized by object type, object class and by object actor preview kind.
 * Some objects share identical ways for previews, for example Arrays, Sets and
 * NodeLists.
 *
 * Any stringifier function must return a string. If null is returned, * then
 * the default stringifier will be used. When invoked, the stringifier is
 * given the same two arguments as those given to VariablesView.getString().
 */
VariablesView.stringifiers = {};

VariablesView.stringifiers.byType = {
  string(aGrip, { noStringQuotes }) {
    if (noStringQuotes) {
      return aGrip;
    }
    return '"' + aGrip + '"';
  },

  longString({ initial }, { noStringQuotes, noEllipsis }) {
    const ellipsis = noEllipsis ? "" : ELLIPSIS;
    if (noStringQuotes) {
      return initial + ellipsis;
    }
    const result = '"' + initial + '"';
    if (!ellipsis) {
      return result;
    }
    return result.substr(0, result.length - 1) + ellipsis + '"';
  },

  object(aGrip, aOptions) {
    const { preview } = aGrip;
    let stringifier;
    if (aGrip.class) {
      stringifier = VariablesView.stringifiers.byObjectClass[aGrip.class];
    }
    if (!stringifier && preview && preview.kind) {
      stringifier = VariablesView.stringifiers.byObjectKind[preview.kind];
    }
    if (stringifier) {
      return stringifier(aGrip, aOptions);
    }
    return null;
  },

  symbol(aGrip) {
    const name = aGrip.name || "";
    return "Symbol(" + name + ")";
  },

  mapEntry(aGrip) {
    const {
      preview: { key, value },
    } = aGrip;

    const keyString = VariablesView.getString(key, {
      concise: true,
      noStringQuotes: true,
    });
    const valueString = VariablesView.getString(value, { concise: true });

    return keyString + " \u2192 " + valueString;
  },
}; // VariablesView.stringifiers.byType

VariablesView.stringifiers.byObjectClass = {
  Function(aGrip, { concise }) {
    // TODO: Bug 948484 - support arrow functions and ES6 generators

    let name = aGrip.userDisplayName || aGrip.displayName || aGrip.name || "";
    name = VariablesView.getString(name, { noStringQuotes: true });

    // TODO: Bug 948489 - Support functions with destructured parameters and
    // rest parameters
    const params = aGrip.parameterNames || "";
    if (!concise) {
      return "function " + name + "(" + params + ")";
    }
    return (name || "function ") + "(" + params + ")";
  },

  RegExp({ displayString }) {
    return VariablesView.getString(displayString, { noStringQuotes: true });
  },

  Date({ preview }) {
    if (!preview || !("timestamp" in preview)) {
      return null;
    }

    if (typeof preview.timestamp != "number") {
      return new Date(preview.timestamp).toString(); // invalid date
    }

    return "Date " + new Date(preview.timestamp).toISOString();
  },

  Number(aGrip) {
    const { preview } = aGrip;
    if (preview === undefined) {
      return null;
    }
    return (
      aGrip.class + " { " + VariablesView.getString(preview.wrappedValue) + " }"
    );
  },
}; // VariablesView.stringifiers.byObjectClass

VariablesView.stringifiers.byObjectClass.Boolean =
  VariablesView.stringifiers.byObjectClass.Number;

VariablesView.stringifiers.byObjectKind = {
  ArrayLike(aGrip, { concise }) {
    const { preview } = aGrip;
    if (concise) {
      return aGrip.class + "[" + preview.length + "]";
    }

    if (!preview.items) {
      return null;
    }

    let shown = 0,
      lastHole = null;
    const result = [];
    for (const item of preview.items) {
      if (item === null) {
        if (lastHole !== null) {
          result[lastHole] += ",";
        } else {
          result.push("");
        }
        lastHole = result.length - 1;
      } else {
        lastHole = null;
        result.push(VariablesView.getString(item, { concise: true }));
      }
      shown++;
    }

    if (shown < preview.length) {
      const n = preview.length - shown;
      result.push(VariablesView.stringifiers._getNMoreString(n));
    } else if (lastHole !== null) {
      // make sure we have the right number of commas...
      result[lastHole] += ",";
    }

    const prefix = aGrip.class == "Array" ? "" : aGrip.class + " ";
    return prefix + "[" + result.join(", ") + "]";
  },

  MapLike(aGrip, { concise }) {
    const { preview } = aGrip;
    if (concise || !preview.entries) {
      const size =
        typeof preview.size == "number" ? "[" + preview.size + "]" : "";
      return aGrip.class + size;
    }

    const entries = [];
    for (const [key, value] of preview.entries) {
      const keyString = VariablesView.getString(key, {
        concise: true,
        noStringQuotes: true,
      });
      const valueString = VariablesView.getString(value, { concise: true });
      entries.push(keyString + ": " + valueString);
    }

    if (typeof preview.size == "number" && preview.size > entries.length) {
      const n = preview.size - entries.length;
      entries.push(VariablesView.stringifiers._getNMoreString(n));
    }

    return aGrip.class + " {" + entries.join(", ") + "}";
  },

  ObjectWithText(aGrip, { concise }) {
    if (concise) {
      return aGrip.class;
    }

    return aGrip.class + " " + VariablesView.getString(aGrip.preview.text);
  },

  ObjectWithURL(aGrip, { concise }) {
    let result = aGrip.class;
    const url = aGrip.preview.url;
    if (!VariablesView.isFalsy({ value: url })) {
      result += ` \u2192 ${getSourceNames(url)[concise ? "short" : "long"]}`;
    }
    return result;
  },

  // Stringifier for any kind of object.
  Object(aGrip, { concise }) {
    if (concise) {
      return aGrip.class;
    }

    const { preview } = aGrip;
    const props = [];

    if (aGrip.class == "Promise" && aGrip.promiseState) {
      const { state, value, reason } = aGrip.promiseState;
      props.push("<state>: " + VariablesView.getString(state));
      if (state == "fulfilled") {
        props.push(
          "<value>: " + VariablesView.getString(value, { concise: true })
        );
      } else if (state == "rejected") {
        props.push(
          "<reason>: " + VariablesView.getString(reason, { concise: true })
        );
      }
    }

    for (const key of Object.keys(preview.ownProperties || {})) {
      const value = preview.ownProperties[key];
      let valueString = "";
      if (value.get) {
        valueString = "Getter";
      } else if (value.set) {
        valueString = "Setter";
      } else {
        valueString = VariablesView.getString(value.value, { concise: true });
      }
      props.push(key + ": " + valueString);
    }

    for (const key of Object.keys(preview.safeGetterValues || {})) {
      const value = preview.safeGetterValues[key];
      const valueString = VariablesView.getString(value.getterValue, {
        concise: true,
      });
      props.push(key + ": " + valueString);
    }

    if (!props.length) {
      return null;
    }

    if (preview.ownPropertiesLength) {
      const previewLength = Object.keys(preview.ownProperties).length;
      const diff = preview.ownPropertiesLength - previewLength;
      if (diff > 0) {
        props.push(VariablesView.stringifiers._getNMoreString(diff));
      }
    }

    const prefix = aGrip.class != "Object" ? aGrip.class + " " : "";
    return prefix + "{" + props.join(", ") + "}";
  }, // Object

  Error(aGrip, { concise }) {
    const { preview } = aGrip;
    const name = VariablesView.getString(preview.name, {
      noStringQuotes: true,
    });
    if (concise) {
      return name || aGrip.class;
    }

    let msg =
      name +
      ": " +
      VariablesView.getString(preview.message, { noStringQuotes: true });

    if (!VariablesView.isFalsy({ value: preview.stack })) {
      msg +=
        "\n" +
        L10N.getStr("variablesViewErrorStacktrace") +
        "\n" +
        preview.stack;
    }

    return msg;
  },

  DOMException(aGrip, { concise }) {
    const { preview } = aGrip;
    if (concise) {
      return preview.name || aGrip.class;
    }

    let msg =
      aGrip.class +
      " [" +
      preview.name +
      ": " +
      VariablesView.getString(preview.message) +
      "\n" +
      "code: " +
      preview.code +
      "\n" +
      "nsresult: 0x" +
      (+preview.result).toString(16);

    if (preview.filename) {
      msg += "\nlocation: " + preview.filename;
      if (preview.lineNumber) {
        msg += ":" + preview.lineNumber;
      }
    }

    return msg + "]";
  },

  DOMEvent(aGrip, { concise }) {
    const { preview } = aGrip;
    if (!preview.type) {
      return null;
    }

    if (concise) {
      return aGrip.class + " " + preview.type;
    }

    let result = preview.type;

    if (
      preview.eventKind == "key" &&
      preview.modifiers &&
      preview.modifiers.length
    ) {
      result += " " + preview.modifiers.join("-");
    }

    const props = [];
    if (preview.target) {
      const target = VariablesView.getString(preview.target, { concise: true });
      props.push("target: " + target);
    }

    for (const prop in preview.properties) {
      const value = preview.properties[prop];
      props.push(
        prop + ": " + VariablesView.getString(value, { concise: true })
      );
    }

    return result + " {" + props.join(", ") + "}";
  }, // DOMEvent

  DOMNode(aGrip, { concise }) {
    const { preview } = aGrip;

    switch (preview.nodeType) {
      case nodeConstants.DOCUMENT_NODE: {
        let result = aGrip.class;
        if (preview.location) {
          result += ` \u2192 ${
            getSourceNames(preview.location)[concise ? "short" : "long"]
          }`;
        }

        return result;
      }

      case nodeConstants.ATTRIBUTE_NODE: {
        const value = VariablesView.getString(preview.value, {
          noStringQuotes: true,
        });
        return preview.nodeName + '="' + escapeHTML(value) + '"';
      }

      case nodeConstants.TEXT_NODE:
        return (
          preview.nodeName + " " + VariablesView.getString(preview.textContent)
        );

      case nodeConstants.COMMENT_NODE: {
        const comment = VariablesView.getString(preview.textContent, {
          noStringQuotes: true,
        });
        return "<!--" + comment + "-->";
      }

      case nodeConstants.DOCUMENT_FRAGMENT_NODE: {
        if (concise || !preview.childNodes) {
          return aGrip.class + "[" + preview.childNodesLength + "]";
        }
        const nodes = [];
        for (const node of preview.childNodes) {
          nodes.push(VariablesView.getString(node));
        }
        if (nodes.length < preview.childNodesLength) {
          const n = preview.childNodesLength - nodes.length;
          nodes.push(VariablesView.stringifiers._getNMoreString(n));
        }
        return aGrip.class + " [" + nodes.join(", ") + "]";
      }

      case nodeConstants.ELEMENT_NODE: {
        const attrs = preview.attributes;
        if (!concise) {
          let n = 0,
            result = "<" + preview.nodeName;
          for (const name in attrs) {
            const value = VariablesView.getString(attrs[name], {
              noStringQuotes: true,
            });
            result += " " + name + '="' + escapeHTML(value) + '"';
            n++;
          }
          if (preview.attributesLength > n) {
            result += " " + ELLIPSIS;
          }
          return result + ">";
        }

        let result = "<" + preview.nodeName;
        if (attrs.id) {
          result += "#" + attrs.id;
        }

        if (attrs.class) {
          result += "." + attrs.class.trim().replace(/\s+/, ".");
        }
        return result + ">";
      }

      default:
        return null;
    }
  }, // DOMNode
}; // VariablesView.stringifiers.byObjectKind

/**
 * Get the "N more…" formatted string, given an N. This is used for displaying
 * how many elements are not displayed in an object preview (eg. an array).
 *
 * @private
 * @param number aNumber
 * @return string
 */
VariablesView.stringifiers._getNMoreString = function (aNumber) {
  const str = L10N.getStr("variablesViewMoreObjects");
  return PluralForm.get(aNumber, str).replace("#1", aNumber);
};

/**
 * Returns a custom class style for a grip.
 *
 * @param any aGrip
 *        @see Variable.setGrip
 * @return string
 *         The custom class style.
 */
VariablesView.getClass = function (aGrip) {
  if (aGrip && typeof aGrip == "object") {
    if (aGrip.preview) {
      switch (aGrip.preview.kind) {
        case "DOMNode":
          return "token-domnode";
      }
    }

    switch (aGrip.type) {
      case "undefined":
        return "token-undefined";
      case "null":
        return "token-null";
      case "Infinity":
      case "-Infinity":
      case "NaN":
      case "-0":
        return "token-number";
      case "longString":
        return "token-string";
    }
  }
  switch (typeof aGrip) {
    case "string":
      return "token-string";
    case "boolean":
      return "token-boolean";
    case "number":
      return "token-number";
    default:
      return "token-other";
  }
};

/**
 * A monotonically-increasing counter, that guarantees the uniqueness of scope,
 * variables and properties ids.
 *
 * @param string aName
 *        An optional string to prefix the id with.
 * @return number
 *         A unique id.
 */
var generateId = (function () {
  let count = 0;
  return function (aName = "") {
    return aName.toLowerCase().trim().replace(/\s+/g, "-") + ++count;
  };
})();

/**
 * Quote and escape a string. The result will be another string containing an
 * ECMAScript StringLiteral which will produce the original one when evaluated
 * by `eval` or similar.
 *
 * @param string aString
 *       An optional string to be escaped. If no string is passed, the function
 *       returns an empty string.
 * @return string
 */
function escapeString(aString) {
  if (typeof aString !== "string") {
    return "";
  }
  // U+2028 and U+2029 are allowed in JSON but not in ECMAScript string literals.
  return JSON.stringify(aString)
    .replace(/\u2028/g, "\\u2028")
    .replace(/\u2029/g, "\\u2029");
}

/**
 * Escape some HTML special characters. We do not need full HTML serialization
 * here, we just want to make strings safe to display in HTML attributes, for
 * the stringifiers.
 *
 * @param string aString
 * @return string
 */
export function escapeHTML(aString) {
  return aString
    .replace(/&/g, "&amp;")
    .replace(/"/g, "&quot;")
    .replace(/</g, "&lt;")
    .replace(/>/g, "&gt;");
}
