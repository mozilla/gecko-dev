/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { KeyCodes } = require("resource://devtools/client/shared/keycodes.js");

const EventEmitter = require("resource://devtools/shared/event-emitter.js");
const AutocompletePopup = require("resource://devtools/client/shared/autocomplete-popup.js");

// Maximum number of selector suggestions shown in the panel.
const MAX_SUGGESTIONS = 15;

/**
 * Converts any input field into a document search box.
 *
 * @param {InspectorPanel} inspector
 *        The InspectorPanel to access the inspector commands for
 *        search and document traversal.
 * @param {DOMNode} input
 *        The input element to which the panel will be attached and from where
 *        search input will be taken.
 * @param {DOMNode} clearBtn
 *        The clear button in the input field that will clear the input value.
 * @param {DOMNode} prevBtn
 *        The prev button in the search label that will move
 *        selection to previous match.
 * @param {DOMNode} nextBtn
 *        The next button in the search label that will move
 *        selection to next match.
 *
 * Emits the following events:
 * - search-cleared: when the search box is emptied
 * - search-result: when a search is made and a result is selected
 */
function InspectorSearch(inspector, input, clearBtn, prevBtn, nextBtn) {
  this.inspector = inspector;
  this.searchBox = input;
  this.searchClearButton = clearBtn;
  this.searchPrevButton = prevBtn;
  this.searchNextButton = nextBtn;
  this._lastSearched = null;

  this._onKeyDown = this._onKeyDown.bind(this);
  this._onInput = this._onInput.bind(this);
  this.findPrev = this.findPrev.bind(this);
  this.findNext = this.findNext.bind(this);
  this._onClearSearch = this._onClearSearch.bind(this);

  this.searchBox.addEventListener("keydown", this._onKeyDown, true);
  this.searchBox.addEventListener("input", this._onInput, true);
  this.searchPrevButton.addEventListener("click", this.findPrev, true);
  this.searchNextButton.addEventListener("click", this.findNext, true);
  this.searchClearButton.addEventListener("click", this._onClearSearch);

  this.autocompleter = new SelectorAutocompleter(inspector, input);
  EventEmitter.decorate(this);
}

exports.InspectorSearch = InspectorSearch;

InspectorSearch.prototype = {
  destroy() {
    this.searchBox.removeEventListener("keydown", this._onKeyDown, true);
    this.searchBox.removeEventListener("input", this._onInput, true);
    this.searchPrevButton.removeEventListener("click", this.findPrev, true);
    this.searchNextButton.removeEventListener("click", this.findNext, true);
    this.searchClearButton.removeEventListener("click", this._onClearSearch);
    this.searchBox = null;
    this.searchPrevButton = null;
    this.searchNextButton = null;
    this.searchClearButton = null;
    this.autocompleter.destroy();
  },

  _onSearch(reverse = false) {
    this.doFullTextSearch(this.searchBox.value, reverse).catch(console.error);
  },

  async doFullTextSearch(query, reverse) {
    const lastSearched = this._lastSearched;
    this._lastSearched = query;

    const searchContainer = this.searchBox.parentNode;

    if (query.length === 0) {
      searchContainer.classList.remove("devtools-searchbox-no-match");
      if (!lastSearched || lastSearched.length) {
        this.emit("search-cleared");
      }
      return;
    }

    const res = await this.inspector.commands.inspectorCommand.findNextNode(
      query,
      {
        reverse,
      }
    );

    // Value has changed since we started this request, we're done.
    if (query !== this.searchBox.value) {
      return;
    }

    if (res) {
      this.inspector.selection.setNodeFront(res.node, {
        reason: "inspectorsearch",
      });
      searchContainer.classList.remove("devtools-searchbox-no-match");
      res.query = query;
      this.emit("search-result", res);
    } else {
      searchContainer.classList.add("devtools-searchbox-no-match");
      this.emit("search-result");
    }
  },

  _onInput() {
    if (this.searchBox.value.length === 0) {
      this.searchClearButton.hidden = true;
      this._onSearch();
    } else {
      this.searchClearButton.hidden = false;
    }
  },

  _onKeyDown(event) {
    if (event.keyCode === KeyCodes.DOM_VK_RETURN) {
      this._onSearch(event.shiftKey);
    }

    const modifierKey =
      Services.appinfo.OS === "Darwin" ? event.metaKey : event.ctrlKey;
    if (event.keyCode === KeyCodes.DOM_VK_G && modifierKey) {
      this._onSearch(event.shiftKey);
      event.preventDefault();
    }

    // The search box is a `<input type="search">`, which would clear on pressing Escape.
    // Prevent the clear action from happening when the autocomplete popup is visible and
    // only hide it. The preventDefault has to be called during keydown and not keypress
    // to avoid the input clearance.
    if (
      event.keyCode === KeyCodes.DOM_VK_ESCAPE &&
      this.autocompleter.searchPopup?.isOpen
    ) {
      event.preventDefault();
      this.autocompleter.hidePopup();
    }
  },

  findNext() {
    this._onSearch();
  },

  findPrev() {
    this._onSearch(true);
  },

  _onClearSearch() {
    this.searchBox.parentNode.classList.remove("devtools-searchbox-no-match");
    this.searchBox.value = "";
    this.searchBox.focus();
    this.searchClearButton.hidden = true;
    this.emit("search-cleared");
  },
};

