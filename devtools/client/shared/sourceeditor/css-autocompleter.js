/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  cssTokenizer,
  cssTokenizerWithLineColumn,
} = require("resource://devtools/shared/css/parsing-utils.js");

/**
 * Here is what this file (+ css-parsing-utils.js) do.
 *
 * The main objective here is to provide as much suggestions to the user editing
 * a stylesheet in Style Editor. The possible things that can be suggested are:
 *  - CSS property names
 *  - CSS property values
 *  - CSS Selectors
 *  - Some other known CSS keywords
 *
 * Gecko provides a list of both property names and their corresponding values.
 * We take out a list of matching selectors using the Inspector actor's
 * `getSuggestionsForQuery` method. Now the only thing is to parse the CSS being
 * edited by the user, figure out what token or word is being written and last
 * but the most difficult, what is being edited.
 *
 * The file 'css-parsing-utils' helps to convert the CSS into meaningful tokens,
 * each having a certain type associated with it. These tokens help us to figure
 * out the currently edited word and to write a CSS state machine to figure out
 * what the user is currently editing (e.g. a selector or a property or a value,
 * or even fine grained information like an id in the selector).
 *
 * The `resolveState` method iterated over the tokens spitted out by the
 * tokenizer, using switch cases, follows a state machine logic and finally
 * figures out these informations:
 *  - The state of the CSS at the cursor (one out of CSS_STATES)
 *  - The current token that is being edited `completing`
 *  - If the state is "selector", the selector state (one of SELECTOR_STATES)
 *  - If the state is "selector", the current selector till the cursor
 *  - If the state is "value", the corresponding property name
 *
 * In case of "value" and "property" states, we simply use the information
 * provided by Gecko to filter out the possible suggestions.
 * For "selector" state, we request the Inspector actor to query the page DOM
 * and filter out the possible suggestions.
 * For "media" and "keyframes" state, the only possible suggestions for now are
 * "media" and "keyframes" respectively, although "media" can have suggestions
 * like "max-width", "orientation" etc. Similarly "value" state can also have
 * much better logical suggestions if we fine grain identify a sub state just
 * like we do for the "selector" state.
 */

class CSSCompleter {
  // Autocompletion types.

  // These can be read _a lot_ in a hotpath, so keep those as individual constants using
  // a Symbol as a value so the lookup is faster.
  static CSS_STATE_NULL = Symbol("state_null");
  // foo { bar|: … };
  static CSS_STATE_PROPERTY = Symbol("state_property");
  // foo {bar: baz|};
  static CSS_STATE_VALUE = Symbol("state_value");
  // f| {bar: baz};
  static CSS_STATE_SELECTOR = Symbol("state_selector");
  // @med| , or , @media scr| { };
  static CSS_STATE_MEDIA = Symbol("state_media");
  // @keyf|;
  static CSS_STATE_KEYFRAMES = Symbol("state_keyframes");
  // @keyframs foobar { t|;
  static CSS_STATE_FRAME = Symbol("state_frame");

  static CSS_SELECTOR_STATE_NULL = Symbol("selector_state_null");
  // #f|
  static CSS_SELECTOR_STATE_ID = Symbol("selector_state_id");
  // #foo.b|
  static CSS_SELECTOR_STATE_CLASS = Symbol("selector_state_class");
  // fo|
  static CSS_SELECTOR_STATE_TAG = Symbol("selector_state_tag");
  // foo:|
  static CSS_SELECTOR_STATE_PSEUDO = Symbol("selector_state_pseudo");
  // foo[b|
  static CSS_SELECTOR_STATE_ATTRIBUTE = Symbol("selector_state_attribute");
  // foo[bar=b|
  static CSS_SELECTOR_STATE_VALUE = Symbol("selector_state_value");

  static SELECTOR_STATE_STRING_BY_SYMBOL = new Map([
    [CSSCompleter.CSS_SELECTOR_STATE_NULL, "null"],
    [CSSCompleter.CSS_SELECTOR_STATE_ID, "id"],
    [CSSCompleter.CSS_SELECTOR_STATE_CLASS, "class"],
    [CSSCompleter.CSS_SELECTOR_STATE_TAG, "tag"],
    [CSSCompleter.CSS_SELECTOR_STATE_PSEUDO, "pseudo"],
    [CSSCompleter.CSS_SELECTOR_STATE_ATTRIBUTE, "attribute"],
    [CSSCompleter.CSS_SELECTOR_STATE_VALUE, "value"],
  ]);

  /**
   * @constructor
   * @param options {Object} An options object containing the following options:
   *        - walker {Object} The object used for query selecting from the current
   *                 target's DOM.
   *        - maxEntries {Number} Maximum selectors suggestions to display.
   *        - cssProperties {Object} The database of CSS properties.
   */
  constructor(options = {}) {
    this.walker = options.walker;
    this.maxEntries = options.maxEntries || 15;
    this.cssProperties = options.cssProperties;

    this.propertyNames = this.cssProperties.getNames().sort();

    // Array containing the [line, ch, scopeStack] for the locations where the
    // CSS state is "null"
    this.nullStates = [];
  }

