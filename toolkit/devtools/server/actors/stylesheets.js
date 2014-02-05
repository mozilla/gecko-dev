/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

let { components, Cc, Ci, Cu } = require('chrome');

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/NetUtil.jsm");
Cu.import("resource://gre/modules/FileUtils.jsm");
Cu.import("resource://gre/modules/devtools/SourceMap.jsm");

const promise = require("sdk/core/promise");
const events = require("sdk/event/core");
const protocol = require("devtools/server/protocol");
const {Arg, Option, method, RetVal, types} = protocol;
const {LongStringActor, ShortLongString} = require("devtools/server/actors/string");

loader.lazyGetter(this, "CssLogic", () => require("devtools/styleinspector/css-logic").CssLogic);

let TRANSITION_CLASS = "moz-styleeditor-transitioning";
let TRANSITION_DURATION_MS = 500;
let TRANSITION_RULE = "\
:root.moz-styleeditor-transitioning, :root.moz-styleeditor-transitioning * {\
transition-duration: " + TRANSITION_DURATION_MS + "ms !important; \
transition-delay: 0ms !important;\
transition-timing-function: ease-out !important;\
transition-property: all !important;\
}";

let LOAD_ERROR = "error-load";

exports.register = function(handle) {
  handle.addTabActor(StyleSheetsActor, "styleSheetsActor");
  handle.addGlobalActor(StyleSheetsActor, "styleSheetsActor");
};

exports.unregister = function(handle) {
  handle.removeTabActor(StyleSheetsActor);
  handle.removeGlobalActor(StyleSheetsActor);
};

types.addActorType("stylesheet");
types.addActorType("originalsource");

/**
 * Creates a StyleSheetsActor. StyleSheetsActor provides remote access to the
 * stylesheets of a document.
 */
