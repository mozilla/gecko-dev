/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {Cc, Ci, Cu} = require("chrome");
const {Promise: promise} = Cu.import("resource://gre/modules/Promise.jsm", {});
const {CssLogic} = require("devtools/styleinspector/css-logic");
const {InplaceEditor, editableField, editableItem} = require("devtools/shared/inplace-editor");
const {ELEMENT_STYLE, PSEUDO_ELEMENTS} = require("devtools/server/actors/styles");
const {gDevTools} = Cu.import("resource:///modules/devtools/gDevTools.jsm", {});
const {OutputParser} = require("devtools/output-parser");
const {PrefObserver, PREF_ORIG_SOURCES} = require("devtools/styleeditor/utils");
const {parseSingleValue, parseDeclarations} = require("devtools/styleinspector/css-parsing-utils");
const overlays = require("devtools/styleinspector/style-inspector-overlays");

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

const HTML_NS = "http://www.w3.org/1999/xhtml";
const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
const PREF_UA_STYLES = "devtools.inspector.showUserAgentStyles";
const PREF_DEFAULT_COLOR_UNIT = "devtools.defaultColorUnit";

/**
 * These regular expressions are adapted from firebug's css.js, and are
 * used to parse CSSStyleDeclaration's cssText attribute.
 */

// Used to split on css line separators
const CSS_LINE_RE = /(?:[^;\(]*(?:\([^\)]*?\))?[^;\(]*)*;?/g;

// Used to parse a single property line.
const CSS_PROP_RE = /\s*([^:\s]*)\s*:\s*(.*?)\s*(?:! (important))?;?$/;

// Used to parse an external resource from a property value
const CSS_RESOURCE_RE = /url\([\'\"]?(.*?)[\'\"]?\)/;

const IOService = Cc["@mozilla.org/network/io-service;1"]
                  .getService(Ci.nsIIOService);

function promiseWarn(err) {
  console.error(err);
  return promise.reject(err);
}

/**
 * To figure out how shorthand properties are interpreted by the
 * engine, we will set properties on a dummy element and observe
 * how their .style attribute reflects them as computed values.
 * This function creates the document in which those dummy elements
 * will be created.
 */
var gDummyPromise;
function createDummyDocument() {
  if (gDummyPromise) {
    return gDummyPromise;
  }
  const { getDocShell, create: makeFrame } = require("sdk/frame/utils");

  let frame = makeFrame(Services.appShell.hiddenDOMWindow.document, {
    nodeName: "iframe",
    namespaceURI: "http://www.w3.org/1999/xhtml",
    allowJavascript: false,
    allowPlugins: false,
    allowAuth: false
  });
  let docShell = getDocShell(frame);
  let eventTarget = docShell.chromeEventHandler;
  docShell.createAboutBlankContentViewer(Cc["@mozilla.org/nullprincipal;1"].createInstance(Ci.nsIPrincipal));
  let window = docShell.contentViewer.DOMDocument.defaultView;
  window.location = "data:text/html,<html></html>";
  let deferred = promise.defer();
  eventTarget.addEventListener("DOMContentLoaded", function handler(event) {
    eventTarget.removeEventListener("DOMContentLoaded", handler, false);
    deferred.resolve(window.document);
    frame.remove();
  }, false);
  gDummyPromise = deferred.promise;
  return gDummyPromise;
}

/**
 * Our model looks like this:
 *
 * ElementStyle:
 *   Responsible for keeping track of which properties are overridden.
 *   Maintains a list of Rule objects that apply to the element.
 * Rule:
 *   Manages a single style declaration or rule.
 *   Responsible for applying changes to the properties in a rule.
 *   Maintains a list of TextProperty objects.
 * TextProperty:
 *   Manages a single property from the cssText attribute of the
 *     relevant declaration.
 *   Maintains a list of computed properties that come from this
 *     property declaration.
 *   Changes to the TextProperty are sent to its related Rule for
 *     application.
 */

/**
 * ElementStyle maintains a list of Rule objects for a given element.
 *
 * @param {Element} aElement
 *        The element whose style we are viewing.
 * @param {object} aStore
 *        The ElementStyle can use this object to store metadata
 *        that might outlast the rule view, particularly the current
 *        set of disabled properties.
 * @param {PageStyleFront} aPageStyle
 *        Front for the page style actor that will be providing
 *        the style information.
 * @param {bool} aShowUserAgentStyles
 *        Should user agent styles be inspected?
 *
 * @constructor
 */
function ElementStyle(aElement, aStore, aPageStyle, aShowUserAgentStyles) {
  this.element = aElement;
  this.store = aStore || {};
  this.pageStyle = aPageStyle;
  this.showUserAgentStyles = aShowUserAgentStyles;

  // We don't want to overwrite this.store.userProperties so we only create it
  // if it doesn't already exist.
  if (!("userProperties" in this.store)) {
    this.store.userProperties = new UserProperties();
  }

  if (!("disabled" in this.store)) {
    this.store.disabled = new WeakMap();
  }
}

// We're exporting _ElementStyle for unit tests.
exports._ElementStyle = ElementStyle;

ElementStyle.prototype = {
  // The element we're looking at.
  element: null,

  // Empty, unconnected element of the same type as this node, used
  // to figure out how shorthand properties will be parsed.
  dummyElement: null,

  init: function()
  {
    // To figure out how shorthand properties are interpreted by the
    // engine, we will set properties on a dummy element and observe
    // how their .style attribute reflects them as computed values.
    return this.dummyElementPromise = createDummyDocument().then(document => {
      this.dummyElement = document.createElementNS(this.element.namespaceURI,
                                                   this.element.tagName);
      document.documentElement.appendChild(this.dummyElement);
      return this.dummyElement;
    }).then(null, promiseWarn);
  },

  destroy: function() {
    this.dummyElement = null;
    this.dummyElementPromise.then(dummyElement => {
      if (dummyElement.parentNode) {
        dummyElement.parentNode.removeChild(dummyElement);
      }
      this.dummyElementPromise = null;
    });
  },

  /**
   * Called by the Rule object when it has been changed through the
   * setProperty* methods.
   */
  _changed: function() {
    if (this.onChanged) {
      this.onChanged();
    }
  },

  /**
   * Refresh the list of rules to be displayed for the active element.
   * Upon completion, this.rules[] will hold a list of Rule objects.
   *
   * Returns a promise that will be resolved when the elementStyle is
   * ready.
   */
  populate: function() {
    let populated = this.pageStyle.getApplied(this.element, {
      inherited: true,
      matchedSelectors: true,
      filter: this.showUserAgentStyles ? "ua" : undefined,
    }).then(entries => {
      // Make sure the dummy element has been created before continuing...
      return this.dummyElementPromise.then(() => {
        if (this.populated != populated) {
          // Don't care anymore.
          return;
        }

        // Store the current list of rules (if any) during the population
        // process.  They will be reused if possible.
        this._refreshRules = this.rules;

        this.rules = [];

        for (let entry of entries) {
          this._maybeAddRule(entry);
        }

        // Mark overridden computed styles.
        this.markOverriddenAll();

        this._sortRulesForPseudoElement();

        // We're done with the previous list of rules.
        delete this._refreshRules;

        return null;
      });
    }).then(null, promiseWarn);
    this.populated = populated;
    return this.populated;
  },

  /**
   * Put pseudo elements in front of others.
   */
   _sortRulesForPseudoElement: function() {
      this.rules = this.rules.sort((a, b) => {
        return (a.pseudoElement || "z") > (b.pseudoElement || "z");
      });
   },

  /**
   * Add a rule if it's one we care about.  Filters out duplicates and
   * inherited styles with no inherited properties.
   *
   * @param {object} aOptions
   *        Options for creating the Rule, see the Rule constructor.
   *
   * @return {bool} true if we added the rule.
   */
  _maybeAddRule: function(aOptions) {
    // If we've already included this domRule (for example, when a
    // common selector is inherited), ignore it.
    if (aOptions.rule &&
        this.rules.some(function(rule) rule.domRule === aOptions.rule)) {
      return false;
    }

    if (aOptions.system) {
      return false;
    }

    let rule = null;

    // If we're refreshing and the rule previously existed, reuse the
    // Rule object.
    if (this._refreshRules) {
      for (let r of this._refreshRules) {
        if (r.matches(aOptions)) {
          rule = r;
          rule.refresh(aOptions);
          break;
        }
      }
    }

    // If this is a new rule, create its Rule object.
    if (!rule) {
      rule = new Rule(this, aOptions);
    }

    // Ignore inherited rules with no properties.
    if (aOptions.inherited && rule.textProps.length == 0) {
      return false;
    }

    this.rules.push(rule);
    return true;
  },

  /**
   * Calls markOverridden with all supported pseudo elements
   */
  markOverriddenAll: function() {
    this.markOverridden();
    for (let pseudo of PSEUDO_ELEMENTS) {
      this.markOverridden(pseudo);
    }
  },

  /**
   * Mark the properties listed in this.rules for a given pseudo element
   * with an overridden flag if an earlier property overrides it.
   * @param {string} pseudo
   *        Which pseudo element to flag as overridden.
   *        Empty string or undefined will default to no pseudo element.
   */
  markOverridden: function(pseudo="") {
    // Gather all the text properties applied by these rules, ordered
    // from more- to less-specific.
    let textProps = [];
    for (let rule of this.rules) {
      if (rule.pseudoElement == pseudo) {
        textProps = textProps.concat(rule.textProps.slice(0).reverse());
      }
    }

    // Gather all the computed properties applied by those text
    // properties.
    let computedProps = [];
    for (let textProp of textProps) {
      computedProps = computedProps.concat(textProp.computed);
    }

    // Walk over the computed properties.  As we see a property name
    // for the first time, mark that property's name as taken by this
    // property.
    //
    // If we come across a property whose name is already taken, check
    // its priority against the property that was found first:
    //
    //   If the new property is a higher priority, mark the old
    //   property overridden and mark the property name as taken by
    //   the new property.
    //
    //   If the new property is a lower or equal priority, mark it as
    //   overridden.
    //
    // _overriddenDirty will be set on each prop, indicating whether its
    // dirty status changed during this pass.
    let taken = {};
    for (let computedProp of computedProps) {
      let earlier = taken[computedProp.name];
      let overridden;
      if (earlier &&
          computedProp.priority === "important" &&
          earlier.priority !== "important") {
        // New property is higher priority.  Mark the earlier property
        // overridden (which will reverse its dirty state).
        earlier._overriddenDirty = !earlier._overriddenDirty;
        earlier.overridden = true;
        overridden = false;
      } else {
        overridden = !!earlier;
      }

      computedProp._overriddenDirty = (!!computedProp.overridden != overridden);
      computedProp.overridden = overridden;
      if (!computedProp.overridden && computedProp.textProp.enabled) {
        taken[computedProp.name] = computedProp;
      }
    }

    // For each TextProperty, mark it overridden if all of its
    // computed properties are marked overridden.  Update the text
    // property's associated editor, if any.  This will clear the
    // _overriddenDirty state on all computed properties.
    for (let textProp of textProps) {
      // _updatePropertyOverridden will return true if the
      // overridden state has changed for the text property.
      if (this._updatePropertyOverridden(textProp)) {
        textProp.updateEditor();
      }
    }
  },

  /**
   * Mark a given TextProperty as overridden or not depending on the
   * state of its computed properties.  Clears the _overriddenDirty state
   * on all computed properties.
   *
   * @param {TextProperty} aProp
   *        The text property to update.
   *
   * @return {bool} true if the TextProperty's overridden state (or any of its
   *         computed properties overridden state) changed.
   */
  _updatePropertyOverridden: function(aProp) {
    let overridden = true;
    let dirty = false;
    for each (let computedProp in aProp.computed) {
      if (!computedProp.overridden) {
        overridden = false;
      }
      dirty = computedProp._overriddenDirty || dirty;
      delete computedProp._overriddenDirty;
    }

    dirty = (!!aProp.overridden != overridden) || dirty;
    aProp.overridden = overridden;
    return dirty;
  }
};

/**
 * A single style rule or declaration.
 *
 * @param {ElementStyle} aElementStyle
 *        The ElementStyle to which this rule belongs.
 * @param {object} aOptions
 *        The information used to construct this rule.  Properties include:
 *          rule: A StyleRuleActor
 *          inherited: An element this rule was inherited from.  If omitted,
 *            the rule applies directly to the current element.
 *          isSystem: Is this a user agent style?
 * @constructor
 */
function Rule(aElementStyle, aOptions) {
  this.elementStyle = aElementStyle;
  this.domRule = aOptions.rule || null;
  this.style = aOptions.rule;
  this.matchedSelectors = aOptions.matchedSelectors || [];
  this.pseudoElement = aOptions.pseudoElement || "";

  this.isSystem = aOptions.isSystem;
  this.inherited = aOptions.inherited || null;
  this._modificationDepth = 0;

  if (this.domRule) {
    let parentRule = this.domRule.parentRule;
    if (parentRule && parentRule.type == Ci.nsIDOMCSSRule.MEDIA_RULE) {
      this.mediaText = parentRule.mediaText;
    }
  }

  // Populate the text properties with the style's current cssText
  // value, and add in any disabled properties from the store.
  this.textProps = this._getTextProperties();
  this.textProps = this.textProps.concat(this._getDisabledProperties());
}

Rule.prototype = {
  mediaText: "",

  get title() {
    if (this._title) {
      return this._title;
    }
    this._title = CssLogic.shortSource(this.sheet);
    if (this.domRule.type !== ELEMENT_STYLE) {
      this._title += ":" + this.ruleLine;
    }

    this._title = this._title + (this.mediaText ? " @media " + this.mediaText : "");
    return this._title;
  },

  get inheritedSource() {
    if (this._inheritedSource) {
      return this._inheritedSource;
    }
    this._inheritedSource = "";
    if (this.inherited) {
      let eltText = this.inherited.tagName.toLowerCase();
      if (this.inherited.id) {
        eltText += "#" + this.inherited.id;
      }
      this._inheritedSource =
        CssLogic._strings.formatStringFromName("rule.inheritedFrom", [eltText], 1);
    }
    return this._inheritedSource;
  },

  get selectorText() {
    return this.domRule.selectors ? this.domRule.selectors.join(", ") : CssLogic.l10n("rule.sourceElement");
  },

  /**
   * The rule's stylesheet.
   */
  get sheet() {
    return this.domRule ? this.domRule.parentStyleSheet : null;
  },

  /**
   * The rule's line within a stylesheet
   */
  get ruleLine() {
    return this.domRule ? this.domRule.line : null;
  },

  /**
   * The rule's column within a stylesheet
   */
  get ruleColumn() {
    return this.domRule ? this.domRule.column : null;
  },

  /**
   * Get display name for this rule based on the original source
   * for this rule's style sheet.
   *
   * @return {Promise}
   *         Promise which resolves with location as an object containing
   *         both the full and short version of the source string.
   */
  getOriginalSourceStrings: function() {
    if (this._originalSourceStrings) {
      return promise.resolve(this._originalSourceStrings);
    }
    return this.domRule.getOriginalLocation().then(({href, line}) => {
      let sourceStrings = {
        full: href + ":" + line,
        short: CssLogic.shortSource({href: href}) + ":" + line
      };

      this._originalSourceStrings = sourceStrings;
      return sourceStrings;
    });
  },

  /**
   * Returns true if the rule matches the creation options
   * specified.
   *
   * @param {object} aOptions
   *        Creation options.  See the Rule constructor for documentation.
   */
  matches: function(aOptions) {
    return this.style === aOptions.rule;
  },

  /**
   * Create a new TextProperty to include in the rule.
   *
   * @param {string} aName
   *        The text property name (such as "background" or "border-top").
   * @param {string} aValue
   *        The property's value (not including priority).
   * @param {string} aPriority
   *        The property's priority (either "important" or an empty string).
   * @param {TextProperty} aSiblingProp
   *        Optional, property next to which the new property will be added.
   */
  createProperty: function(aName, aValue, aPriority, aSiblingProp) {
    let prop = new TextProperty(this, aName, aValue, aPriority);

    if (aSiblingProp) {
      let ind = this.textProps.indexOf(aSiblingProp);
      this.textProps.splice(ind + 1, 0, prop);
    }
    else {
      this.textProps.push(prop);
    }

    this.applyProperties();
    return prop;
  },

  /**
   * Reapply all the properties in this rule, and update their
   * computed styles. Store disabled properties in the element
   * style's store. Will re-mark overridden properties.
   *
   * @param {string} [aName]
   *        A text property name (such as "background" or "border-top") used
   *        when calling from setPropertyValue & setPropertyName to signify
   *        that the property should be saved in store.userProperties.
   */
  applyProperties: function(aModifications, aName) {
    this.elementStyle.markOverriddenAll();

    if (!aModifications) {
      aModifications = this.style.startModifyingProperties();
    }
    let disabledProps = [];
    let store = this.elementStyle.store;

    for (let prop of this.textProps) {
      if (!prop.enabled) {
        disabledProps.push({
          name: prop.name,
          value: prop.value,
          priority: prop.priority
        });
        continue;
      }
      if (prop.value.trim() === "") {
        continue;
      }

      aModifications.setProperty(prop.name, prop.value, prop.priority);

      prop.updateComputed();
    }

    // Store disabled properties in the disabled store.
    let disabled = this.elementStyle.store.disabled;
    if (disabledProps.length > 0) {
      disabled.set(this.style, disabledProps);
    } else {
      disabled.delete(this.style);
    }

    let promise = aModifications.apply().then(() => {
      let cssProps = {};
      for (let cssProp of parseDeclarations(this.style.cssText)) {
        cssProps[cssProp.name] = cssProp;
      }

      for (let textProp of this.textProps) {
        if (!textProp.enabled) {
          continue;
        }
        let cssProp = cssProps[textProp.name];

        if (!cssProp) {
          cssProp = {
            name: textProp.name,
            value: "",
            priority: ""
          };
        }

        if (aName && textProp.name == aName) {
          store.userProperties.setProperty(
            this.style,
            textProp.name,
            textProp.value);
        }
        textProp.priority = cssProp.priority;
      }

      this.elementStyle.markOverriddenAll();

      if (promise === this._applyingModifications) {
        this._applyingModifications = null;
      }

      this.elementStyle._changed();
    }).then(null, promiseWarn);

    this._applyingModifications = promise;
    return promise;
  },

  /**
   * Renames a property.
   *
   * @param {TextProperty} aProperty
   *        The property to rename.
   * @param {string} aName
   *        The new property name (such as "background" or "border-top").
   */
  setPropertyName: function(aProperty, aName) {
    if (aName === aProperty.name) {
      return;
    }
    let modifications = this.style.startModifyingProperties();
    modifications.removeProperty(aProperty.name);
    aProperty.name = aName;
    this.applyProperties(modifications, aName);
  },

  /**
   * Sets the value and priority of a property, then reapply all properties.
   *
   * @param {TextProperty} aProperty
   *        The property to manipulate.
   * @param {string} aValue
   *        The property's value (not including priority).
   * @param {string} aPriority
   *        The property's priority (either "important" or an empty string).
   */
  setPropertyValue: function(aProperty, aValue, aPriority) {
    if (aValue === aProperty.value && aPriority === aProperty.priority) {
      return;
    }

    aProperty.value = aValue;
    aProperty.priority = aPriority;
    this.applyProperties(null, aProperty.name);
  },

  /**
   * Just sets the value and priority of a property, in order to preview its
   * effect on the content document.
   *
   * @param {TextProperty} aProperty
   *        The property which value will be previewed
   * @param {String} aValue
   *        The value to be used for the preview
   * @param {String} aPriority
   *        The property's priority (either "important" or an empty string).
   */
  previewPropertyValue: function(aProperty, aValue, aPriority) {
    let modifications = this.style.startModifyingProperties();
    modifications.setProperty(aProperty.name, aValue, aPriority);
    modifications.apply();
  },

  /**
   * Disables or enables given TextProperty.
   *
   * @param {TextProperty} aProperty
   *        The property to enable/disable
   * @param {Boolean} aValue
   */
  setPropertyEnabled: function(aProperty, aValue) {
    aProperty.enabled = !!aValue;
    let modifications = this.style.startModifyingProperties();
    if (!aProperty.enabled) {
      modifications.removeProperty(aProperty.name);
    }
    this.applyProperties(modifications);
  },

  /**
   * Remove a given TextProperty from the rule and update the rule
   * accordingly.
   *
   * @param {TextProperty} aProperty
   *        The property to be removed
   */
  removeProperty: function(aProperty) {
    this.textProps = this.textProps.filter(function(prop) prop != aProperty);
    let modifications = this.style.startModifyingProperties();
    modifications.removeProperty(aProperty.name);
    // Need to re-apply properties in case removing this TextProperty
    // exposes another one.
    this.applyProperties(modifications);
  },

  /**
   * Get the list of TextProperties from the style.  Needs
   * to parse the style's cssText.
   */
  _getTextProperties: function() {
    let textProps = [];
    let store = this.elementStyle.store;
    let props = parseDeclarations(this.style.cssText);
    for (let prop of props) {
      let name = prop.name;
      if (this.inherited && !domUtils.isInheritedProperty(name)) {
        continue;
      }
      let value = store.userProperties.getProperty(this.style, name, prop.value);
      let textProp = new TextProperty(this, name, value, prop.priority);
      textProps.push(textProp);
    }

    return textProps;
  },

  /**
   * Return the list of disabled properties from the store for this rule.
   */
  _getDisabledProperties: function() {
    let store = this.elementStyle.store;

    // Include properties from the disabled property store, if any.
    let disabledProps = store.disabled.get(this.style);
    if (!disabledProps) {
      return [];
    }

    let textProps = [];

    for each (let prop in disabledProps) {
      let value = store.userProperties.getProperty(this.style, prop.name, prop.value);
      let textProp = new TextProperty(this, prop.name, value, prop.priority);
      textProp.enabled = false;
      textProps.push(textProp);
    }

    return textProps;
  },

  /**
   * Reread the current state of the rules and rebuild text
   * properties as needed.
   */
  refresh: function(aOptions) {
    this.matchedSelectors = aOptions.matchedSelectors || [];
    let newTextProps = this._getTextProperties();

    // Update current properties for each property present on the style.
    // This will mark any touched properties with _visited so we
    // can detect properties that weren't touched (because they were
    // removed from the style).
    // Also keep track of properties that didn't exist in the current set
    // of properties.
    let brandNewProps = [];
    for (let newProp of newTextProps) {
      if (!this._updateTextProperty(newProp)) {
        brandNewProps.push(newProp);
      }
    }

    // Refresh editors and disabled state for all the properties that
    // were updated.
    for (let prop of this.textProps) {
      // Properties that weren't touched during the update
      // process must no longer exist on the node.  Mark them disabled.
      if (!prop._visited) {
        prop.enabled = false;
        prop.updateEditor();
      } else {
        delete prop._visited;
      }
    }

    // Add brand new properties.
    this.textProps = this.textProps.concat(brandNewProps);

    // Refresh the editor if one already exists.
    if (this.editor) {
      this.editor.populate();
    }
  },

  /**
   * Update the current TextProperties that match a given property
   * from the cssText.  Will choose one existing TextProperty to update
   * with the new property's value, and will disable all others.
   *
   * When choosing the best match to reuse, properties will be chosen
   * by assigning a rank and choosing the highest-ranked property:
   *   Name, value, and priority match, enabled. (6)
   *   Name, value, and priority match, disabled. (5)
   *   Name and value match, enabled. (4)
   *   Name and value match, disabled. (3)
   *   Name matches, enabled. (2)
   *   Name matches, disabled. (1)
   *
   * If no existing properties match the property, nothing happens.
   *
   * @param {TextProperty} aNewProp
   *        The current version of the property, as parsed from the
   *        cssText in Rule._getTextProperties().
   *
   * @return {bool} true if a property was updated, false if no properties
   *         were updated.
   */
  _updateTextProperty: function(aNewProp) {
    let match = { rank: 0, prop: null };

    for each (let prop in this.textProps) {
      if (prop.name != aNewProp.name)
        continue;

      // Mark this property visited.
      prop._visited = true;

      // Start at rank 1 for matching name.
      let rank = 1;

      // Value and Priority matches add 2 to the rank.
      // Being enabled adds 1.  This ranks better matches higher,
      // with priority breaking ties.
      if (prop.value === aNewProp.value) {
        rank += 2;
        if (prop.priority === aNewProp.priority) {
          rank += 2;
        }
      }

      if (prop.enabled) {
        rank += 1;
      }

      if (rank > match.rank) {
        if (match.prop) {
          // We outrank a previous match, disable it.
          match.prop.enabled = false;
          match.prop.updateEditor();
        }
        match.rank = rank;
        match.prop = prop;
      } else if (rank) {
        // A previous match outranks us, disable ourself.
        prop.enabled = false;
        prop.updateEditor();
      }
    }

    // If we found a match, update its value with the new text property
    // value.
    if (match.prop) {
      match.prop.set(aNewProp);
      return true;
    }

    return false;
  },

  /**
   * Jump between editable properties in the UI.  Will begin editing the next
   * name, if possible.  If this is the last element in the set, then begin
   * editing the previous value.  If this is the *only* element in the set,
   * then settle for focusing the new property editor.
   *
   * @param {TextProperty} aTextProperty
   *        The text property that will be left to focus on a sibling.
   *
   */
  editClosestTextProperty: function(aTextProperty) {
    let index = this.textProps.indexOf(aTextProperty);
    let previous = false;

    // If this is the last element, move to the previous instead of next
    if (index === this.textProps.length - 1) {
      index = index - 1;
      previous = true;
    }
    else {
      index = index + 1;
    }

    let nextProp = this.textProps[index];

    // If possible, begin editing the next name or previous value.
    // Otherwise, settle for focusing the new property element.
    if (nextProp) {
      if (previous) {
        nextProp.editor.valueSpan.click();
      } else {
        nextProp.editor.nameSpan.click();
      }
    } else {
      aTextProperty.rule.editor.closeBrace.focus();
    }
  }
};

/**
 * A single property in a rule's cssText.
 *
 * @param {Rule} aRule
 *        The rule this TextProperty came from.
 * @param {string} aName
 *        The text property name (such as "background" or "border-top").
 * @param {string} aValue
 *        The property's value (not including priority).
 * @param {string} aPriority
 *        The property's priority (either "important" or an empty string).
 *
 */
function TextProperty(aRule, aName, aValue, aPriority) {
  this.rule = aRule;
  this.name = aName;
  this.value = aValue;
  this.priority = aPriority;
  this.enabled = true;
  this.updateComputed();
}

TextProperty.prototype = {
  /**
   * Update the editor associated with this text property,
   * if any.
   */
  updateEditor: function() {
    if (this.editor) {
      this.editor.update();
    }
  },

  /**
   * Update the list of computed properties for this text property.
   */
  updateComputed: function() {
    if (!this.name) {
      return;
    }

    // This is a bit funky.  To get the list of computed properties
    // for this text property, we'll set the property on a dummy element
    // and see what the computed style looks like.
    let dummyElement = this.rule.elementStyle.dummyElement;
    let dummyStyle = dummyElement.style;
    dummyStyle.cssText = "";
    dummyStyle.setProperty(this.name, this.value, this.priority);

    this.computed = [];
    for (let i = 0, n = dummyStyle.length; i < n; i++) {
      let prop = dummyStyle.item(i);
      this.computed.push({
        textProp: this,
        name: prop,
        value: dummyStyle.getPropertyValue(prop),
        priority: dummyStyle.getPropertyPriority(prop),
      });
    }
  },

  /**
   * Set all the values from another TextProperty instance into
   * this TextProperty instance.
   *
   * @param {TextProperty} aOther
   *        The other TextProperty instance.
   */
  set: function(aOther) {
    let changed = false;
    for (let item of ["name", "value", "priority", "enabled"]) {
      if (this[item] != aOther[item]) {
        this[item] = aOther[item];
        changed = true;
      }
    }

    if (changed) {
      this.updateEditor();
    }
  },

  setValue: function(aValue, aPriority) {
    this.rule.setPropertyValue(this, aValue, aPriority);
    this.updateEditor();
  },

  setName: function(aName) {
    this.rule.setPropertyName(this, aName);
    this.updateEditor();
  },

  setEnabled: function(aValue) {
    this.rule.setPropertyEnabled(this, aValue);
    this.updateEditor();
  },

  remove: function() {
    this.rule.removeProperty(this);
  }
};

/**
 * View hierarchy mostly follows the model hierarchy.
 *
 * CssRuleView:
 *   Owns an ElementStyle and creates a list of RuleEditors for its
 *    Rules.
 * RuleEditor:
 *   Owns a Rule object and creates a list of TextPropertyEditors
 *     for its TextProperties.
 *   Manages creation of new text properties.
 * TextPropertyEditor:
 *   Owns a TextProperty object.
 *   Manages changes to the TextProperty.
 *   Can be expanded to display computed properties.
 *   Can mark a property disabled or enabled.
 */

/**
 * CssRuleView is a view of the style rules and declarations that
 * apply to a given element.  After construction, the 'element'
 * property will be available with the user interface.
 *
 * @param {Inspector} aInspector
 * @param {Document} aDoc
 *        The document that will contain the rule view.
 * @param {object} aStore
 *        The CSS rule view can use this object to store metadata
 *        that might outlast the rule view, particularly the current
 *        set of disabled properties.
 * @param {PageStyleFront} aPageStyle
 *        The PageStyleFront for communicating with the remote server.
 * @constructor
 */
function CssRuleView(aInspector, aDoc, aStore, aPageStyle) {
  this.inspector = aInspector;
  this.doc = aDoc;
  this.store = aStore || {};
  this.pageStyle = aPageStyle;
  this.element = this.doc.createElementNS(HTML_NS, "div");
  this.element.className = "ruleview devtools-monospace";
  this.element.flex = 1;

  this._outputParser = new OutputParser();

  this._buildContextMenu = this._buildContextMenu.bind(this);
  this._contextMenuUpdate = this._contextMenuUpdate.bind(this);
  this._onSelectAll = this._onSelectAll.bind(this);
  this._onCopy = this._onCopy.bind(this);
  this._onCopyColor = this._onCopyColor.bind(this);
  this._onToggleOrigSources = this._onToggleOrigSources.bind(this);

  this.element.addEventListener("copy", this._onCopy);

  this._handlePrefChange = this._handlePrefChange.bind(this);
  this._onSourcePrefChanged = this._onSourcePrefChanged.bind(this);

  this._prefObserver = new PrefObserver("devtools.");
  this._prefObserver.on(PREF_ORIG_SOURCES, this._onSourcePrefChanged);
  this._prefObserver.on(PREF_UA_STYLES, this._handlePrefChange);
  this._prefObserver.on(PREF_DEFAULT_COLOR_UNIT, this._handlePrefChange);

  this.showUserAgentStyles = Services.prefs.getBoolPref(PREF_UA_STYLES);

  let options = {
    autoSelect: true,
    theme: "auto"
  };
  this.popup = new AutocompletePopup(aDoc.defaultView.parent.document, options);

  this._buildContextMenu();
  this._showEmpty();

  // Add the tooltips and highlighters to the view
  this.tooltips = new overlays.TooltipsOverlay(this);
  this.tooltips.addToView();
  this.highlighters = new overlays.HighlightersOverlay(this);
  this.highlighters.addToView();
}

exports.CssRuleView = CssRuleView;

CssRuleView.prototype = {
  // The element that we're inspecting.
  _viewedElement: null,

  /**
   * Build the context menu.
   */
  _buildContextMenu: function() {
    let doc = this.doc.defaultView.parent.document;

    this._contextmenu = doc.createElementNS(XUL_NS, "menupopup");
    this._contextmenu.addEventListener("popupshowing", this._contextMenuUpdate);
    this._contextmenu.id = "rule-view-context-menu";

    this.menuitemSelectAll = createMenuItem(this._contextmenu, {
      label: "ruleView.contextmenu.selectAll",
      accesskey: "ruleView.contextmenu.selectAll.accessKey",
      command: this._onSelectAll
    });
    this.menuitemCopy = createMenuItem(this._contextmenu, {
      label: "ruleView.contextmenu.copy",
      accesskey: "ruleView.contextmenu.copy.accessKey",
      command: this._onCopy
    });
    this.menuitemCopyColor = createMenuItem(this._contextmenu, {
      label: "ruleView.contextmenu.copyColor",
      accesskey: "ruleView.contextmenu.copyColor.accessKey",
      command: this._onCopyColor
    });
    this.menuitemSources= createMenuItem(this._contextmenu, {
      label: "ruleView.contextmenu.showOrigSources",
      accesskey: "ruleView.contextmenu.showOrigSources.accessKey",
      command: this._onToggleOrigSources
    });

    let popupset = doc.documentElement.querySelector("popupset");
    if (!popupset) {
      popupset = doc.createElementNS(XUL_NS, "popupset");
      doc.documentElement.appendChild(popupset);
    }

    popupset.appendChild(this._contextmenu);
  },

  /**
   * Update the context menu. This means enabling or disabling menuitems as
   * appropriate.
   */
  _contextMenuUpdate: function() {
    let win = this.doc.defaultView;

    // Copy selection.
    let selection = win.getSelection();
    let copy;

    if (selection.toString()) {
      // Panel text selected
      copy = true;
    } else if (selection.anchorNode) {
      // input type="text"
      let { selectionStart, selectionEnd } = this.doc.popupNode;

      if (isFinite(selectionStart) && isFinite(selectionEnd) &&
          selectionStart !== selectionEnd) {
        copy = true;
      }
    } else {
      // No text selected, disable copy.
      copy = false;
    }

    this.menuitemCopyColor.hidden = !this._isColorPopup();
    this.menuitemCopy.disabled = !copy;

    let label = "ruleView.contextmenu.showOrigSources";
    if (Services.prefs.getBoolPref(PREF_ORIG_SOURCES)) {
      label = "ruleView.contextmenu.showCSSSources";
    }
    this.menuitemSources.setAttribute("label",
                                      _strings.GetStringFromName(label));

    let accessKey = label + ".accessKey";
    this.menuitemSources.setAttribute("accesskey",
                                      _strings.GetStringFromName(accessKey));
  },

  /**
   * Get the type of a given node in the rule-view
   * @param {DOMNode} node The node which we want information about
   * @return {Object} The type information object contains the following props:
   * - type {String} One of the VIEW_NODE_XXX_TYPE const in
   *   style-inspector-overlays
   * - value {Object} Depends on the type of the node
   * returns null of the node isn't anything we care about
   */
  getNodeInfo: function(node) {
    let type, value;
    let classes = node.classList;
    let prop = getParentTextProperty(node);

    if (classes.contains("ruleview-propertyname") && prop) {
      type = overlays.VIEW_NODE_PROPERTY_TYPE;
      value = {
        property: node.textContent,
        value: getPropertyNameAndValue(node).value,
        enabled: prop.enabled,
        overridden: prop.overridden,
        pseudoElement: prop.rule.pseudoElement,
        sheetHref: prop.rule.domRule.href
      };
    } else if (classes.contains("ruleview-propertyvalue") && prop) {
      type = overlays.VIEW_NODE_VALUE_TYPE;
      value = {
        property: getPropertyNameAndValue(node).name,
        value: node.textContent,
        enabled: prop.enabled,
        overridden: prop.overridden,
        pseudoElement: prop.rule.pseudoElement,
        sheetHref: prop.rule.domRule.href
      };
    } else if (classes.contains("theme-link") && prop) {
      type = overlays.VIEW_NODE_IMAGE_URL_TYPE;
      value = {
        property: getPropertyNameAndValue(node).name,
        value: node.parentNode.textContent,
        url: node.textContent,
        enabled: prop.enabled,
        overridden: prop.overridden,
        pseudoElement: prop.rule.pseudoElement,
        sheetHref: prop.rule.domRule.href
      };
    } else if (classes.contains("ruleview-selector-unmatched") ||
               classes.contains("ruleview-selector-matched")) {
      type = overlays.VIEW_NODE_SELECTOR_TYPE;
      value = node.textContent;
    } else {
      return null;
    }

    return {
      type: type,
      value: value
    };
  },

  /**
   * A helper that determines if the popup was opened with a click to a color
   * value and saves the color to this._colorToCopy.
   *
   * @return {Boolean}
   *         true if click on color opened the popup, false otherwise.
   */
  _isColorPopup: function () {
    this._colorToCopy = "";

    let trigger = this.doc.popupNode;
    if (!trigger) {
      return false;
    }

    let container = (trigger.nodeType == trigger.TEXT_NODE) ?
                     trigger.parentElement : trigger;

    let isColorNode = el => el.dataset && "color" in el.dataset;

    while (!isColorNode(container)) {
      container = container.parentNode;
      if (!container) {
        return false;
      }
    }

    this._colorToCopy = container.dataset["color"];
    return true;
  },

  /**
   * Select all text.
   */
  _onSelectAll: function() {
    let win = this.doc.defaultView;
    let selection = win.getSelection();

    selection.selectAllChildren(this.doc.documentElement);
  },

  /**
   * Copy selected text from the rule view.
   *
   * @param {Event} event
   *        The event object.
   */
  _onCopy: function(event) {
    try {
      let target = event.target;
      let text;

      if (event.target.nodeName === "menuitem") {
        target = this.doc.popupNode;
      }

      if (target.nodeName == "input") {
        let start = Math.min(target.selectionStart, target.selectionEnd);
        let end = Math.max(target.selectionStart, target.selectionEnd);
        let count = end - start;
        text = target.value.substr(start, count);
      } else {
        let win = this.doc.defaultView;
        let selection = win.getSelection();

        text = selection.toString();

        // Remove any double newlines.
        text = text.replace(/(\r?\n)\r?\n/g, "$1");

        // Remove "inline"
        let inline = _strings.GetStringFromName("rule.sourceInline");
        let rx = new RegExp("^" + inline + "\\r?\\n?", "g");
        text = text.replace(rx, "");
      }

      clipboardHelper.copyString(text, this.doc);
      event.preventDefault();
    } catch(e) {
      console.error(e);
    }
  },

  /**
   * Copy the most recently selected color value to clipboard.
   */
  _onCopyColor: function() {
    clipboardHelper.copyString(this._colorToCopy, this.styleDocument);
  },

  /**
   *  Toggle the original sources pref.
   */
  _onToggleOrigSources: function() {
    let isEnabled = Services.prefs.getBoolPref(PREF_ORIG_SOURCES);
    Services.prefs.setBoolPref(PREF_ORIG_SOURCES, !isEnabled);
  },

  setPageStyle: function(aPageStyle) {
    this.pageStyle = aPageStyle;
  },

  /**
   * Return {bool} true if the rule view currently has an input editor visible.
   */
  get isEditing() {
    return this.element.querySelectorAll(".styleinspector-propertyeditor").length > 0
      || this.tooltips.colorPicker.tooltip.isShown();
  },

  _handlePrefChange: function(pref) {
    if (pref === PREF_UA_STYLES) {
      this.showUserAgentStyles = Services.prefs.getBoolPref(pref);
    }

    // Reselect the currently selected element
    let refreshOnPrefs = [PREF_UA_STYLES, PREF_DEFAULT_COLOR_UNIT];
    if (refreshOnPrefs.indexOf(pref) > -1) {
      let element = this._viewedElement;
      this._viewedElement = null;
      this.highlight(element);
    }
  },

  _onSourcePrefChanged: function() {
    if (this.menuitemSources) {
      let isEnabled = Services.prefs.getBoolPref(PREF_ORIG_SOURCES);
      this.menuitemSources.setAttribute("checked", isEnabled);
    }

    // update text of source links
    for (let rule of this._elementStyle.rules) {
      if (rule.editor) {
        rule.editor.updateSourceLink();
      }
    }
  },

  destroy: function() {
    this.clear();

    gDummyPromise = null;

    this._prefObserver.off(PREF_ORIG_SOURCES, this._onSourcePrefChanged);
    this._prefObserver.off(PREF_UA_STYLES, this._handlePrefChange);
    this._prefObserver.off(PREF_DEFAULT_COLOR_UNIT, this._handlePrefChange);
    this._prefObserver.destroy();

    this.element.removeEventListener("copy", this._onCopy);
    delete this._onCopy;

    delete this._outputParser;

    // Remove context menu
    if (this._contextmenu) {
      // Destroy the Select All menuitem.
      this.menuitemSelectAll.removeEventListener("command", this._onSelectAll);
      this.menuitemSelectAll = null;

      // Destroy the Copy menuitem.
      this.menuitemCopy.removeEventListener("command", this._onCopy);
      this.menuitemCopy = null;

      // Destroy Copy Color menuitem.
      this.menuitemCopyColor.removeEventListener("command", this._onCopyColor);
      this.menuitemCopyColor = null;

      this.menuitemSources.removeEventListener("command", this._onToggleOrigSources);
      this.menuitemSources = null;

      // Destroy the context menu.
      this._contextmenu.removeEventListener("popupshowing", this._contextMenuUpdate);
      this._contextmenu.parentNode.removeChild(this._contextmenu);
      this._contextmenu = null;
    }

    // We manage the popupNode ourselves so we also need to destroy it.
    this.doc.popupNode = null;

    this.tooltips.destroy();
    this.highlighters.destroy();

    if (this.element.parentNode) {
      this.element.parentNode.removeChild(this.element);
    }

    if (this.elementStyle) {
      this.elementStyle.destroy();
    }

    this.popup.destroy();
  },

  /**
   * Update the highlighted element.
   *
   * @param {NodeActor} aElement
   *        The node whose style rules we'll inspect.
   */
  highlight: function(aElement) {
    if (this._viewedElement === aElement) {
      return promise.resolve(undefined);
    }

    this.clear();

    this._viewedElement = aElement;
    if (!this._viewedElement) {
      this._showEmpty();
      return promise.resolve(undefined);
    }

    this._elementStyle = new ElementStyle(aElement, this.store,
      this.pageStyle, this.showUserAgentStyles);

    return this._elementStyle.init().then(() => {
      if (this._viewedElement === aElement) {
        return this._populate();
      }
    }).then(() => {
      if (this._viewedElement === aElement) {
        this._elementStyle.onChanged = () => {
          this._changed();
        };
      }
    }).then(null, console.error);
  },

  /**
   * Update the rules for the currently highlighted element.
   */
  refreshPanel: function() {
    // Ignore refreshes during editing or when no element is selected.
    if (this.isEditing || !this._elementStyle) {
      return;
    }

    // Repopulate the element style.
    this._populate(true);
  },

  _populate: function(clearRules = false) {
    let elementStyle = this._elementStyle;
    return this._elementStyle.populate().then(() => {
      if (this._elementStyle != elementStyle) {
        return;
      }

      if (clearRules) {
        this._clearRules();
      }
      this._createEditors();

      // Notify anyone that cares that we refreshed.
      var evt = this.doc.createEvent("Events");
      evt.initEvent("CssRuleViewRefreshed", true, false);
      this.element.dispatchEvent(evt);
      return undefined;
    }).then(null, promiseWarn);
  },

  /**
   * Show the user that the rule view has no node selected.
   */
  _showEmpty: function() {
    if (this.doc.getElementById("noResults") > 0) {
      return;
    }

    createChild(this.element, "div", {
      id: "noResults",
      textContent: CssLogic.l10n("rule.empty")
    });
  },

  /**
   * Clear the rules.
   */
  _clearRules: function() {
    while (this.element.hasChildNodes()) {
      this.element.removeChild(this.element.lastChild);
    }
  },

  /**
   * Clear the rule view.
   */
  clear: function() {
    this._clearRules();
    this._viewedElement = null;
    this._elementStyle = null;
  },

  /**
   * Called when the user has made changes to the ElementStyle.
   * Emits an event that clients can listen to.
   */
  _changed: function() {
    var evt = this.doc.createEvent("Events");
    evt.initEvent("CssRuleViewChanged", true, false);
    this.element.dispatchEvent(evt);
  },

  /**
   * Text for header that shows above rules for this element
   */
  get selectedElementLabel() {
    if (this._selectedElementLabel) {
      return this._selectedElementLabel;
    }
    this._selectedElementLabel = CssLogic.l10n("rule.selectedElement");
    return this._selectedElementLabel;
  },

  /**
   * Text for header that shows above rules for pseudo elements
   */
  get pseudoElementLabel() {
    if (this._pseudoElementLabel) {
      return this._pseudoElementLabel;
    }
    this._pseudoElementLabel = CssLogic.l10n("rule.pseudoElement");
    return this._pseudoElementLabel;
  },

  togglePseudoElementVisibility: function(value) {
    this._showPseudoElements = !!value;
    let isOpen = this.showPseudoElements;

    Services.prefs.setBoolPref("devtools.inspector.show_pseudo_elements",
      isOpen);

    this.element.classList.toggle("show-pseudo-elements", isOpen);

    if (this.pseudoElementTwisty) {
      if (isOpen) {
        this.pseudoElementTwisty.setAttribute("open", "true");
      }
      else {
        this.pseudoElementTwisty.removeAttribute("open");
      }
    }
  },

  get showPseudoElements() {
    if (this._showPseudoElements === undefined) {
      this._showPseudoElements =
        Services.prefs.getBoolPref("devtools.inspector.show_pseudo_elements");
    }
    return this._showPseudoElements;
  },

  _getRuleViewHeaderClassName: function(isPseudo) {
    let baseClassName = "theme-gutter ruleview-header";
    return isPseudo ? baseClassName + " ruleview-expandable-header" : baseClassName;
  },

  /**
   * Creates editor UI for each of the rules in _elementStyle.
   */
  _createEditors: function() {
    // Run through the current list of rules, attaching
    // their editors in order.  Create editors if needed.
    let lastInheritedSource = "";
    let seenPseudoElement = false;
    let seenNormalElement = false;

    if (!this._elementStyle.rules) {
      return;
    }

    for (let rule of this._elementStyle.rules) {
      if (rule.domRule.system) {
        continue;
      }

      // Only print header for this element if there are pseudo elements
      if (seenPseudoElement && !seenNormalElement && !rule.pseudoElement) {
        seenNormalElement = true;
        let div = this.doc.createElementNS(HTML_NS, "div");
        div.className = this._getRuleViewHeaderClassName();
        div.textContent = this.selectedElementLabel;
        this.element.appendChild(div);
      }

      let inheritedSource = rule.inheritedSource;
      if (inheritedSource != lastInheritedSource) {
        let div = this.doc.createElementNS(HTML_NS, "div");
        div.className = this._getRuleViewHeaderClassName();
        div.textContent = inheritedSource;
        lastInheritedSource = inheritedSource;
        this.element.appendChild(div);
      }

      if (!seenPseudoElement && rule.pseudoElement) {
        seenPseudoElement = true;

        let div = this.doc.createElementNS(HTML_NS, "div");
        div.className = this._getRuleViewHeaderClassName(true);
        div.textContent = this.pseudoElementLabel;
        div.addEventListener("dblclick", () => {
          this.togglePseudoElementVisibility(!this.showPseudoElements);
        }, false);

        let twisty = this.pseudoElementTwisty =
          this.doc.createElementNS(HTML_NS, "span");
        twisty.className = "ruleview-expander theme-twisty";
        twisty.addEventListener("click", () => {
          this.togglePseudoElementVisibility(!this.showPseudoElements);
        }, false);

        div.insertBefore(twisty, div.firstChild);
        this.element.appendChild(div);
      }

      if (!rule.editor) {
        rule.editor = new RuleEditor(this, rule);
      }

      this.element.appendChild(rule.editor.element);
    }

    this.togglePseudoElementVisibility(this.showPseudoElements);
  }
};

/**
 * Create a RuleEditor.
 *
 * @param {CssRuleView} aRuleView
 *        The CssRuleView containg the document holding this rule editor.
 * @param {Rule} aRule
 *        The Rule object we're editing.
 * @constructor
 */
function RuleEditor(aRuleView, aRule) {
  this.ruleView = aRuleView;
  this.doc = this.ruleView.doc;
  this.rule = aRule;
  this.isEditable = !aRule.isSystem;
  // Flag that blocks updates of the selector and properties when it is
  // being edited
  this.isEditing = false;

  this._onNewProperty = this._onNewProperty.bind(this);
  this._newPropertyDestroy = this._newPropertyDestroy.bind(this);
  this._onSelectorDone = this._onSelectorDone.bind(this);

  this._create();
}

RuleEditor.prototype = {
  get isSelectorEditable() {
    let toolbox = this.ruleView.inspector.toolbox;
    return toolbox.target.client.traits.selectorEditable;
  },

  _create: function() {
    this.element = this.doc.createElementNS(HTML_NS, "div");
    this.element.className = "ruleview-rule theme-separator";
    this.element.setAttribute("uneditable", !this.isEditable);
    this.element._ruleEditor = this;
    if (this.rule.pseudoElement) {
      this.element.classList.add("ruleview-rule-pseudo-element");
    }

    // Give a relative position for the inplace editor's measurement
    // span to be placed absolutely against.
    this.element.style.position = "relative";

    // Add the source link.
    let source = createChild(this.element, "div", {
      class: "ruleview-rule-source theme-link"
    });
    source.addEventListener("click", function() {
      if (source.hasAttribute("unselectable")) {
        return;
      }
      let rule = this.rule.domRule;
      let evt = this.doc.createEvent("CustomEvent");
      evt.initCustomEvent("CssRuleViewCSSLinkClicked", true, false, {
        rule: rule,
      });
      this.element.dispatchEvent(evt);
    }.bind(this));
    let sourceLabel = this.doc.createElementNS(XUL_NS, "label");
    sourceLabel.setAttribute("crop", "center");
    sourceLabel.classList.add("source-link-label");
    source.appendChild(sourceLabel);

    this.updateSourceLink();

    let code = createChild(this.element, "div", {
      class: "ruleview-code"
    });

    let header = createChild(code, "div", {});

    this.selectorContainer = createChild(header, "span", {
      class: "ruleview-selectorcontainer"
    });

    this.selectorText = createChild(this.selectorContainer, "span", {
      class: "ruleview-selector theme-fg-color3"
    });

    if (this.isEditable && this.rule.domRule.type !== ELEMENT_STYLE &&
        this.isSelectorEditable) {
      this.selectorContainer.addEventListener("click", aEvent => {
        // Clicks within the selector shouldn't propagate any further.
        aEvent.stopPropagation();
      }, false);

      editableField({
        element: this.selectorText,
        done: this._onSelectorDone,
        stopOnShiftTab: true,
        stopOnTab: true,
        stopOnReturn: true
      });
    }

    this.openBrace = createChild(header, "span", {
      class: "ruleview-ruleopen",
      textContent: " {"
    });

    this.propertyList = createChild(code, "ul", {
      class: "ruleview-propertylist"
    });

    this.populate();

    this.closeBrace = createChild(code, "div", {
      class: "ruleview-ruleclose",
      tabindex: this.isEditable ? "0" : "-1",
      textContent: "}"
    });

    this.element.addEventListener("contextmenu", event => {
      try {
        // In the sidebar we do not have this.doc.popupNode so we need to save
        // the node ourselves.
        this.doc.popupNode = event.explicitOriginalTarget;
        let win = this.doc.defaultView;
        win.focus();

        this.ruleView._contextmenu.openPopupAtScreen(
          event.screenX, event.screenY, true);

      } catch(e) {
        console.error(e);
      }
    }, false);

    if (this.isEditable) {
      code.addEventListener("click", () => {
        let selection = this.doc.defaultView.getSelection();
        if (selection.isCollapsed) {
          this.newProperty();
        }
      }, false);

      this.element.addEventListener("mousedown", () => {
        this.doc.defaultView.focus();
      }, false);

      // Create a property editor when the close brace is clicked.
      editableItem({ element: this.closeBrace }, (aElement) => {
        this.newProperty();
      });
    }
  },

  updateSourceLink: function RuleEditor_updateSourceLink()
  {
    let sourceLabel = this.element.querySelector(".source-link-label");
    let sourceHref = (this.rule.sheet && this.rule.sheet.href) ?
      this.rule.sheet.href : this.rule.title;
    sourceLabel.setAttribute("tooltiptext", sourceHref);

    if (this.rule.isSystem) {
      let uaLabel = _strings.GetStringFromName("rule.userAgentStyles");
      sourceLabel.setAttribute("value", uaLabel + " " + this.rule.title);

      // Special case about:PreferenceStyleSheet, as it is generated on the
      // fly and the URI is not registered with the about: handler.
      // https://bugzilla.mozilla.org/show_bug.cgi?id=935803#c37
      if (sourceHref === "about:PreferenceStyleSheet") {
        sourceLabel.parentNode.setAttribute("unselectable", "true");
        sourceLabel.setAttribute("value", uaLabel);
        sourceLabel.removeAttribute("tooltiptext");
      }
    } else {
      sourceLabel.setAttribute("value", this.rule.title);
    }

    let showOrig = Services.prefs.getBoolPref(PREF_ORIG_SOURCES);
    if (showOrig && !this.rule.isSystem && this.rule.domRule.type != ELEMENT_STYLE) {
      this.rule.getOriginalSourceStrings().then((strings) => {
        sourceLabel.setAttribute("value", strings.short);
        sourceLabel.setAttribute("tooltiptext", strings.full);
      })
    }
  },

  /**
   * Update the rule editor with the contents of the rule.
   */
  populate: function() {
    // Clear out existing viewers.
    while (this.selectorText.hasChildNodes()) {
      this.selectorText.removeChild(this.selectorText.lastChild);
    }

    // If selector text comes from a css rule, highlight selectors that
    // actually match.  For custom selector text (such as for the 'element'
    // style, just show the text directly.
    if (this.rule.domRule.type === ELEMENT_STYLE) {
      this.selectorText.textContent = this.rule.selectorText;
    } else {
      this.rule.domRule.selectors.forEach((selector, i) => {
        if (i != 0) {
          createChild(this.selectorText, "span", {
            class: "ruleview-selector-separator",
            textContent: ", "
          });
        }
        let cls;
        if (this.rule.matchedSelectors.indexOf(selector) > -1) {
          cls = "ruleview-selector-matched";
        } else {
          cls = "ruleview-selector-unmatched";
        }
        createChild(this.selectorText, "span", {
          class: cls,
          textContent: selector
        });
      });
    }

    for (let prop of this.rule.textProps) {
      if (!prop.editor) {
        let editor = new TextPropertyEditor(this, prop);
        this.propertyList.appendChild(editor.element);
      }
    }
  },

  /**
   * Programatically add a new property to the rule.
   *
   * @param {string} aName
   *        Property name.
   * @param {string} aValue
   *        Property value.
   * @param {string} aPriority
   *        Property priority.
   * @param {TextProperty} aSiblingProp
   *        Optional, property next to which the new property will be added.
   * @return {TextProperty}
   *        The new property
   */
  addProperty: function(aName, aValue, aPriority, aSiblingProp) {
    let prop = this.rule.createProperty(aName, aValue, aPriority, aSiblingProp);
    let index = this.rule.textProps.indexOf(prop);
    let editor = new TextPropertyEditor(this, prop);

    // Insert this node before the DOM node that is currently at its new index
    // in the property list.  There is currently one less node in the DOM than
    // in the property list, so this causes it to appear after aSiblingProp.
    // If there is no node at its index, as is the case where this is the last
    // node being inserted, then this behaves as appendChild.
    this.propertyList.insertBefore(editor.element,
      this.propertyList.children[index]);

    return prop;
  },

  /**
   * Programatically add a list of new properties to the rule.  Focus the UI
   * to the proper location after adding (either focus the value on the
   * last property if it is empty, or create a new property and focus it).
   *
   * @param {Array} aProperties
   *        Array of properties, which are objects with this signature:
   *        {
   *          name: {string},
   *          value: {string},
   *          priority: {string}
   *        }
   * @param {TextProperty} aSiblingProp
   *        Optional, the property next to which all new props should be added.
   */
  addProperties: function(aProperties, aSiblingProp) {
    if (!aProperties || !aProperties.length) {
      return;
    }

    let lastProp = aSiblingProp;
    for (let p of aProperties) {
      lastProp = this.addProperty(p.name, p.value, p.priority, lastProp);
    }

    // Either focus on the last value if incomplete, or start a new one.
    if (lastProp && lastProp.value.trim() === "") {
      lastProp.editor.valueSpan.click();
    } else {
      this.newProperty();
    }
  },

  /**
   * Create a text input for a property name.  If a non-empty property
   * name is given, we'll create a real TextProperty and add it to the
   * rule.
   */
  newProperty: function() {
    // If we're already creating a new property, ignore this.
    if (!this.closeBrace.hasAttribute("tabindex")) {
      return;
    }

    // While we're editing a new property, it doesn't make sense to
    // start a second new property editor, so disable focusing the
    // close brace for now.
    this.closeBrace.removeAttribute("tabindex");

    this.newPropItem = createChild(this.propertyList, "li", {
      class: "ruleview-property ruleview-newproperty",
    });

    this.newPropSpan = createChild(this.newPropItem, "span", {
      class: "ruleview-propertyname",
      tabindex: "0"
    });

    this.multipleAddedProperties = null;

    this.editor = new InplaceEditor({
      element: this.newPropSpan,
      done: this._onNewProperty,
      destroy: this._newPropertyDestroy,
      advanceChars: ":",
      contentType: InplaceEditor.CONTENT_TYPES.CSS_PROPERTY,
      popup: this.ruleView.popup
    });

    // Auto-close the input if multiple rules get pasted into new property.
    this.editor.input.addEventListener("paste",
      blurOnMultipleProperties, false);
  },

  /**
   * Called when the new property input has been dismissed.
   *
   * @param {string} aValue
   *        The value in the editor.
   * @param {bool} aCommit
   *        True if the value should be committed.
   */
  _onNewProperty: function(aValue, aCommit) {
    if (!aValue || !aCommit) {
      return;
    }

    // parseDeclarations allows for name-less declarations, but in the present
    // case, we're creating a new declaration, it doesn't make sense to accept
    // these entries
    this.multipleAddedProperties = parseDeclarations(aValue).filter(d => d.name);

    // Blur the editor field now and deal with adding declarations later when
    // the field gets destroyed (see _newPropertyDestroy)
    this.editor.input.blur();
  },

  /**
   * Called when the new property editor is destroyed.
   * This is where the properties (type TextProperty) are actually being
   * added, since we want to wait until after the inplace editor `destroy`
   * event has been fired to keep consistent UI state.
   */
  _newPropertyDestroy: function() {
    // We're done, make the close brace focusable again.
    this.closeBrace.setAttribute("tabindex", "0");

    this.propertyList.removeChild(this.newPropItem);
    delete this.newPropItem;
    delete this.newPropSpan;

    // If properties were added, we want to focus the proper element.
    // If the last new property has no value, focus the value on it.
    // Otherwise, start a new property and focus that field.
    if (this.multipleAddedProperties && this.multipleAddedProperties.length) {
      this.addProperties(this.multipleAddedProperties);
    }
  },

  /**
   * Called when the selector's inplace editor is closed.
   * Ignores the change if the user pressed escape, otherwise
   * commits it.
   *
   * @param {string} aValue
   *        The value contained in the editor.
   * @param {boolean} aCommit
   *        True if the change should be applied.
   */
  _onSelectorDone: function(aValue, aCommit) {
    if (!aCommit || this.isEditing || aValue === "" ||
        aValue === this.rule.selectorText) {
      return;
    }

    this.isEditing = true;

    this.rule.domRule.modifySelector(aValue).then(isModified => {
      this.isEditing = false;

      if (isModified) {
        this.ruleView.refreshPanel();
      }
    }).then(null, err => {
      this.isEditing = false;
      promiseWarn(err);
    });
  }
};

/**
 * Create a TextPropertyEditor.
 *
 * @param {RuleEditor} aRuleEditor
 *        The rule editor that owns this TextPropertyEditor.
 * @param {TextProperty} aProperty
 *        The text property to edit.
 * @constructor
 */
function TextPropertyEditor(aRuleEditor, aProperty) {
  this.ruleEditor = aRuleEditor;
  this.doc = this.ruleEditor.doc;
  this.popup = this.ruleEditor.ruleView.popup;
  this.prop = aProperty;
  this.prop.editor = this;
  this.browserWindow = this.doc.defaultView.top;
  this.removeOnRevert = this.prop.value === "";

  this._onEnableClicked = this._onEnableClicked.bind(this);
  this._onExpandClicked = this._onExpandClicked.bind(this);
  this._onStartEditing = this._onStartEditing.bind(this);
  this._onNameDone = this._onNameDone.bind(this);
  this._onValueDone = this._onValueDone.bind(this);
  this._onValidate = throttle(this._previewValue, 10, this);
  this.update = this.update.bind(this);

  this._create();
  this.update();
}

TextPropertyEditor.prototype = {
  /**
   * Boolean indicating if the name or value is being currently edited.
   */
  get editing() {
    return !!(this.nameSpan.inplaceEditor || this.valueSpan.inplaceEditor ||
      this.ruleEditor.ruleView.tooltips.colorPicker.tooltip.isShown() ||
      this.ruleEditor.ruleView.tooltips.colorPicker.eyedropperOpen) ||
      this.popup.isOpen;
  },

  /**
   * Create the property editor's DOM.
   */
  _create: function() {
    this.element = this.doc.createElementNS(HTML_NS, "li");
    this.element.classList.add("ruleview-property");

    // The enable checkbox will disable or enable the rule.
    this.enable = createChild(this.element, "div", {
      class: "ruleview-enableproperty theme-checkbox",
      tabindex: "-1"
    });

    // Click to expand the computed properties of the text property.
    this.expander = createChild(this.element, "span", {
      class: "ruleview-expander theme-twisty"
    });
    this.expander.addEventListener("click", this._onExpandClicked, true);

    this.nameContainer = createChild(this.element, "span", {
      class: "ruleview-namecontainer"
    });

    // Property name, editable when focused.  Property name
    // is committed when the editor is unfocused.
    this.nameSpan = createChild(this.nameContainer, "span", {
      class: "ruleview-propertyname theme-fg-color5",
      tabindex: this.ruleEditor.isEditable ? "0" : "-1",
    });

    appendText(this.nameContainer, ": ");

    // Create a span that will hold the property and semicolon.
    // Use this span to create a slightly larger click target
    // for the value.
    let propertyContainer = createChild(this.element, "span", {
      class: "ruleview-propertycontainer"
    });


    // Property value, editable when focused.  Changes to the
    // property value are applied as they are typed, and reverted
    // if the user presses escape.
    this.valueSpan = createChild(propertyContainer, "span", {
      class: "ruleview-propertyvalue theme-fg-color1",
      tabindex: this.ruleEditor.isEditable ? "0" : "-1",
    });

    // Storing the TextProperty on the elements for easy access
    // (for instance by the tooltip)
    this.valueSpan.textProperty = this.prop;
    this.nameSpan.textProperty = this.prop;

    // Save the initial value as the last committed value,
    // for restoring after pressing escape.
    this.committed = { name: this.prop.name,
                       value: this.prop.value,
                       priority: this.prop.priority };

    appendText(propertyContainer, ";");

    this.warning = createChild(this.element, "div", {
      class: "ruleview-warning",
      hidden: "",
      title: CssLogic.l10n("rule.warning.title"),
    });

    // Holds the viewers for the computed properties.
    // will be populated in |_updateComputed|.
    this.computed = createChild(this.element, "ul", {
      class: "ruleview-computedlist",
    });

    // Only bind event handlers if the rule is editable.
    if (this.ruleEditor.isEditable) {
      this.enable.addEventListener("click", this._onEnableClicked, true);

      this.nameContainer.addEventListener("click", (aEvent) => {
        // Clicks within the name shouldn't propagate any further.
        aEvent.stopPropagation();
        if (aEvent.target === propertyContainer) {
          this.nameSpan.click();
        }
      }, false);

      editableField({
        start: this._onStartEditing,
        element: this.nameSpan,
        done: this._onNameDone,
        destroy: this.update,
        advanceChars: ':',
        contentType: InplaceEditor.CONTENT_TYPES.CSS_PROPERTY,
        popup: this.popup
      });

      // Auto blur name field on multiple CSS rules get pasted in.
      this.nameContainer.addEventListener("paste",
        blurOnMultipleProperties, false);

      propertyContainer.addEventListener("click", (aEvent) => {
        // Clicks within the value shouldn't propagate any further.
        aEvent.stopPropagation();

        if (aEvent.target === propertyContainer) {
          this.valueSpan.click();
        }
      }, false);

      this.valueSpan.addEventListener("click", (event) => {
        let target = event.target;

        if (target.nodeName === "a") {
          event.stopPropagation();
          event.preventDefault();
          this.browserWindow.openUILinkIn(target.href, "tab");
        }
      }, false);

      editableField({
        start: this._onStartEditing,
        element: this.valueSpan,
        done: this._onValueDone,
        destroy: this.update,
        validate: this._onValidate,
        advanceChars: ';',
        contentType: InplaceEditor.CONTENT_TYPES.CSS_VALUE,
        property: this.prop,
        popup: this.popup
      });
    }
  },

  /**
   * Get the path from which to resolve requests for this
   * rule's stylesheet.
   * @return {string} the stylesheet's href.
   */
  get sheetHref() {
    let domRule = this.prop.rule.domRule;
    if (domRule) {
      return domRule.href || domRule.nodeHref;
    }
  },

  /**
   * Get the URI from which to resolve relative requests for
   * this rule's stylesheet.
   * @return {nsIURI} A URI based on the the stylesheet's href.
   */
  get sheetURI() {
    if (this._sheetURI === undefined) {
      if (this.sheetHref) {
        this._sheetURI = IOService.newURI(this.sheetHref, null, null);
      } else {
        this._sheetURI = null;
      }
    }

    return this._sheetURI;
  },

  /**
   * Resolve a URI based on the rule stylesheet
   * @param {string} relativePath the path to resolve
   * @return {string} the resolved path.
   */
  resolveURI: function(relativePath) {
    if (this.sheetURI) {
      relativePath = this.sheetURI.resolve(relativePath);
    }
    return relativePath;
  },

  /**
   * Check the property value to find an external resource (if any).
   * @return {string} the URI in the property value, or null if there is no match.
   */
  getResourceURI: function() {
    let val = this.prop.value;
    let uriMatch = CSS_RESOURCE_RE.exec(val);
    let uri = null;

    if (uriMatch && uriMatch[1]) {
      uri = uriMatch[1];
    }

    return uri;
  },

  /**
   * Populate the span based on changes to the TextProperty.
   */
  update: function() {
    if (this.prop.enabled) {
      this.enable.style.removeProperty("visibility");
      this.enable.setAttribute("checked", "");
    } else {
      this.enable.style.visibility = "visible";
      this.enable.removeAttribute("checked");
    }

    this.warning.hidden = this.editing || this.isValid();

    if ((this.prop.overridden || !this.prop.enabled) && !this.editing) {
      this.element.classList.add("ruleview-overridden");
    } else {
      this.element.classList.remove("ruleview-overridden");
    }

    let name = this.prop.name;
    this.nameSpan.textContent = name;

    // Combine the property's value and priority into one string for
    // the value.
    let val = this.prop.value;
    if (this.prop.priority) {
      val += " !" + this.prop.priority;
    }

    let store = this.prop.rule.elementStyle.store;
    let propDirty = store.userProperties.contains(this.prop.rule.style, name);

    if (propDirty) {
      this.element.setAttribute("dirty", "");
    } else {
      this.element.removeAttribute("dirty");
    }

    let swatchClass = "ruleview-colorswatch";
    let outputParser = this.ruleEditor.ruleView._outputParser;
    let frag = outputParser.parseCssProperty(name, val, {
      colorSwatchClass: swatchClass,
      colorClass: "ruleview-color",
      defaultColorType: !propDirty,
      urlClass: "theme-link",
      baseURI: this.sheetURI
    });
    this.valueSpan.innerHTML = "";
    this.valueSpan.appendChild(frag);

    // Attach the color picker tooltip to the color swatches
    this._swatchSpans = this.valueSpan.querySelectorAll("." + swatchClass);
    if (this.ruleEditor.isEditable) {
      for (let span of this._swatchSpans) {
        // Capture the original declaration value to be able to revert later
        let originalValue = this.valueSpan.textContent;
        // Adding this swatch to the list of swatches our colorpicker knows about
        this.ruleEditor.ruleView.tooltips.colorPicker.addSwatch(span, {
          onPreview: () => this._previewValue(this.valueSpan.textContent),
          onCommit: () => this._applyNewValue(this.valueSpan.textContent),
          onRevert: () => this._applyNewValue(originalValue)
        });
      }
    }

    // Populate the computed styles.
    this._updateComputed();
  },

  _onStartEditing: function() {
    this.element.classList.remove("ruleview-overridden");
    this._previewValue(this.prop.value);
  },

  /**
   * Populate the list of computed styles.
   */
  _updateComputed: function () {
    // Clear out existing viewers.
    while (this.computed.hasChildNodes()) {
      this.computed.removeChild(this.computed.lastChild);
    }

    let showExpander = false;
    for each (let computed in this.prop.computed) {
      // Don't bother to duplicate information already
      // shown in the text property.
      if (computed.name === this.prop.name) {
        continue;
      }

      showExpander = true;

      let li = createChild(this.computed, "li", {
        class: "ruleview-computed"
      });

      if (computed.overridden) {
        li.classList.add("ruleview-overridden");
      }

      createChild(li, "span", {
        class: "ruleview-propertyname theme-fg-color5",
        textContent: computed.name
      });
      appendText(li, ": ");

      let outputParser = this.ruleEditor.ruleView._outputParser;
      let frag = outputParser.parseCssProperty(
        computed.name, computed.value, {
          colorSwatchClass: "ruleview-colorswatch",
          urlClass: "theme-link",
          baseURI: this.sheetURI
        }
      );

      createChild(li, "span", {
        class: "ruleview-propertyvalue theme-fg-color1",
        child: frag
      });

      appendText(li, ";");
    }

    // Show or hide the expander as needed.
    if (showExpander) {
      this.expander.style.visibility = "visible";
    } else {
      this.expander.style.visibility = "hidden";
    }
  },

  /**
   * Handles clicks on the disabled property.
   */
  _onEnableClicked: function(aEvent) {
    let checked = this.enable.hasAttribute("checked");
    if (checked) {
      this.enable.removeAttribute("checked");
    } else {
      this.enable.setAttribute("checked", "");
    }
    this.prop.setEnabled(!checked);
    aEvent.stopPropagation();
  },

  /**
   * Handles clicks on the computed property expander.
   */
  _onExpandClicked: function(aEvent) {
    this.computed.classList.toggle("styleinspector-open");
    if (this.computed.classList.contains("styleinspector-open")) {
      this.expander.setAttribute("open", "true");
    } else {
      this.expander.removeAttribute("open");
    }
    aEvent.stopPropagation();
  },

  /**
   * Called when the property name's inplace editor is closed.
   * Ignores the change if the user pressed escape, otherwise
   * commits it.
   *
   * @param {string} aValue
   *        The value contained in the editor.
   * @param {boolean} aCommit
   *        True if the change should be applied.
   */
  _onNameDone: function(aValue, aCommit) {
    if (aCommit && !this.ruleEditor.isEditing) {
      // Unlike the value editor, if a name is empty the entire property
      // should always be removed.
      if (aValue.trim() === "") {
        this.remove();
      } else {
        // Adding multiple rules inside of name field overwrites the current
        // property with the first, then adds any more onto the property list.
        let properties = parseDeclarations(aValue);

        if (properties.length) {
          this.prop.setName(properties[0].name);
          if (properties.length > 1) {
            this.prop.setValue(properties[0].value, properties[0].priority);
            this.ruleEditor.addProperties(properties.slice(1), this.prop);
          }
        }
      }
    }
  },

  /**
   * Remove property from style and the editors from DOM.
   * Begin editing next available property.
   */
  remove: function() {
    if (this._swatchSpans && this._swatchSpans.length) {
      for (let span of this._swatchSpans) {
        this.ruleEditor.ruleView.tooltips.colorPicker.removeSwatch(span);
      }
    }

    this.element.parentNode.removeChild(this.element);
    this.ruleEditor.rule.editClosestTextProperty(this.prop);
    this.nameSpan.textProperty = null;
    this.valueSpan.textProperty = null;
    this.prop.remove();
  },

  /**
   * Called when a value editor closes.  If the user pressed escape,
   * revert to the value this property had before editing.
   *
   * @param {string} aValue
   *        The value contained in the editor.
   * @param {bool} aCommit
   *        True if the change should be applied.
   */
   _onValueDone: function(aValue, aCommit) {
    if (!aCommit && !this.ruleEditor.isEditing) {
       // A new property should be removed when escape is pressed.
       if (this.removeOnRevert) {
         this.remove();
       } else {
         this.prop.setValue(this.committed.value, this.committed.priority);
       }
       return;
    }

    let {propertiesToAdd,firstValue} = this._getValueAndExtraProperties(aValue);

    // First, set this property value (common case, only modified a property)
    let val = parseSingleValue(firstValue);
    this.prop.setValue(val.value, val.priority);
    this.removeOnRevert = false;
    this.committed.value = this.prop.value;
    this.committed.priority = this.prop.priority;

    // If needed, add any new properties after this.prop.
    this.ruleEditor.addProperties(propertiesToAdd, this.prop);

    // If the name or value is not actively being edited, and the value is
    // empty, then remove the whole property.
    // A timeout is used here to accurately check the state, since the inplace
    // editor `done` and `destroy` events fire before the next editor
    // is focused.
    if (val.value.trim() === "") {
      setTimeout(() => {
        if (!this.editing) {
          this.remove();
        }
      }, 0);
    }
  },

  /**
   * Parse a value string and break it into pieces, starting with the
   * first value, and into an array of additional properties (if any).
   *
   * Example: Calling with "red; width: 100px" would return
   * { firstValue: "red", propertiesToAdd: [{ name: "width", value: "100px" }] }
   *
   * @param {string} aValue
   *        The string to parse
   * @return {object} An object with the following properties:
   *        firstValue: A string containing a simple value, like
   *                    "red" or "100px!important"
   *        propertiesToAdd: An array with additional properties, following the
   *                         parseDeclarations format of {name,value,priority}
   */
  _getValueAndExtraProperties: function(aValue) {
    // The inplace editor will prevent manual typing of multiple properties,
    // but we need to deal with the case during a paste event.
    // Adding multiple properties inside of value editor sets value with the
    // first, then adds any more onto the property list (below this property).
    let firstValue = aValue;
    let propertiesToAdd = [];

    let properties = parseDeclarations(aValue);

    // Check to see if the input string can be parsed as multiple properties
    if (properties.length) {
      // Get the first property value (if any), and any remaining properties (if any)
      if (!properties[0].name && properties[0].value) {
        firstValue = properties[0].value;
        propertiesToAdd = properties.slice(1);
      }
      // In some cases, the value could be a property:value pair itself.
      // Join them as one value string and append potentially following properties
      else if (properties[0].name && properties[0].value) {
        firstValue = properties[0].name + ": " + properties[0].value;
        propertiesToAdd = properties.slice(1);
      }
    }

    return {
      propertiesToAdd: propertiesToAdd,
      firstValue: firstValue
    };
  },

  _applyNewValue: function(aValue) {
    let val = parseSingleValue(aValue);

    this.prop.setValue(val.value, val.priority);
    this.removeOnRevert = false;
    this.committed.value = this.prop.value;
    this.committed.priority = this.prop.priority;
  },

  /**
   * Live preview this property, without committing changes.
   * @param {string} aValue The value to set the current property to.
   */
  _previewValue: function(aValue) {
    // Since function call is throttled, we need to make sure we are still
    // editing, and any selector modifications have been completed
    if (!this.editing || this.ruleEditor.isEditing) {
      return;
    }

    let val = parseSingleValue(aValue);
    this.ruleEditor.rule.previewPropertyValue(this.prop, val.value, val.priority);
  },

  /**
   * Validate this property. Does it make sense for this value to be assigned
   * to this property name? This does not apply the property value
   *
   * @param {string} [aValue]
   *        The property value used for validation.
   *        Defaults to the current value for this.prop
   *
   * @return {bool} true if the property value is valid, false otherwise.
   */
  isValid: function(aValue) {
    let name = this.prop.name;
    let value = typeof aValue == "undefined" ? this.prop.value : aValue;
    let val = parseSingleValue(value);

    let style = this.doc.createElementNS(HTML_NS, "div").style;
    let prefs = Services.prefs;

    // We toggle output of errors whilst the user is typing a property value.
    let prefVal = prefs.getBoolPref("layout.css.report_errors");
    prefs.setBoolPref("layout.css.report_errors", false);

    let validValue = false;
    try {
      style.setProperty(name, val.value, val.priority);
      validValue = style.getPropertyValue(name) !== "" || val.value === "";
    } finally {
      prefs.setBoolPref("layout.css.report_errors", prefVal);
    }
    return validValue;
  }
};

/**
 * Store of CSSStyleDeclarations mapped to properties that have been changed by
 * the user.
 */
function UserProperties() {
  this.map = new Map();
}

UserProperties.prototype = {
  /**
   * Get a named property for a given CSSStyleDeclaration.
   *
   * @param {CSSStyleDeclaration} aStyle
   *        The CSSStyleDeclaration against which the property is mapped.
   * @param {string} aName
   *        The name of the property to get.
   * @param {string} aDefault
   *        The value to return if the property is has been changed outside of
   *        the rule view.
   * @return {string}
   *          The property value if it has previously been set by the user, null
   *          otherwise.
   */
  getProperty: function(aStyle, aName, aDefault) {
    let key = this.getKey(aStyle);
    let entry = this.map.get(key, null);

    if (entry && aName in entry) {
      let item = entry[aName];
      if (item != aDefault) {
        delete entry[aName];
        return aDefault;
      }
      return item;
    }
    return aDefault;
  },

  /**
   * Set a named property for a given CSSStyleDeclaration.
   *
   * @param {CSSStyleDeclaration} aStyle
   *        The CSSStyleDeclaration against which the property is to be mapped.
   * @param {String} aName
   *        The name of the property to set.
   * @param {String} aUserValue
   *        The value of the property to set.
   */
  setProperty: function(aStyle, aName, aUserValue) {
    let key = this.getKey(aStyle);
    let entry = this.map.get(key, null);
    if (entry) {
      entry[aName] = aUserValue;
    } else {
      let props = {};
      props[aName] = aUserValue;
      this.map.set(key, props);
    }
  },

  /**
   * Check whether a named property for a given CSSStyleDeclaration is stored.
   *
   * @param {CSSStyleDeclaration} aStyle
   *        The CSSStyleDeclaration against which the property would be mapped.
   * @param {String} aName
   *        The name of the property to check.
   */
  contains: function(aStyle, aName) {
    let key = this.getKey(aStyle);
    let entry = this.map.get(key, null);
    return !!entry && aName in entry;
  },

  getKey: function(aStyle) {
    return aStyle.href + ":" + aStyle.line;
  }
};

/**
 * Helper functions
 */

/**
 * Create a child element with a set of attributes.
 *
 * @param {Element} aParent
 *        The parent node.
 * @param {string} aTag
 *        The tag name.
 * @param {object} aAttributes
 *        A set of attributes to set on the node.
 */
function createChild(aParent, aTag, aAttributes) {
  let elt = aParent.ownerDocument.createElementNS(HTML_NS, aTag);
  for (let attr in aAttributes) {
    if (aAttributes.hasOwnProperty(attr)) {
      if (attr === "textContent") {
        elt.textContent = aAttributes[attr];
      } else if(attr === "child") {
        elt.appendChild(aAttributes[attr]);
      } else {
        elt.setAttribute(attr, aAttributes[attr]);
      }
    }
  }
  aParent.appendChild(elt);
  return elt;
}

function createMenuItem(aMenu, aAttributes) {
  let item = aMenu.ownerDocument.createElementNS(XUL_NS, "menuitem");

  item.setAttribute("label", _strings.GetStringFromName(aAttributes.label));
  item.setAttribute("accesskey", _strings.GetStringFromName(aAttributes.accesskey));
  item.addEventListener("command", aAttributes.command);

  aMenu.appendChild(item);

  return item;
}

function setTimeout() {
  let window = Services.appShell.hiddenDOMWindow;
  return window.setTimeout.apply(window, arguments);
}

function clearTimeout() {
  let window = Services.appShell.hiddenDOMWindow;
  return window.clearTimeout.apply(window, arguments);
}

function throttle(func, wait, scope) {
  var timer = null;
  return function() {
    if(timer) {
      clearTimeout(timer);
    }
    var args = arguments;
    timer = setTimeout(function() {
      timer = null;
      func.apply(scope, args);
    }, wait);
  };
}

/**
 * Event handler that causes a blur on the target if the input has
 * multiple CSS properties as the value.
 */
function blurOnMultipleProperties(e) {
  setTimeout(() => {
    let props = parseDeclarations(e.target.value);
    if (props.length > 1) {
      e.target.blur();
    }
  }, 0);
}

/**
 * Append a text node to an element.
 */
function appendText(aParent, aText) {
  aParent.appendChild(aParent.ownerDocument.createTextNode(aText));
}

/**
 * Walk up the DOM from a given node until a parent property holder is found.
 * For elements inside the computed property list, the non-computed parent
 * property holder will be returned
 * @param {DOMNode} node The node to start from
 * @return {DOMNode} The parent property holder node, or null if not found
 */
function getParentTextPropertyHolder(node) {
  while (true) {
    if (!node || !node.classList) {
      return null;
    }
    if (node.classList.contains("ruleview-property")) {
      return node;
    }
    node = node.parentNode;
  }
}

/**
 * For any given node, find the TextProperty it is in if any
 * @param {DOMNode} node The node to start from
 * @return {TextProperty}
 */
function getParentTextProperty(node) {
  let parent = getParentTextPropertyHolder(node);
  if (!parent) {
    return null;
  }

  let propValue = parent.querySelector(".ruleview-propertyvalue");
  if (!propValue) {
    return null;
  }

  return propValue.textProperty;
}

/**
 * Walker up the DOM from a given node until a parent property holder is found,
 * and return the textContent for the name and value nodes.
 * Stops at the first property found, so if node is inside the computed property
 * list, the computed property will be returned
 * @param {DOMNode} node The node to start from
 * @return {Object} {name, value}
 */
function getPropertyNameAndValue(node) {
  while (true) {
    if (!node || !node.classList) {
      return null;
    }
    // Check first for ruleview-computed since it's the deepest
    if (node.classList.contains("ruleview-computed") ||
        node.classList.contains("ruleview-property")) {
      return {
        name: node.querySelector(".ruleview-propertyname").textContent,
        value: node.querySelector(".ruleview-propertyvalue").textContent
      };
    }
    node = node.parentNode;
  }
}

XPCOMUtils.defineLazyGetter(this, "clipboardHelper", function() {
  return Cc["@mozilla.org/widget/clipboardhelper;1"].
    getService(Ci.nsIClipboardHelper);
});

XPCOMUtils.defineLazyGetter(this, "_strings", function() {
  return Services.strings.createBundle(
    "chrome://global/locale/devtools/styleinspector.properties");
});

XPCOMUtils.defineLazyGetter(this, "domUtils", function() {
  return Cc["@mozilla.org/inspector/dom-utils;1"].getService(Ci.inIDOMUtils);
});

loader.lazyGetter(this, "AutocompletePopup", () => require("devtools/shared/autocomplete-popup").AutocompletePopup);