/**
 * Converts any input box on a page to a CSS selector search and suggestion box.
 *
 * Emits 'processing-done' event when it is done processing the current
 * keypress, search request or selection from the list, whether that led to a
 * search or not.
 *
 * @constructor
 * @param InspectorPanel inspector
 *        The InspectorPanel to access the inspector commands for
 *        search and document traversal.
 * @param nsiInputElement inputNode
 *        The input element to which the panel will be attached and from where
 *        search input will be taken.
 */
class SelectorAutocompleter extends EventEmitter {
  constructor(inspector, inputNode) {
    super();

    this.inspector = inspector;
    this.searchBox = inputNode;
    this.panelDoc = this.searchBox.ownerDocument;

    this.showSuggestions = this.showSuggestions.bind(this);

    // Options for the AutocompletePopup.
    const options = {
      listId: "searchbox-panel-listbox",
      autoSelect: true,
      position: "top",
      onClick: this.#onSearchPopupClick,
    };

    // The popup will be attached to the toolbox document.
    this.searchPopup = new AutocompletePopup(inspector._toolbox.doc, options);

    this.searchBox.addEventListener("input", this.showSuggestions, true);
    this.searchBox.addEventListener("keypress", this.#onSearchKeypress, true);
  }

  // The possible states of the query.
  States = {
    CLASS: "class",
    ID: "id",
    TAG: "tag",
    ATTRIBUTE: "attribute",
    // This is for pseudo classes (e.g. `:active`, `:not()`). We keep it as "pseudo" as
    // the server handles both pseudo elements and pseudo classes under the same type
    PSEUDO_CLASS: "pseudo",
    // This is for pseudo element (e.g. `::selection`)
    PSEUDO_ELEMENT: "pseudo-element",
  };

  // The current state of the query.
  #state = null;

  // The query corresponding to last state computation.
  #lastStateCheckAt = null;

  get walker() {
    return this.inspector.walker;
  }

  /**
   * Computes the state of the query. State refers to whether the query
   * currently requires a class suggestion, or a tag, or an Id suggestion.
   * This getter will effectively compute the state by traversing the query
   * character by character each time the query changes.
   *
   * @example
   *        '#f' requires an Id suggestion, so the state is States.ID
   *        'div > .foo' requires class suggestion, so state is States.CLASS
   */
  // eslint-disable-next-line complexity
  get state() {
    if (!this.searchBox || !this.searchBox.value) {
      return null;
    }

    const query = this.searchBox.value;
    if (this.#lastStateCheckAt == query) {
      // If query is the same, return early.
      return this.#state;
    }
    this.#lastStateCheckAt = query;

    this.#state = null;
    let subQuery = "";
    // Now we iterate over the query and decide the state character by
    // character.
    // The logic here is that while iterating, the state can go from one to
    // another with some restrictions. Like, if the state is Class, then it can
    // never go to Tag state without a space or '>' character; Or like, a Class
    // state with only '.' cannot go to an Id state without any [a-zA-Z] after
    // the '.' which means that '.#' is a selector matching a class name '#'.
    // Similarily for '#.' which means a selctor matching an id '.'.
    for (let i = 1; i <= query.length; i++) {
      // Calculate the state.
      subQuery = query.slice(0, i);
      let [secondLastChar, lastChar] = subQuery.slice(-2);
      switch (this.#state) {
        case null:
          // This will happen only in the first iteration of the for loop.
          lastChar = secondLastChar;

        case this.States.TAG: // eslint-disable-line
          if (lastChar === ".") {
            this.#state = this.States.CLASS;
          } else if (lastChar === "#") {
            this.#state = this.States.ID;
          } else if (lastChar === "[") {
            this.#state = this.States.ATTRIBUTE;
          } else if (lastChar === ":") {
            this.#state = this.States.PSEUDO_CLASS;
          } else if (lastChar === ")") {
            this.#state = null;
          } else {
            this.#state = this.States.TAG;
          }
          break;

        case this.States.CLASS:
          if (subQuery.match(/[\.]+[^\.]*$/)[0].length > 2) {
            // Checks whether the subQuery has atleast one [a-zA-Z] after the
            // '.'.
            if (lastChar === " " || lastChar === ">") {
              this.#state = this.States.TAG;
            } else if (lastChar === "#") {
              this.#state = this.States.ID;
            } else if (lastChar === "[") {
              this.#state = this.States.ATTRIBUTE;
            } else if (lastChar === ":") {
              this.#state = this.States.PSEUDO_CLASS;
            } else if (lastChar === ")") {
              this.#state = null;
            } else {
              this.#state = this.States.CLASS;
            }
          }
          break;

        case this.States.ID:
          if (subQuery.match(/[#]+[^#]*$/)[0].length > 2) {
            // Checks whether the subQuery has atleast one [a-zA-Z] after the
            // '#'.
            if (lastChar === " " || lastChar === ">") {
              this.#state = this.States.TAG;
            } else if (lastChar === ".") {
              this.#state = this.States.CLASS;
            } else if (lastChar === "[") {
              this.#state = this.States.ATTRIBUTE;
            } else if (lastChar === ":") {
              this.#state = this.States.PSEUDO_CLASS;
            } else if (lastChar === ")") {
              this.#state = null;
            } else {
              this.#state = this.States.ID;
            }
          }
          break;

        case this.States.ATTRIBUTE:
          if (subQuery.match(/[\[][^\]]+[\]]/) !== null) {
            // Checks whether the subQuery has at least one ']' after the '['.
            if (lastChar === " " || lastChar === ">") {
              this.#state = this.States.TAG;
            } else if (lastChar === ".") {
              this.#state = this.States.CLASS;
            } else if (lastChar === "#") {
              this.#state = this.States.ID;
            } else if (lastChar === ":") {
              this.#state = this.States.PSEUDO_CLASS;
            } else if (lastChar === ")") {
              this.#state = null;
            } else {
              this.#state = this.States.ATTRIBUTE;
            }
          }
          break;

        case this.States.PSEUDO_CLASS:
          if (lastChar === ":" && secondLastChar === ":") {
            // We don't support searching for pseudo elements, so bail out when we
            // see `::`
            this.#state = this.States.PSEUDO_ELEMENT;
            return this.#state;
          }

          if (lastChar === "(") {
            this.#state = null;
          } else if (lastChar === ".") {
            this.#state = this.States.CLASS;
          } else if (lastChar === "#") {
            this.#state = this.States.ID;
          } else {
            this.#state = this.States.PSEUDO_CLASS;
          }
          break;
      }
    }
    return this.#state;
  }

  /**
   * Removes event listeners and cleans up references.
   */
  destroy() {
    this.searchBox.removeEventListener("input", this.showSuggestions, true);
    this.searchBox.removeEventListener(
      "keypress",
      this.#onSearchKeypress,
      true
    );
    this.searchPopup.destroy();
    this.searchPopup = null;
    this.searchBox = null;
    this.panelDoc = null;
  }

  /**
   * Handles keypresses inside the input box.
   */
  #onSearchKeypress = event => {
    const popup = this.searchPopup;
    switch (event.keyCode) {
      case KeyCodes.DOM_VK_RETURN:
      case KeyCodes.DOM_VK_TAB:
        if (popup.isOpen) {
          if (popup.selectedItem) {
            this.searchBox.value = popup.selectedItem.label;
          }
          this.hidePopup();
        } else if (!popup.isOpen) {
          // When tab is pressed with focus on searchbox and closed popup,
          // do not prevent the default to avoid a keyboard trap and move focus
          // to next/previous element.
          this.emitForTests("processing-done");
          return;
        }
        break;

      case KeyCodes.DOM_VK_UP:
        if (popup.isOpen && popup.itemCount > 0) {
          popup.selectPreviousItem();
          this.searchBox.value = popup.selectedItem.label;
        }
        break;

      case KeyCodes.DOM_VK_DOWN:
        if (popup.isOpen && popup.itemCount > 0) {
          popup.selectNextItem();
          this.searchBox.value = popup.selectedItem.label;
        }
        break;

      case KeyCodes.DOM_VK_ESCAPE:
        if (popup.isOpen) {
          this.hidePopup();
        } else {
          this.emitForTests("processing-done");
          return;
        }
        break;

      default:
        return;
    }

    event.preventDefault();
    event.stopPropagation();
    this.emitForTests("processing-done");
  };

  /**
   * Handles click events from the autocomplete popup.
   */
  #onSearchPopupClick = event => {
    const selectedItem = this.searchPopup.selectedItem;
    if (selectedItem) {
      this.searchBox.value = selectedItem.label;
    }
    this.hidePopup();

    event.preventDefault();
    event.stopPropagation();
  };