let StyleSheetsActor = protocol.ActorClass({
  typeName: "stylesheets",

  /**
   * The window we work with, taken from the parent actor.
   */
  get window() this.parentActor.window,

  /**
   * The current content document of the window we work with.
   */
  get document() this.window.document,

  form: function()
  {
    return { actor: this.actorID };
  },

  initialize: function (conn, tabActor) {
    protocol.Actor.prototype.initialize.call(this, null);

    this.parentActor = tabActor;

    // keep a map of sheets-to-actors so we don't create two actors for one sheet
    this._sheets = new Map();
  },

  /**
   * Destroy the current StyleSheetsActor instance.
   */
  destroy: function()
  {
    this._sheets.clear();
  },

  /**
   * Protocol method for getting a list of StyleSheetActors representing
   * all the style sheets in this document.
   */
  getStyleSheets: method(function() {
    let deferred = promise.defer();

    let window = this.window;
    var domReady = () => {
      window.removeEventListener("DOMContentLoaded", domReady, true);

      let documents = [this.document];
      let actors = [];
      for (let doc of documents) {
        let sheets = this._addStyleSheets(doc.styleSheets);
        actors = actors.concat(sheets);
        // Recursively handle style sheets of the documents in iframes.
        for (let iframe of doc.getElementsByTagName("iframe")) {
          documents.push(iframe.contentDocument);
        }
      }
      deferred.resolve(actors);
    };

    if (window.document.readyState === "loading") {
      window.addEventListener("DOMContentLoaded", domReady, true);
    } else {
      domReady();
    }

    return deferred.promise;
  }, {
    request: {},
    response: { styleSheets: RetVal("array:stylesheet") }
  }),

  /**
   * Add all the stylesheets to the map and create an actor for each one
   * if not already created. Send event that there are new stylesheets.
   *
   * @param {[DOMStyleSheet]} styleSheets
   *        Stylesheets to add
   * @return {[object]}
   *         Array of actors for each StyleSheetActor created
   */
  _addStyleSheets: function(styleSheets)
  {
    let sheets = [];
    for (let i = 0; i < styleSheets.length; i++) {
      let styleSheet = styleSheets[i];
      sheets.push(styleSheet);

      // Get all sheets, including imported ones
      let imports = this._getImported(styleSheet);
      sheets = sheets.concat(imports);
    }
    let actors = sheets.map(this._createStyleSheetActor.bind(this));

    return actors;
  },

  /**
   * Get all the stylesheets @imported from a stylesheet.
   *
   * @param  {DOMStyleSheet} styleSheet
   *         Style sheet to search
   * @return {array}
   *         All the imported stylesheets
   */
  _getImported: function(styleSheet) {
   let imported = [];

   for (let i = 0; i < styleSheet.cssRules.length; i++) {
      let rule = styleSheet.cssRules[i];
      if (rule.type == Ci.nsIDOMCSSRule.IMPORT_RULE) {
        // Associated styleSheet may be null if it has already been seen due to
        // duplicate @imports for the same URL.
        if (!rule.styleSheet) {
          continue;
        }
        imported.push(rule.styleSheet);

        // recurse imports in this stylesheet as well
        imported = imported.concat(this._getImported(rule.styleSheet));
      }
      else if (rule.type != Ci.nsIDOMCSSRule.CHARSET_RULE) {
        // @import rules must precede all others except @charset
        break;
      }
    }
    return imported;
  },

  /**
   * Create a new actor for a style sheet, if it hasn't already been created.
   *
   * @param  {DOMStyleSheet} styleSheet
   *         The style sheet to create an actor for.
   * @return {StyleSheetActor}
   *         The actor for this style sheet
   */
  _createStyleSheetActor: function(styleSheet)
  {
    if (this._sheets.has(styleSheet)) {
      return this._sheets.get(styleSheet);
    }
    let actor = new StyleSheetActor(styleSheet, this);

    this.manage(actor);
    this._sheets.set(styleSheet, actor);

    return actor;
  },

  /**
   * Clear all the current stylesheet actors in map.
   */
  _clearStyleSheetActors: function() {
    for (let actor in this._sheets) {
      this.unmanage(this._sheets[actor]);
    }
    this._sheets.clear();
  },

  /**
   * Create a new style sheet in the document with the given text.
   * Return an actor for it.
   *
   * @param  {object} request
   *         Debugging protocol request object, with 'text property'
   * @return {object}
   *         Object with 'styelSheet' property for form on new actor.
   */
  addStyleSheet: method(function(text) {
    let parent = this.document.documentElement;
    let style = this.document.createElementNS("http://www.w3.org/1999/xhtml", "style");
    style.setAttribute("type", "text/css");

    if (text) {
      style.appendChild(this.document.createTextNode(text));
    }
    parent.appendChild(style);

    let actor = this._createStyleSheetActor(style.sheet);
    return actor;
  }, {
    request: { text: Arg(0, "string") },
    response: { styleSheet: RetVal("stylesheet") }
  })
});

/**
 * The corresponding Front object for the StyleSheetsActor.
 */
let StyleSheetsFront = protocol.FrontClass(StyleSheetsActor, {
  initialize: function(client, tabForm) {
    protocol.Front.prototype.initialize.call(this, client);
    this.actorID = tabForm.styleSheetsActor;

    client.addActorPool(this);
    this.manage(this);
  }
});

/**
 * A StyleSheetActor represents a stylesheet on the server.
 */