  /**
   * Returns a list of suggestions based on the caret position.
   *
   * @param source {String} String of the source code.
   * @param cursor {Object} Cursor location with line and ch properties.
   *
   * @returns [{object}] A sorted list of objects containing the following
   *          peroperties:
   *          - label {String} Full keyword for the suggestion
   *          - preLabel {String} Already entered part of the label
   */
  complete(source, cursor) {
    // Getting the context from the caret position.
    if (!this.resolveState({ source, line: cursor.line, column: cursor.ch })) {
      // We couldn't resolve the context, we won't be able to complete.
      return Promise.resolve([]);
    }

    // Properly suggest based on the state.
    switch (this.state) {
      case CSSCompleter.CSS_STATE_PROPERTY:
        return this.completeProperties(this.completing);

      case CSSCompleter.CSS_STATE_VALUE:
        return this.completeValues(this.propertyName, this.completing);

      case CSSCompleter.CSS_STATE_SELECTOR:
        return this.suggestSelectors();

      case CSSCompleter.CSS_STATE_MEDIA:
      case CSSCompleter.CSS_STATE_KEYFRAMES:
        if ("media".startsWith(this.completing)) {
          return Promise.resolve([
            {
              label: "media",
              preLabel: this.completing,
              text: "media",
            },
          ]);
        } else if ("keyframes".startsWith(this.completing)) {
          return Promise.resolve([
            {
              label: "keyframes",
              preLabel: this.completing,
              text: "keyframes",
            },
          ]);
        }
    }
    return Promise.resolve([]);
  }