  /**
   * Populates the suggestions list and show the suggestion popup.
   *
   * @param {Array} suggestions: List of suggestions
   * @param {String|null} popupState: One of SelectorAutocompleter.States
   * @return {Promise} promise that will resolve when the autocomplete popup is fully
   * displayed or hidden.
   */
  #showPopup(suggestions, popupState) {
    let total = 0;
    const query = this.searchBox.value;
    const items = [];

    for (let [value, state] of suggestions) {
      if (popupState === this.States.PSEUDO_CLASS) {
        value = query.substring(0, query.lastIndexOf(":")) + value;
      } else if (query.match(/[\s>+~]$/)) {
        // for cases like 'div ', 'div >', 'div+' or 'div~'
        value = query + value;
      } else if (query.match(/[\s>+~][\.#a-zA-Z][^\s>+~\.#\[]*$/)) {
        // for cases like 'div #a' or 'div .a' or 'div > d' and likewise
        const lastPart = query.match(/[\s>+~][\.#a-zA-Z][^\s>+~\.#\[]*$/)[0];
        value = query.slice(0, -1 * lastPart.length + 1) + value;
      } else if (query.match(/[a-zA-Z][#\.][^#\.\s+>~]*$/)) {
        // for cases like 'div.class' or '#foo.bar' and likewise
        const lastPart = query.match(/[a-zA-Z][#\.][^#\.\s+>~]*$/)[0];
        value = query.slice(0, -1 * lastPart.length + 1) + value;
      } else if (query.match(/[a-zA-Z]*\[[^\]]*\][^\]]*/)) {
        // for cases like '[foo].bar' and likewise
        const attrPart = query.substring(0, query.lastIndexOf("]") + 1);
        value = attrPart + value;
      }

      const item = {
        preLabel: query,
        label: value,
      };

      // In case the query's state is tag and the item's state is id or class
      // adjust the preLabel
      if (popupState === this.States.TAG && state === this.States.CLASS) {
        item.preLabel = "." + item.preLabel;
      }
      if (popupState === this.States.TAG && state === this.States.ID) {
        item.preLabel = "#" + item.preLabel;
      }

      items.push(item);
      if (++total > MAX_SUGGESTIONS - 1) {
        break;
      }
    }

    if (total > 0) {
      const onPopupOpened = this.searchPopup.once("popup-opened");
      this.searchPopup.once("popup-closed", () => {
        this.searchPopup.setItems(items);
        // The offset is left padding (22px) + left border width (1px) of searchBox.
        const xOffset = 23;
        this.searchPopup.openPopup(this.searchBox, xOffset);
      });
      this.searchPopup.hidePopup();
      return onPopupOpened;
    }

    return this.hidePopup();
  }

  /**
   * Hide the suggestion popup if necessary.
   */
  hidePopup() {
    const onPopupClosed = this.searchPopup.once("popup-closed");
    this.searchPopup.hidePopup();
    return onPopupClosed;
  }

  /**
   * Suggests classes,ids and tags based on the user input as user types in the
   * searchbox.
   */
  async showSuggestions() {
    let query = this.searchBox.value;
    const originalQuery = this.searchBox.value;

    const state = this.state;
    let completing = "";

    if (
      // Hide the popup if:
      // - the query is empty
      !query ||
      // - the query ends with * (because we don't want to suggest all nodes)
      query.endsWith("*") ||
      // - if it is an attribute selector (because it would give a lot of useless results).
      state === this.States.ATTRIBUTE ||
      // - if it is a pseudo element selector (we don't support it, see Bug 1097991)
      state === this.States.PSEUDO_ELEMENT
    ) {
      this.hidePopup();
      this.emitForTests("processing-done", { query: originalQuery });
      return;
    }

    if (state === this.States.TAG) {
      // gets the tag that is being completed. For ex:
      // - 'di' returns 'di'
      // - 'div.foo s' returns 's'
      // - 'div.foo > s' returns 's'
      // - 'div.foo + s' returns 's'
      // - 'div.foo ~ s' returns 's'
      // - 'div.foo x-el_1' returns 'x-el_1'
      const matches = query.match(/[\s>+~]?(?<tag>[a-zA-Z0-9_-]*)$/);
      completing = matches.groups.tag;
      query = query.slice(0, query.length - completing.length);
    } else if (state === this.States.CLASS) {
      // gets the class that is being completed. For ex. '.foo.b' returns 'b'
      completing = query.match(/\.([^\.]*)$/)[1];
      query = query.slice(0, query.length - completing.length - 1);
    } else if (state === this.States.ID) {
      // gets the id that is being completed. For ex. '.foo#b' returns 'b'
      completing = query.match(/#([^#]*)$/)[1];
      query = query.slice(0, query.length - completing.length - 1);
    } else if (state === this.States.PSEUDO_CLASS) {
      // The getSuggestionsForQuery expects a pseudo element without the : prefix
      completing = query.substring(query.lastIndexOf(":") + 1);
      query = "";
    }
    // TODO: implement some caching so that over the wire request is not made
    // everytime.
    if (/[\s+>~]$/.test(query)) {
      query += "*";
    }

    let suggestions =
      await this.inspector.commands.inspectorCommand.getSuggestionsForQuery(
        query,
        completing,
        state
      );

    if (state === this.States.CLASS) {
      completing = "." + completing;
    } else if (state === this.States.ID) {
      completing = "#" + completing;
    } else if (state === this.States.PSEUDO_CLASS) {
      completing = ":" + completing;
      // Remove pseudo-element suggestions, since the search does not work with them (Bug 1097991)
      suggestions = suggestions.filter(
        suggestion => !suggestion[0].startsWith("::")
      );
    }

    // If there is a single tag match and it's what the user typed, then
    // don't need to show a popup.
    if (suggestions.length === 1 && suggestions[0][0] === completing) {
      suggestions = [];
    }

    // Wait for the autocomplete-popup to fire its popup-opened event, to make sure
    // the autoSelect item has been selected.
    await this.#showPopup(suggestions, state);
    this.emitForTests("processing-done", { query: originalQuery });
  }
}

exports.SelectorAutocompleter = SelectorAutocompleter;