let StyleSheetActor = protocol.ActorClass({
  typeName: "stylesheet",

  events: {
    "property-change" : {
      type: "propertyChange",
      property: Arg(0, "string"),
      value: Arg(1, "json")
    },
    "style-applied" : {
      type: "styleApplied"
    }
  },

  /* List of original sources that generated this stylesheet */
  _originalSources: null,

  toString: function() {
    return "[StyleSheetActor " + this.actorID + "]";
  },

  /**
   * Window of target
   */
  get window() this._window || this.parentActor.window,

  /**
   * Document of target.
   */
  get document() this.window.document,

  /**
   * URL of underlying stylesheet.
   */
  get href() this.rawSheet.href,

  /**
   * Retrieve the index (order) of stylesheet in the document.
   *
   * @return number
   */
  get styleSheetIndex()
  {
    if (this._styleSheetIndex == -1) {
      for (let i = 0; i < this.document.styleSheets.length; i++) {
        if (this.document.styleSheets[i] == this.rawSheet) {
          this._styleSheetIndex = i;
          break;
        }
      }
    }
    return this._styleSheetIndex;
  },

  initialize: function(aStyleSheet, aParentActor, aWindow) {
    protocol.Actor.prototype.initialize.call(this, null);

    this.rawSheet = aStyleSheet;
    this.parentActor = aParentActor;
    this.conn = this.parentActor.conn;

    this._window = aWindow;

    // text and index are unknown until source load
    this.text = null;
    this._styleSheetIndex = -1;

    this._transitionRefCount = 0;

    // if this sheet has an @import, then it's rules are loaded async
    let ownerNode = this.rawSheet.ownerNode;
    if (ownerNode) {
      let onSheetLoaded = function(event) {
        ownerNode.removeEventListener("load", onSheetLoaded, false);
        this._notifyPropertyChanged("ruleCount");
      }.bind(this);

      ownerNode.addEventListener("load", onSheetLoaded, false);
    }
  },

  /**
   * Get the current state of the actor
   *
   * @return {object}
   *         With properties of the underlying stylesheet, plus 'text',
   *        'styleSheetIndex' and 'parentActor' if it's @imported
   */
  form: function(detail) {
    if (detail === "actorid") {
      return this.actorID;
    }

    let docHref;
    if (this.rawSheet.ownerNode) {
      if (this.rawSheet.ownerNode instanceof Ci.nsIDOMHTMLDocument) {
        docHref = this.rawSheet.ownerNode.location.href;
      }
      if (this.rawSheet.ownerNode.ownerDocument) {
        docHref = this.rawSheet.ownerNode.ownerDocument.location.href;
      }
    }

    let form = {
      actor: this.actorID,  // actorID is set when this actor is added to a pool
      href: this.href,
      nodeHref: docHref,
      disabled: this.rawSheet.disabled,
      title: this.rawSheet.title,
      system: !CssLogic.isContentStylesheet(this.rawSheet),
      styleSheetIndex: this.styleSheetIndex
    }

    try {
      form.ruleCount = this.rawSheet.cssRules.length;
    }
    catch(e) {
      // stylesheet had an @import rule that wasn't loaded yet
    }
    return form;
  },

  /**
   * Toggle the disabled property of the style sheet
   *
   * @return {object}
   *         'disabled' - the disabled state after toggling.
   */
  toggleDisabled: method(function() {
    this.rawSheet.disabled = !this.rawSheet.disabled;
    this._notifyPropertyChanged("disabled");

    return this.rawSheet.disabled;
  }, {
    response: { disabled: RetVal("boolean")}
  }),

  /**
   * Send an event notifying that a property of the stylesheet
   * has changed.
   *
   * @param  {string} property
   *         Name of the changed property
   */
  _notifyPropertyChanged: function(property) {
    events.emit(this, "property-change", property, this.form()[property]);
  },

  /**
   * Protocol method to get the text of this stylesheet.
   */
  getText: method(function() {
    return this._getText().then((text) => {
      return new LongStringActor(this.conn, text || "");
    });
  }, {
    response: {
      text: RetVal("longstring")
    }
  }),

  /**
   * Fetch the text for this stylesheet from the cache or network. Return
   * cached text if it's already been fetched.
   *
   * @return {Promise}
   *         Promise that resolves with a string text of the stylesheet.
   */
  _getText: function() {
    if (this.text) {
      return promise.resolve(this.text);
    }

    if (!this.href) {
      // this is an inline <style> sheet
      let content = this.rawSheet.ownerNode.textContent;
      this.text = content;
      return promise.resolve(content);
    }

    let options = {
      window: this.window,
      charset: this._getCSSCharset()
    };

    return fetch(this.href, options).then(({ content }) => {
      this.text = content;
      return content;
    });
  },

  /**
   * Protocol method to get the original source (actors) for this
   * stylesheet if it has uses source maps.
   */
  getOriginalSources: method(function() {
    if (this._originalSources) {
      return promise.resolve(this._originalSources);
    }
    return this._fetchOriginalSources();
  }, {
    request: {},
    response: {
      originalSources: RetVal("nullable:array:originalsource")
    }
  }),

  /**
   * Fetch the original sources (actors) for this style sheet using its
   * source map. If they've already been fetched, returns cached array.
   *
   * @return {Promise}
   *         Promise that resolves with an array of OriginalSourceActors
   */
  _fetchOriginalSources: function() {
    this._clearOriginalSources();
    this._originalSources = [];

    return this.getSourceMap().then((sourceMap) => {
      if (!sourceMap) {
        return null;
      }
      for (let url of sourceMap.sources) {
        let actor = new OriginalSourceActor(url, sourceMap, this);

        this.manage(actor);
        this._originalSources.push(actor);
      }
      return this._originalSources;
    })
  },

  /**
   * Get the SourceMapConsumer for this stylesheet's source map, if
   * it exists. Saves the consumer for later queries.
   *
   * @return {Promise}
   *         A promise that resolves with a SourceMapConsumer, or null.
   */
  getSourceMap: function() {
    if (this._sourceMap) {
      return this._sourceMap;
    }
    return this._fetchSourceMap();
  },

  /**
   * Fetch the source map for this stylesheet.
   *
   * @return {Promise}
   *         A promise that resolves with a SourceMapConsumer, or null.
   */
  _fetchSourceMap: function() {
    let deferred = promise.defer();

    this._getText().then((content) => {
      let url = this._extractSourceMapUrl(content);
      if (!url) {
        // no source map for this stylesheet
        deferred.resolve(null);
        return;
      };

      url = normalize(url, this.href);

      let map = fetch(url, { loadFromCache: false, window: this.window })
        .then(({content}) => {
          let map = new SourceMapConsumer(content);
          this._setSourceMapRoot(map, url, this.href);
          this._sourceMap = promise.resolve(map);

          deferred.resolve(map);
          return map;
        }, deferred.reject);

      this._sourceMap = map;
    }, deferred.reject);

    return deferred.promise;
  },

  /**
   * Clear and unmanage the original source actors for this stylesheet.
   */
  _clearOriginalSources: function() {
    for (actor in this._originalSources) {
      this.unmanage(actor);
    }
    this._originalSources = null;
  },

  /**
   * Sets the source map's sourceRoot to be relative to the source map url.
   */
  _setSourceMapRoot: function(aSourceMap, aAbsSourceMapURL, aScriptURL) {
    const base = dirname(
      aAbsSourceMapURL.startsWith("data:")
        ? aScriptURL
        : aAbsSourceMapURL);
    aSourceMap.sourceRoot = aSourceMap.sourceRoot
      ? normalize(aSourceMap.sourceRoot, base)
      : base;
  },

  /**
   * Get the source map url specified in the text of a stylesheet.
   *
   * @param  {string} content
   *         The text of the style sheet.
   * @return {string}
   *         Url of source map.
   */
  _extractSourceMapUrl: function(content) {
    var matches = /sourceMappingURL\=([^\s\*]*)/.exec(content);
    if (matches) {
      return matches[1];
    }
    return null;
  },

  /**
   * Protocol method that gets the location in the original source of a
   * line, column pair in this stylesheet, if its source mapped, otherwise
   * a promise of the same location.
   */
  getOriginalLocation: method(function(line, column) {
    return this.getSourceMap().then((sourceMap) => {
      if (sourceMap) {
        return sourceMap.originalPositionFor({ line: line, column: column });
      }
      return {
        source: this.href,
        line: line,
        column: column
      }
    });
  }, {
    request: {
      line: Arg(0, "number"),
      column: Arg(1, "number")
    },
    response: RetVal(types.addDictType("originallocationresponse", {
      source: "string",
      line: "number",
      column: "number"
    }))
  }),

  /**
   * Get the charset of the stylesheet according to the character set rules
   * defined in <http://www.w3.org/TR/CSS2/syndata.html#charset>.
   *
   * @param string channelCharset
   *        Charset of the source string if set by the HTTP channel.
   */
  _getCSSCharset: function(channelCharset)
  {
    // StyleSheet's charset can be specified from multiple sources
    if (channelCharset && channelCharset.length > 0) {
      // step 1 of syndata.html: charset given in HTTP header.
      return channelCharset;
    }

    let sheet = this.rawSheet;
    if (sheet) {
      // Do we have a @charset rule in the stylesheet?
      // step 2 of syndata.html (without the BOM check).
      if (sheet.cssRules) {
        let rules = sheet.cssRules;
        if (rules.length
            && rules.item(0).type == Ci.nsIDOMCSSRule.CHARSET_RULE) {
          return rules.item(0).encoding;
        }
      }

      // step 3: charset attribute of <link> or <style> element, if it exists
      if (sheet.ownerNode && sheet.ownerNode.getAttribute) {
        let linkCharset = sheet.ownerNode.getAttribute("charset");
        if (linkCharset != null) {
          return linkCharset;
        }
      }

      // step 4 (1 of 2): charset of referring stylesheet.
      let parentSheet = sheet.parentStyleSheet;
      if (parentSheet && parentSheet.cssRules &&
          parentSheet.cssRules[0].type == Ci.nsIDOMCSSRule.CHARSET_RULE) {
        return parentSheet.cssRules[0].encoding;
      }

      // step 4 (2 of 2): charset of referring document.
      if (sheet.ownerNode && sheet.ownerNode.ownerDocument.characterSet) {
        return sheet.ownerNode.ownerDocument.characterSet;
      }
    }

    // step 5: default to utf-8.
    return "UTF-8";
  },

  /**
   * Update the style sheet in place with new text.
   *
   * @param  {object} request
   *         'text' - new text
   *         'transition' - whether to do CSS transition for change.
   */
  update: method(function(text, transition) {
    DOMUtils.parseStyleSheet(this.rawSheet, text);

    this.text = text;

    this._notifyPropertyChanged("ruleCount");

    if (transition) {
      this._insertTransistionRule();
    }
    else {
      this._notifyStyleApplied();
    }
  }, {
    request: {
      text: Arg(0, "string"),
      transition: Arg(1, "boolean")
    }
  }),

  /**
   * Insert a catch-all transition rule into the document. Set a timeout
   * to remove the rule after a certain time.
   */
  _insertTransistionRule: function() {
    // Insert the global transition rule
    // Use a ref count to make sure we do not add it multiple times.. and remove
    // it only when all pending StyleSheets-generated transitions ended.
    if (this._transitionRefCount == 0) {
      this.rawSheet.insertRule(TRANSITION_RULE, this.rawSheet.cssRules.length);
      this.document.documentElement.classList.add(TRANSITION_CLASS);
    }

    this._transitionRefCount++;

    // Set up clean up and commit after transition duration (+10% buffer)
    // @see _onTransitionEnd
    this.window.setTimeout(this._onTransitionEnd.bind(this),
                           Math.floor(TRANSITION_DURATION_MS * 1.1));
  },

  /**
   * This cleans up class and rule added for transition effect and then
   * notifies that the style has been applied.
   */
  _onTransitionEnd: function()
  {
    if (--this._transitionRefCount == 0) {
      this.document.documentElement.classList.remove(TRANSITION_CLASS);
      this.rawSheet.deleteRule(this.rawSheet.cssRules.length - 1);
    }

    events.emit(this, "style-applied");
  }
})