  /**
   * Resolves the state of CSS given a source and a cursor location, or an array of tokens.
   * This method implements a custom written CSS state machine. The various switch
   * statements provide the transition rules for the state. It also finds out various
   * information about the nearby CSS like the property name being completed, the complete
   * selector, etc.
   *
   * @param options {Object}
   * @param sourceTokens {Array<InspectorCSSToken>} Optional array of the tokens representing
   *                     a CSS source. When this is defined, `source`, `line` and `column`
   *                     shouldn't be passed.
   * @param options.source {String} Optional string of the source code. When this is defined,
   *                       `sourceTokens` shouldn't be passed.
   * @param options.line {Number} Cursor line. Mandatory when source is passed.
   * @param options.column {Number} Cursor column. Mandatory when source is passed
   *
   * @returns CSS_STATE
   *          One of CSS_STATE enum or null if the state cannot be resolved.
   */
  // eslint-disable-next-line complexity
  resolveState({ sourceTokens, source, line, column }) {
    if (sourceTokens && source) {
      throw new Error(
        "This function only accepts sourceTokens or source, not both"
      );
    }

    // _state can be one of CSS_STATES;
    let _state = CSSCompleter.CSS_STATE_NULL;
    let selector = "";
    let selectorState = CSSCompleter.CSS_SELECTOR_STATE_NULL;
    let propertyName = null;
    let scopeStack = [];
    let selectors = [];

    // If we need to retrieve the tokens, fetch the closest null state line/ch from cached
    // null state locations to save some cycle.
    const matchedStateIndex = !sourceTokens
      ? this.findNearestNullState(line)
      : -1;
    if (matchedStateIndex > -1) {
      const state = this.nullStates[matchedStateIndex];
      line -= state[0];
      if (line == 0) {
        column -= state[1];
      }
      source = source.split("\n").slice(state[0]);
      source[0] = source[0].slice(state[1]);
      source = source.join("\n");
      scopeStack = [...state[2]];
      this.nullStates.length = matchedStateIndex + 1;
    } else {
      this.nullStates = [];
    }

    const tokens = sourceTokens || cssTokenizerWithLineColumn(source);
    const tokIndex = tokens.length - 1;

    if (
      !sourceTokens &&
      tokIndex >= 0 &&
      (tokens[tokIndex].loc.end.line < line ||
        (tokens[tokIndex].loc.end.line === line &&
          tokens[tokIndex].loc.end.column < column))
    ) {
      // If the last token ends before the cursor location, we didn't
      // tokenize it correctly.  This special case can happen if the
      // final token is a comment.
      return null;
    }

    let cursor = 0;
    // This will maintain a stack of paired elements like { & }, @m & }, : & ;
    // etc
    let token = null;
    let selectorBeforeNot = null;
    while (cursor <= tokIndex && (token = tokens[cursor++])) {
      switch (_state) {
        case CSSCompleter.CSS_STATE_PROPERTY:
          // From CSS_STATE_PROPERTY, we can either go to CSS_STATE_VALUE
          // state when we hit the first ':' or CSS_STATE_SELECTOR if "}" is
          // reached.
          if (token.tokenType === "Colon") {
            scopeStack.push(":");
            if (tokens[cursor - 2].tokenType != "WhiteSpace") {
              propertyName = tokens[cursor - 2].text;
            } else {
              propertyName = tokens[cursor - 3].text;
            }
            _state = CSSCompleter.CSS_STATE_VALUE;
          }

          if (token.tokenType === "CloseCurlyBracket") {
            if (/[{f]/.test(scopeStack.at(-1))) {
              const popped = scopeStack.pop();
              if (popped == "f") {
                _state = CSSCompleter.CSS_STATE_FRAME;
              } else {
                selector = "";
                selectors = [];
                _state = CSSCompleter.CSS_STATE_NULL;
              }
            }
          }
          break;

        case CSSCompleter.CSS_STATE_VALUE:
          // From CSS_STATE_VALUE, we can go to one of CSS_STATE_PROPERTY,
          // CSS_STATE_FRAME, CSS_STATE_SELECTOR and CSS_STATE_NULL
          if (token.tokenType === "Semicolon") {
            if (/[:]/.test(scopeStack.at(-1))) {
              scopeStack.pop();
              _state = CSSCompleter.CSS_STATE_PROPERTY;
            }
          }

          if (token.tokenType === "CloseCurlyBracket") {
            if (scopeStack.at(-1) == ":") {
              scopeStack.pop();
            }

            if (/[{f]/.test(scopeStack.at(-1))) {
              const popped = scopeStack.pop();
              if (popped == "f") {
                _state = CSSCompleter.CSS_STATE_FRAME;
              } else {
                selector = "";
                selectors = [];
                _state = CSSCompleter.CSS_STATE_NULL;
              }
            }
          }
          break;

        case CSSCompleter.CSS_STATE_SELECTOR:
          // From CSS_STATE_SELECTOR, we can only go to CSS_STATE_PROPERTY
          // when we hit "{"
          if (token.tokenType === "CurlyBracketBlock") {
            scopeStack.push("{");
            _state = CSSCompleter.CSS_STATE_PROPERTY;
            selectors.push(selector);
            selector = "";
            break;
          }

          switch (selectorState) {
            case CSSCompleter.CSS_SELECTOR_STATE_ID:
            case CSSCompleter.CSS_SELECTOR_STATE_CLASS:
            case CSSCompleter.CSS_SELECTOR_STATE_TAG:
              switch (token.tokenType) {
                case "Hash":
                case "IDHash":
                  selectorState = CSSCompleter.CSS_SELECTOR_STATE_ID;
                  selector += token.text;
                  break;

                case "Delim":
                  if (token.text == ".") {
                    selectorState = CSSCompleter.CSS_SELECTOR_STATE_CLASS;
                    selector += ".";
                    if (
                      cursor <= tokIndex &&
                      tokens[cursor].tokenType == "Ident"
                    ) {
                      token = tokens[cursor++];
                      selector += token.text;
                    }
                  } else if (token.text == "#") {
                    // Lonely # char, that doesn't produce a Hash nor IDHash
                    selectorState = CSSCompleter.CSS_SELECTOR_STATE_ID;
                    selector += "#";
                  } else if (
                    token.text == "+" ||
                    token.text == "~" ||
                    token.text == ">"
                  ) {
                    selectorState = CSSCompleter.CSS_SELECTOR_STATE_NULL;
                    selector += token.text;
                  }
                  break;

                case "Comma":
                  selectorState = CSSCompleter.CSS_SELECTOR_STATE_NULL;
                  selectors.push(selector);
                  selector = "";
                  break;

                case "Colon":
                  selectorState = CSSCompleter.CSS_SELECTOR_STATE_PSEUDO;
                  selector += ":";
                  if (cursor > tokIndex) {
                    break;
                  }

                  token = tokens[cursor++];
                  switch (token.tokenType) {
                    case "Function":
                      if (token.value == "not") {
                        selectorBeforeNot = selector;
                        selector = "";
                        scopeStack.push("(");
                      } else {
                        selector += token.text;
                      }
                      selectorState = CSSCompleter.CSS_SELECTOR_STATE_NULL;
                      break;

                    case "Ident":
                      selector += token.text;
                      break;
                  }
                  break;

                case "SquareBracketBlock":
                  selectorState = CSSCompleter.CSS_SELECTOR_STATE_ATTRIBUTE;
                  scopeStack.push("[");
                  selector += "[";
                  break;

                case "CloseParenthesis":
                  if (scopeStack.at(-1) == "(") {
                    scopeStack.pop();
                    selector = selectorBeforeNot + "not(" + selector + ")";
                    selectorBeforeNot = null;
                  } else {
                    selector += ")";
                  }
                  selectorState = CSSCompleter.CSS_SELECTOR_STATE_NULL;
                  break;

                case "WhiteSpace":
                  selectorState = CSSCompleter.CSS_SELECTOR_STATE_NULL;
                  selector && (selector += " ");
                  break;
              }
              break;

            case CSSCompleter.CSS_SELECTOR_STATE_NULL:
              // From CSS_SELECTOR_STATE_NULL state, we can go to one of
              // CSS_SELECTOR_STATE_ID, CSS_SELECTOR_STATE_CLASS or
              // CSS_SELECTOR_STATE_TAG
              switch (token.tokenType) {
                case "Hash":
                case "IDHash":
                  selectorState = CSSCompleter.CSS_SELECTOR_STATE_ID;
                  selector += token.text;
                  break;

                case "Ident":
                  selectorState = CSSCompleter.CSS_SELECTOR_STATE_TAG;
                  selector += token.text;
                  break;

                case "Delim":
                  if (token.text == ".") {
                    selectorState = CSSCompleter.CSS_SELECTOR_STATE_CLASS;
                    selector += ".";
                    if (
                      cursor <= tokIndex &&
                      tokens[cursor].tokenType == "Ident"
                    ) {
                      token = tokens[cursor++];
                      selector += token.text;
                    }
                  } else if (token.text == "#") {
                    // Lonely # char, that doesn't produce a Hash nor IDHash
                    selectorState = CSSCompleter.CSS_SELECTOR_STATE_ID;
                    selector += "#";
                  } else if (token.text == "*") {
                    selectorState = CSSCompleter.CSS_SELECTOR_STATE_TAG;
                    selector += "*";
                  } else if (
                    token.text == "+" ||
                    token.text == "~" ||
                    token.text == ">"
                  ) {
                    selector += token.text;
                  }
                  break;

                case "Comma":
                  selectorState = CSSCompleter.CSS_SELECTOR_STATE_NULL;
                  selectors.push(selector);
                  selector = "";
                  break;

                case "Colon":
                  selectorState = CSSCompleter.CSS_SELECTOR_STATE_PSEUDO;
                  selector += ":";
                  if (cursor > tokIndex) {
                    break;
                  }

                  token = tokens[cursor++];
                  switch (token.tokenType) {
                    case "Function":
                      if (token.value == "not") {
                        selectorBeforeNot = selector;
                        selector = "";
                        scopeStack.push("(");
                      } else {
                        selector += token.text;
                      }
                      selectorState = CSSCompleter.CSS_SELECTOR_STATE_NULL;
                      break;

                    case "Ident":
                      selector += token.text;
                      break;
                  }
                  break;

                case "SquareBracketBlock":
                  selectorState = CSSCompleter.CSS_SELECTOR_STATE_ATTRIBUTE;
                  scopeStack.push("[");
                  selector += "[";
                  break;

                case "CloseParenthesis":
                  if (scopeStack.at(-1) == "(") {
                    scopeStack.pop();
                    selector = selectorBeforeNot + "not(" + selector + ")";
                    selectorBeforeNot = null;
                  } else {
                    selector += ")";
                  }
                  selectorState = CSSCompleter.CSS_SELECTOR_STATE_NULL;
                  break;

                case "WhiteSpace":
                  selector && (selector += " ");
                  break;
              }
              break;

            case CSSCompleter.CSS_SELECTOR_STATE_PSEUDO:
              switch (token.tokenType) {
                case "Delim":
                  if (
                    token.text == "+" ||
                    token.text == "~" ||
                    token.text == ">"
                  ) {
                    selectorState = CSSCompleter.CSS_SELECTOR_STATE_NULL;
                    selector += token.text;
                  }
                  break;

                case "Comma":
                  selectorState = CSSCompleter.CSS_SELECTOR_STATE_NULL;
                  selectors.push(selector);
                  selector = "";
                  break;

                case "Colon":
                  selectorState = CSSCompleter.CSS_SELECTOR_STATE_PSEUDO;
                  selector += ":";
                  if (cursor > tokIndex) {
                    break;
                  }

                  token = tokens[cursor++];
                  switch (token.tokenType) {
                    case "Function":
                      if (token.value == "not") {
                        selectorBeforeNot = selector;
                        selector = "";
                        scopeStack.push("(");
                      } else {
                        selector += token.text;
                      }
                      selectorState = CSSCompleter.CSS_SELECTOR_STATE_NULL;
                      break;

                    case "Ident":
                      selector += token.text;
                      break;
                  }
                  break;
                case "SquareBracketBlock":
                  selectorState = CSSCompleter.CSS_SELECTOR_STATE_ATTRIBUTE;
                  scopeStack.push("[");
                  selector += "[";
                  break;

                case "WhiteSpace":
                  selectorState = CSSCompleter.CSS_SELECTOR_STATE_NULL;
                  selector && (selector += " ");
                  break;
              }
              break;

            case CSSCompleter.CSS_SELECTOR_STATE_ATTRIBUTE:
              switch (token.tokenType) {
                case "IncludeMatch":
                case "DashMatch":
                case "PrefixMatch":
                case "IncludeSuffixMatchMatch":
                case "SubstringMatch":
                  selector += token.text;
                  token = tokens[cursor++];
                  break;

                case "Delim":
                  if (token.text == "=") {
                    selectorState = CSSCompleter.CSS_SELECTOR_STATE_VALUE;
                    selector += token.text;
                  }
                  break;

                case "CloseSquareBracket":
                  if (scopeStack.at(-1) == "[") {
                    scopeStack.pop();
                  }

                  selectorState = CSSCompleter.CSS_SELECTOR_STATE_NULL;
                  selector += "]";
                  break;

                case "Ident":
                  selector += token.text;
                  break;

                case "QuotedString":
                  selector += token.value;
                  break;

                case "WhiteSpace":
                  selector && (selector += " ");
                  break;
              }
              break;

            case CSSCompleter.CSS_SELECTOR_STATE_VALUE:
              switch (token.tokenType) {
                case "Ident":
                  selector += token.text;
                  break;

                case "QuotedString":
                  selector += token.value;
                  break;

                case "CloseSquareBracket":
                  if (scopeStack.at(-1) == "[") {
                    scopeStack.pop();
                  }

                  selectorState = CSSCompleter.CSS_SELECTOR_STATE_NULL;
                  selector += "]";
                  break;

                case "WhiteSpace":
                  selector && (selector += " ");
                  break;
              }
              break;
          }
          break;

        case CSSCompleter.CSS_STATE_NULL:
          // From CSS_STATE_NULL state, we can go to either CSS_STATE_MEDIA or
          // CSS_STATE_SELECTOR.
          switch (token.tokenType) {
            case "Hash":
            case "IDHash":
              selectorState = CSSCompleter.CSS_SELECTOR_STATE_ID;
              selector = token.text;
              _state = CSSCompleter.CSS_STATE_SELECTOR;
              break;

            case "Ident":
              selectorState = CSSCompleter.CSS_SELECTOR_STATE_TAG;
              selector = token.text;
              _state = CSSCompleter.CSS_STATE_SELECTOR;
              break;

            case "Delim":
              if (token.text == ".") {
                selectorState = CSSCompleter.CSS_SELECTOR_STATE_CLASS;
                selector = ".";
                _state = CSSCompleter.CSS_STATE_SELECTOR;
                if (cursor <= tokIndex && tokens[cursor].tokenType == "Ident") {
                  token = tokens[cursor++];
                  selector += token.text;
                }
              } else if (token.text == "#") {
                // Lonely # char, that doesn't produce a Hash nor IDHash
                selectorState = CSSCompleter.CSS_SELECTOR_STATE_ID;
                selector = "#";
                _state = CSSCompleter.CSS_STATE_SELECTOR;
              } else if (token.text == "*") {
                selectorState = CSSCompleter.CSS_SELECTOR_STATE_TAG;
                selector = "*";
                _state = CSSCompleter.CSS_STATE_SELECTOR;
              }
              break;

            case "Colon":
              _state = CSSCompleter.CSS_STATE_SELECTOR;
              selectorState = CSSCompleter.CSS_SELECTOR_STATE_PSEUDO;
              selector += ":";
              if (cursor > tokIndex) {
                break;
              }

              token = tokens[cursor++];
              switch (token.tokenType) {
                case "Function":
                  if (token.value == "not") {
                    selectorBeforeNot = selector;
                    selector = "";
                    scopeStack.push("(");
                  } else {
                    selector += token.text;
                  }
                  selectorState = CSSCompleter.CSS_SELECTOR_STATE_NULL;
                  break;

                case "Ident":
                  selector += token.text;
                  break;
              }
              break;

            case "CloseSquareBracket":
              _state = CSSCompleter.CSS_STATE_SELECTOR;
              selectorState = CSSCompleter.CSS_SELECTOR_STATE_ATTRIBUTE;
              scopeStack.push("[");
              selector += "[";
              break;

            case "CurlyBracketBlock":
              if (scopeStack.at(-1) == "@m") {
                scopeStack.pop();
              }
              break;

            case "AtKeyword":
              // XXX: We should probably handle other at-rules (@container, @property, …)
              _state = token.value.startsWith("m")
                ? CSSCompleter.CSS_STATE_MEDIA
                : CSSCompleter.CSS_STATE_KEYFRAMES;
              break;
          }
          break;

        case CSSCompleter.CSS_STATE_MEDIA:
          // From CSS_STATE_MEDIA, we can only go to CSS_STATE_NULL state when
          // we hit the first '{'
          if (token.tokenType == "CurlyBracketBlock") {
            scopeStack.push("@m");
            _state = CSSCompleter.CSS_STATE_NULL;
          }
          break;

        case CSSCompleter.CSS_STATE_KEYFRAMES:
          // From CSS_STATE_KEYFRAMES, we can only go to CSS_STATE_FRAME state
          // when we hit the first '{'
          if (token.tokenType == "CurlyBracketBlock") {
            scopeStack.push("@k");
            _state = CSSCompleter.CSS_STATE_FRAME;
          }
          break;

        case CSSCompleter.CSS_STATE_FRAME:
          // From CSS_STATE_FRAME, we can either go to CSS_STATE_PROPERTY
          // state when we hit the first '{' or to CSS_STATE_SELECTOR when we
          // hit '}'
          if (token.tokenType == "CurlyBracketBlock") {
            scopeStack.push("f");
            _state = CSSCompleter.CSS_STATE_PROPERTY;
          } else if (token.tokenType == "CloseCurlyBracket") {
            if (scopeStack.at(-1) == "@k") {
              scopeStack.pop();
            }

            _state = CSSCompleter.CSS_STATE_NULL;
          }
          break;
      }
      if (_state == CSSCompleter.CSS_STATE_NULL) {
        if (!this.nullStates.length) {
          this.nullStates.push([
            token.loc.end.line,
            token.loc.end.column,
            [...scopeStack],
          ]);
          continue;
        }
        let tokenLine = token.loc.end.line;
        const tokenCh = token.loc.end.column;
        if (tokenLine == 0) {
          continue;
        }
        if (matchedStateIndex > -1) {
          tokenLine += this.nullStates[matchedStateIndex][0];
        }
        this.nullStates.push([tokenLine, tokenCh, [...scopeStack]]);
      }
    }
    // ^ while loop end

    this.state = _state;
    this.propertyName =
      _state == CSSCompleter.CSS_STATE_VALUE ? propertyName : null;
    this.selectorState =
      _state == CSSCompleter.CSS_STATE_SELECTOR ? selectorState : null;
    this.selectorBeforeNot =
      selectorBeforeNot == null ? null : selectorBeforeNot;
    if (token) {
      // If the source text is passed, we need to remove the part of the computed selector
      // after the caret (when sourceTokens are passed, the last token is already sliced,
      // so we'll get the expected value)
      if (!sourceTokens) {
        selector = selector.slice(
          0,
          selector.length + token.loc.end.column - column
        );
      }
      this.selector = selector;
    } else {
      this.selector = "";
    }
    this.selectors = selectors;

    if (token && token.tokenType != "WhiteSpace") {
      let text;
      if (
        token.tokenType === "IDHash" ||
        token.tokenType === "Hash" ||
        token.tokenType === "AtKeyword" ||
        token.tokenType === "Function" ||
        token.tokenType === "QuotedString"
      ) {
        text = token.value;
      } else {
        text = token.text;
      }
      this.completing = (
        sourceTokens
          ? text
          : // If the source text is passed, we need to remove the text after the caret
            // (when sourceTokens are passed, the last token is already sliced, so we'll
            // get the expected value)
            text.slice(0, column - token.loc.start.column)
      ).replace(/^[.#]$/, "");
    } else {
      this.completing = "";
    }
    // Special case the situation when the user just entered ":" after typing a
    // property name.
    if (this.completing == ":" && _state == CSSCompleter.CSS_STATE_VALUE) {
      this.completing = "";
    }

    // Special check for !important; case.
    if (
      token &&
      tokens[cursor - 2] &&
      tokens[cursor - 2].text == "!" &&
      this.completing == "important".slice(0, this.completing.length)
    ) {
      this.completing = "!" + this.completing;
    }
    return _state;
  }

  /**
   * Queries the DOM Walker actor for suggestions regarding the selector being
   * completed
   */
  suggestSelectors() {
    const walker = this.walker;
    if (!walker) {
      return Promise.resolve([]);
    }

    let query = this.selector;
    // Even though the selector matched atleast one node, there is still
    // possibility of suggestions.
    switch (this.selectorState) {
      case CSSCompleter.CSS_SELECTOR_STATE_NULL:
        if (this.completing === ",") {
          return Promise.resolve([]);
        }

        query += "*";
        break;

      case CSSCompleter.CSS_SELECTOR_STATE_TAG:
        query = query.slice(0, query.length - this.completing.length);
        break;

      case CSSCompleter.CSS_SELECTOR_STATE_ID:
      case CSSCompleter.CSS_SELECTOR_STATE_CLASS:
      case CSSCompleter.CSS_SELECTOR_STATE_PSEUDO:
        if (/^[.:#]$/.test(this.completing)) {
          query = query.slice(0, query.length - this.completing.length);
          this.completing = "";
        } else {
          query = query.slice(0, query.length - this.completing.length - 1);
        }
        break;
    }

    if (
      /[\s+>~]$/.test(query) &&
      this.selectorState != CSSCompleter.CSS_SELECTOR_STATE_ATTRIBUTE &&
      this.selectorState != CSSCompleter.CSS_SELECTOR_STATE_VALUE
    ) {
      query += "*";
    }

    // Set the values that this request was supposed to suggest to.
    this._currentQuery = query;
    return walker
      .getSuggestionsForQuery(
        query,
        this.completing,
        CSSCompleter.SELECTOR_STATE_STRING_BY_SYMBOL.get(this.selectorState)
      )
      .then(result => this.prepareSelectorResults(result));
  }

  /**
   * Prepares the selector suggestions returned by the walker actor.
   */
  prepareSelectorResults(result) {
    if (this._currentQuery != result.query) {
      return [];
    }

    const { suggestions } = result;
    const query = this.selector;
    const completion = [];

    // @backward-compat { version 140 } The shape of the returned value from getSuggestionsForQuery
    // changed in 140. This variable should be removed and considered as true when 140 hits release
    const suggestionNewShape =
      this.walker.traits.getSuggestionsForQueryWithoutCount;

    for (const suggestion of suggestions) {
      let value = suggestion[0];
      const state = suggestionNewShape ? suggestion[1] : suggestion[2];

      switch (this.selectorState) {
        case CSSCompleter.CSS_SELECTOR_STATE_ID:
        case CSSCompleter.CSS_SELECTOR_STATE_CLASS:
        case CSSCompleter.CSS_SELECTOR_STATE_PSEUDO:
          if (/^[.:#]$/.test(this.completing)) {
            value =
              query.slice(0, query.length - this.completing.length) + value;
          } else {
            value =
              query.slice(0, query.length - this.completing.length - 1) + value;
          }
          break;

        case CSSCompleter.CSS_SELECTOR_STATE_TAG:
          value = query.slice(0, query.length - this.completing.length) + value;
          break;

        case CSSCompleter.CSS_SELECTOR_STATE_NULL:
          value = query + value;
          break;

        default:
          value = query.slice(0, query.length - this.completing.length) + value;
      }

      const item = {
        label: value,
        preLabel: query,
        text: value,
      };

      // In case the query's state is tag and the item's state is id or class
      // adjust the preLabel
      if (
        this.selectorState === CSSCompleter.CSS_SELECTOR_STATE_TAG &&
        state === CSSCompleter.CSS_SELECTOR_STATE_CLASS
      ) {
        item.preLabel = "." + item.preLabel;
      }
      if (
        this.selectorState === CSSCompleter.CSS_SELECTOR_STATE_TAG &&
        state === CSSCompleter.CSS_SELECTOR_STATE_ID
      ) {
        item.preLabel = "#" + item.preLabel;
      }

      completion.push(item);

      if (completion.length > this.maxEntries - 1) {
        break;
      }
    }
    return completion;
  }

  /**
   * Returns CSS property name suggestions based on the input.
   *
   * @param startProp {String} Initial part of the property being completed.
   */
  completeProperties(startProp) {
    const finalList = [];
    if (!startProp) {
      return Promise.resolve(finalList);
    }

    const length = this.propertyNames.length;
    let i = 0,
      count = 0;
    for (; i < length && count < this.maxEntries; i++) {
      if (this.propertyNames[i].startsWith(startProp)) {
        count++;
        const propName = this.propertyNames[i];
        finalList.push({
          preLabel: startProp,
          label: propName,
          text: propName + ": ",
        });
      } else if (this.propertyNames[i] > startProp) {
        // We have crossed all possible matches alphabetically.
        break;
      }
    }
    return Promise.resolve(finalList);
  }

  /**
   * Returns CSS value suggestions based on the corresponding property.
   *
   * @param propName {String} The property to which the value being completed
   *        belongs.
   * @param startValue {String} Initial part of the value being completed.
   */
  completeValues(propName, startValue) {
    const finalList = [];
    const list = ["!important;", ...this.cssProperties.getValues(propName)];
    // If there is no character being completed, we are showing an initial list
    // of possible values. Skipping '!important' in this case.
    if (!startValue) {
      list.splice(0, 1);
    }

    const length = list.length;
    let i = 0,
      count = 0;
    for (; i < length && count < this.maxEntries; i++) {
      if (list[i].startsWith(startValue)) {
        count++;
        const value = list[i];
        finalList.push({
          preLabel: startValue,
          label: value,
          text: value,
        });
      } else if (list[i] > startValue) {
        // We have crossed all possible matches alphabetically.
        break;
      }
    }
    return Promise.resolve(finalList);
  }

  /**
   * A biased binary search in a sorted array where the middle element is
   * calculated based on the values at the lower and the upper index in each
   * iteration.
   *
   * This method returns the index of the closest null state from the passed
   * `line` argument. Once we have the closest null state, we can start applying
   * the state machine logic from that location instead of the absolute starting
   * of the CSS source. This speeds up the tokenizing and the state machine a
   * lot while using autocompletion at high line numbers in a CSS source.
   */
  findNearestNullState(line) {
    const arr = this.nullStates;
    let high = arr.length - 1;
    let low = 0;
    let target = 0;

    if (high < 0) {
      return -1;
    }
    if (arr[high][0] <= line) {
      return high;
    }
    if (arr[low][0] > line) {
      return -1;
    }

    while (high > low) {
      if (arr[low][0] <= line && arr[low[0] + 1] > line) {
        return low;
      }
      if (arr[high][0] > line && arr[high - 1][0] <= line) {
        return high - 1;
      }

      target =
        (((line - arr[low][0]) / (arr[high][0] - arr[low][0])) * (high - low)) |
        0;

      if (arr[target][0] <= line && arr[target + 1][0] > line) {
        return target;
      } else if (line > arr[target][0]) {
        low = target + 1;
        high--;
      } else {
        high = target - 1;
        low++;
      }
    }

    return -1;
  }

  /**
   * Invalidates the state cache for and above the line.
   */
  invalidateCache(line) {
    this.nullStates.length = this.findNearestNullState(line) + 1;
  }

  /**
   * Get the state information about a token surrounding the {line, ch} position
   *
   * @param {string} source
   *        The complete source of the CSS file. Unlike resolve state method,
   *        this method requires the full source.
   * @param {object} caret
   *        The line, ch position of the caret.
   *
   * @returns {object}
   *          An object containing the state of token covered by the caret.
   *          The object has following properties when the the state is
   *          "selector", "value" or "property", null otherwise:
   *           - state {string} one of CSS_STATES - "selector", "value" etc.
   *           - selector {string} The selector at the caret when `state` is
   *                      selector. OR
   *           - selectors {[string]} Array of selector strings in case when
   *                       `state` is "value" or "property"
   *           - propertyName {string} The property name at the current caret or
   *                          the property name corresponding to the value at
   *                          the caret.
   *           - value {string} The css value at the current caret.
   *           - loc {object} An object containing the starting and the ending
   *                 caret position of the whole selector, value or property.
   *                  - { start: {line, ch}, end: {line, ch}}
   */
  getInfoAt(source, caret) {
    const { line, ch } = caret;
    const sourceArray = source.split("\n");

    // Limits the input source till the {line, ch} caret position
    const limit = function () {
      // `line` is 0-based
      if (sourceArray.length <= line) {
        return source;
      }
      const list = sourceArray.slice(0, line + 1);
      list[line] = list[line].slice(0, ch);
      return list.join("\n");
    };

    const limitedSource = limit(source);

    // Ideally we should be using `cssTokenizer`, which parse incrementaly and returns a generator.
    // `cssTokenizerWithLineColumn` parses the whole `limitedSource` content right away
    // and returns an array of tokens. This can be a performance bottleneck,
    // but `resolveState` would go through all the tokens anyway, as well as `traverseBackward`,
    // which starts from the last token.
    const limitedSourceTokens = cssTokenizerWithLineColumn(limitedSource);
    const state = this.resolveState({
      sourceTokens: limitedSourceTokens,
    });
    const propertyName = this.propertyName;

    /**
     * Method to traverse forwards from the caret location to figure out the
     * ending point of a selector or css value.
     *
     * @param {function} check
     *        A method which takes the current state as an input and determines
     *        whether the state changed or not.
     */
    const traverseForward = check => {
      let forwardCurrentLine = line;
      let forwardCurrentSource = limitedSource;

      // loop to determine the end location of the property name/value/selector.
      do {
        let lineText = sourceArray[forwardCurrentLine];
        if (forwardCurrentLine == line) {
          lineText = lineText.substring(ch);
        }

        let prevToken = undefined;
        const tokensIterator = cssTokenizer(lineText);

        const ech = forwardCurrentLine == line ? ch : 0;
        for (let token of tokensIterator) {
          forwardCurrentSource += sourceArray[forwardCurrentLine].substring(
            ech + token.startOffset,
            ech + token.endOffset
          );

          // WhiteSpace cannot change state.
          if (token.tokenType == "WhiteSpace") {
            prevToken = token;
            continue;
          }

          const forwState = this.resolveState({
            source: forwardCurrentSource,
            line: forwardCurrentLine,
            column: token.endOffset + ech,
          });
          if (check(forwState)) {
            if (prevToken && prevToken.tokenType == "WhiteSpace") {
              token = prevToken;
            }
            return {
              line: forwardCurrentLine,
              ch: token.startOffset + ech,
            };
          }
          prevToken = token;
        }
        forwardCurrentSource += "\n";
      } while (++forwardCurrentLine < sourceArray.length);
      return null;
    };

    /**
     * Method to traverse backwards from the caret location to figure out the
     * starting point of a selector or css value.
     *
     * @param {function} check
     *        A method which takes the current state as an input and determines
     *        whether the state changed or not.
     * @param {boolean} isValue
     *        true if the traversal is being done for a css value state.
     */
    const traverseBackwards = (check, isValue) => {
      let token;
      let previousToken;
      const remainingTokens = Array.from(limitedSourceTokens);

      // Backward loop to determine the beginning location of the selector.
      while (((previousToken = token), (token = remainingTokens.pop()))) {
        // WhiteSpace cannot change state.
        if (token.tokenType == "WhiteSpace") {
          continue;
        }

        const backState = this.resolveState({
          sourceTokens: remainingTokens,
        });
        if (check(backState)) {
          if (previousToken?.tokenType == "WhiteSpace") {
            token = previousToken;
          }

          const loc = isValue ? token.loc.end : token.loc.start;
          return {
            line: loc.line,
            ch: loc.column,
          };
        }
      }
      return null;
    };

    if (state == CSSCompleter.CSS_STATE_SELECTOR) {
      // For selector state, the ending and starting point of the selector is
      // either when the state changes or the selector becomes empty and a
      // single selector can span multiple lines.
      // Backward loop to determine the beginning location of the selector.
      const start = traverseBackwards(backState => {
        return (
          backState != CSSCompleter.CSS_STATE_SELECTOR ||
          (this.selector == "" && this.selectorBeforeNot == null)
        );
      });

      // Forward loop to determine the ending location of the selector.
      const end = traverseForward(forwState => {
        return (
          forwState != CSSCompleter.CSS_STATE_SELECTOR ||
          (this.selector == "" && this.selectorBeforeNot == null)
        );
      });

      // Since we have start and end positions, figure out the whole selector.
      let selector = sourceArray.slice(start.line, end.line + 1);
      selector[selector.length - 1] = selector[selector.length - 1].substring(
        0,
        end.ch
      );
      selector[0] = selector[0].substring(start.ch);
      selector = selector.join("\n");
      return {
        state,
        selector,
        loc: {
          start,
          end,
        },
      };
    } else if (state == CSSCompleter.CSS_STATE_PROPERTY) {
      // A property can only be a single word and thus very easy to calculate.
      const tokensIterator = cssTokenizer(sourceArray[line]);
      for (const token of tokensIterator) {
        // Note that, because we're tokenizing a single line, the
        // token's offset is also the column number.
        if (token.startOffset <= ch && token.endOffset >= ch) {
          return {
            state,
            propertyName: token.text,
            selectors: this.selectors,
            loc: {
              start: {
                line,
                ch: token.startOffset,
              },
              end: {
                line,
                ch: token.endOffset,
              },
            },
          };
        }
      }
    } else if (state == CSSCompleter.CSS_STATE_VALUE) {
      // CSS value can be multiline too, so we go forward and backwards to
      // determine the bounds of the value at caret
      const start = traverseBackwards(
        backState => backState != CSSCompleter.CSS_STATE_VALUE,
        true
      );

      // Find the end of the value using a simple forward scan.
      const remainingSource = source.substring(limitedSource.length);
      const parser = new InspectorCSSParser(remainingSource);
      let end;
      while (true) {
        const token = parser.nextToken();
        if (
          !token ||
          token.tokenType === "Semicolon" ||
          token.tokenType === "CloseCurlyBracket"
        ) {
          // Done.  We're guaranteed to exit the loop once we reach
          // the end of the string.
          end = {
            line: parser.lineNumber + line,
            ch: parser.columnNumber,
          };
          if (end.line === line) {
            end.ch = end.ch + ch;
          }
          break;
        }
      }

      let value = sourceArray.slice(start.line, end.line + 1);
      value[value.length - 1] = value[value.length - 1].substring(0, end.ch);
      value[0] = value[0].substring(start.ch);
      value = value.join("\n");
      return {
        state,
        propertyName,
        selectors: this.selectors,
        value,
        loc: {
          start,
          end,
        },
      };
    }
    return null;
  }
}

module.exports = CSSCompleter;