/**
 * StyleSheetFront is the client-side counterpart to a StyleSheetActor.
 */
var StyleSheetFront = protocol.FrontClass(StyleSheetActor, {
  initialize: function(conn, form) {
    protocol.Front.prototype.initialize.call(this, conn, form);

    this._onPropertyChange = this._onPropertyChange.bind(this);
    events.on(this, "property-change", this._onPropertyChange);
  },

  destroy: function() {
    events.off(this, "property-change", this._onPropertyChange);

    protocol.Front.prototype.destroy.call(this);
  },

  _onPropertyChange: function(property, value) {
    this._form[property] = value;
  },

  form: function(form, detail) {
    if (detail === "actorid") {
      this.actorID = form;
      return;
    }
    this.actorID = form.actor;
    this._form = form;
  },

  get href() this._form.href,
  get nodeHref() this._form.nodeHref,
  get disabled() !!this._form.disabled,
  get title() this._form.title,
  get isSystem() this._form.system,
  get styleSheetIndex() this._form.styleSheetIndex,
  get ruleCount() this._form.ruleCount
});

/**
 * Actor representing an original source of a style sheet that was specified
 * in a source map.
 */
let OriginalSourceActor = protocol.ActorClass({
  typeName: "originalsource",

  initialize: function(aUrl, aSourceMap, aParentActor) {
    protocol.Actor.prototype.initialize.call(this, null);

    this.url = aUrl;
    this.sourceMap = aSourceMap;
    this.parentActor = aParentActor;
    this.conn = this.parentActor.conn;

    this.text = null;
  },

  form: function() {
    return {
      actor: this.actorID, // actorID is set when it's added to a pool
      url: this.url,
      relatedStyleSheet: this.parentActor.form()
    };
  },

  _getText: function() {
    if (this.text) {
      return promise.resolve(this.text);
    }
    return fetch(this.url, { window: this.window }).then(({content}) => {
      this.text = content;
      return content;
    });
  },

  /**
   * Protocol method to get the text of this source.
   */
  getText: method(function() {
    return this._getText().then((text) => {
      return new LongStringActor(this.conn, text || "");
    });
  }, {
    response: {
      text: RetVal("longstring")
    }
  })
})

/**
 * The client-side counterpart for an OriginalSourceActor.
 */
let OriginalSourceFront = protocol.FrontClass(OriginalSourceActor, {
  initialize: function(client, form) {
    protocol.Front.prototype.initialize.call(this, client, form);

    this.isOriginalSource = true;
  },

  form: function(form, detail) {
    if (detail === "actorid") {
      this.actorID = form;
      return;
    }
    this.actorID = form.actor;
    this._form = form;
  },

  get href() this._form.url,
  get url() this._form.url
});


XPCOMUtils.defineLazyGetter(this, "DOMUtils", function () {
  return Cc["@mozilla.org/inspector/dom-utils;1"].getService(Ci.inIDOMUtils);
});

exports.StyleSheetsActor = StyleSheetsActor;
exports.StyleSheetsFront = StyleSheetsFront;

exports.StyleSheetActor = StyleSheetActor;
exports.StyleSheetFront = StyleSheetFront;


/**
 * Performs a request to load the desired URL and returns a promise.
 *
 * @param aURL String
 *        The URL we will request.
 * @returns Promise
 *        A promise of the document at that URL, as a string.
 */
function fetch(aURL, aOptions={ loadFromCache: true, window: null,
                                charset: null}) {
  let deferred = promise.defer();
  let scheme;
  let url = aURL.split(" -> ").pop();
  let charset;
  let contentType;

  try {
    scheme = Services.io.extractScheme(url);
  } catch (e) {
    // In the xpcshell tests, the script url is the absolute path of the test
    // file, which will make a malformed URI error be thrown. Add the file
    // scheme prefix ourselves.
    url = "file://" + url;
    scheme = Services.io.extractScheme(url);
  }

  switch (scheme) {
    case "file":
    case "chrome":
    case "resource":
      try {
        NetUtil.asyncFetch(url, function onFetch(aStream, aStatus, aRequest) {
          if (!components.isSuccessCode(aStatus)) {
            deferred.reject(new Error("Request failed with status code = "
                                      + aStatus
                                      + " after NetUtil.asyncFetch for url = "
                                      + url));
            return;
          }

          let source = NetUtil.readInputStreamToString(aStream, aStream.available());
          contentType = aRequest.contentType;
          deferred.resolve(source);
          aStream.close();
        });
      } catch (ex) {
        deferred.reject(ex);
      }
      break;

    default:
      let channel;
      try {
        channel = Services.io.newChannel(url, null, null);
      } catch (e if e.name == "NS_ERROR_UNKNOWN_PROTOCOL") {
        // On Windows xpcshell tests, c:/foo/bar can pass as a valid URL, but
        // newChannel won't be able to handle it.
        url = "file:///" + url;
        channel = Services.io.newChannel(url, null, null);
      }
      let chunks = [];
      let streamListener = {
        onStartRequest: function(aRequest, aContext, aStatusCode) {
          if (!components.isSuccessCode(aStatusCode)) {
            deferred.reject(new Error("Request failed with status code = "
                                      + aStatusCode
                                      + " in onStartRequest handler for url = "
                                      + url));
          }
        },
        onDataAvailable: function(aRequest, aContext, aStream, aOffset, aCount) {
          chunks.push(NetUtil.readInputStreamToString(aStream, aCount));
        },
        onStopRequest: function(aRequest, aContext, aStatusCode) {
          if (!components.isSuccessCode(aStatusCode)) {
            deferred.reject(new Error("Request failed with status code = "
                                      + aStatusCode
                                      + " in onStopRequest handler for url = "
                                      + url));
            return;
          }

          charset = channel.contentCharset || charset;
          contentType = channel.contentType;
          deferred.resolve(chunks.join(""));
        }
      };

      if (aOptions.window) {
        // respect private browsing
        channel.loadGroup = aOptions.window.QueryInterface(Ci.nsIInterfaceRequestor)
                              .getInterface(Ci.nsIWebNavigation)
                              .QueryInterface(Ci.nsIDocumentLoader)
                              .loadGroup;
      }
      channel.loadFlags = aOptions.loadFromCache
        ? channel.LOAD_FROM_CACHE
        : channel.LOAD_BYPASS_CACHE;
      channel.asyncOpen(streamListener, null);
      break;
  }

  return deferred.promise.then(source => {
    return {
      content: convertToUnicode(source, charset),
      contentType: contentType
    };
  });
}

/**
 * Convert a given string, encoded in a given character set, to unicode.
 *
 * @param string aString
 *        A string.
 * @param string aCharset
 *        A character set.
 */
function convertToUnicode(aString, aCharset=null) {
  // Decoding primitives.
  let converter = Cc["@mozilla.org/intl/scriptableunicodeconverter"]
    .createInstance(Ci.nsIScriptableUnicodeConverter);
  try {
    converter.charset = aCharset || "UTF-8";
    return converter.ConvertToUnicode(aString);
  } catch(e) {
    return aString;
  }
}

/**
 * Normalize multiple relative paths towards the base paths on the right.
 */
function normalize(...aURLs) {
  let base = Services.io.newURI(aURLs.pop(), null, null);
  let url;
  while ((url = aURLs.pop())) {
    base = Services.io.newURI(url, null, base);
  }
  return base.spec;
}

function dirname(aPath) {
  return Services.io.newURI(
    ".", null, Services.io.newURI(aPath, null, null)).spec;
}
