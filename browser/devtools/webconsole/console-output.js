/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {Cc, Ci, Cu} = require("chrome");

loader.lazyImporter(this, "VariablesView", "resource:///modules/devtools/VariablesView.jsm");
loader.lazyImporter(this, "escapeHTML", "resource:///modules/devtools/VariablesView.jsm");
loader.lazyImporter(this, "gDevTools", "resource:///modules/devtools/gDevTools.jsm");
loader.lazyImporter(this, "Task","resource://gre/modules/Task.jsm");

const Heritage = require("sdk/core/heritage");
const XHTML_NS = "http://www.w3.org/1999/xhtml";
const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
const STRINGS_URI = "chrome://browser/locale/devtools/webconsole.properties";

const WebConsoleUtils = require("devtools/toolkit/webconsole/utils").Utils;
const l10n = new WebConsoleUtils.l10n(STRINGS_URI);

// Constants for compatibility with the Web Console output implementation before
// bug 778766.
// TODO: remove these once bug 778766 is fixed.
const COMPAT = {
  // The various categories of messages.
  CATEGORIES: {
    NETWORK: 0,
    CSS: 1,
    JS: 2,
    WEBDEV: 3,
    INPUT: 4,
    OUTPUT: 5,
    SECURITY: 6,
  },

  // The possible message severities.
  SEVERITIES: {
    ERROR: 0,
    WARNING: 1,
    INFO: 2,
    LOG: 3,
  },

  // The preference keys to use for each category/severity combination, indexed
  // first by category (rows) and then by severity (columns).
  //
  // Most of these rather idiosyncratic names are historical and predate the
  // division of message type into "category" and "severity".
  PREFERENCE_KEYS: [
    // Error        Warning       Info    Log
    [ "network",    "netwarn",    null,   "networkinfo", ],  // Network
    [ "csserror",   "cssparser",  null,   null,          ],  // CSS
    [ "exception",  "jswarn",     null,   "jslog",       ],  // JS
    [ "error",      "warn",       "info", "log",         ],  // Web Developer
    [ null,         null,         null,   null,          ],  // Input
    [ null,         null,         null,   null,          ],  // Output
    [ "secerror",   "secwarn",    null,   null,          ],  // Security
  ],

  // The fragment of a CSS class name that identifies each category.
  CATEGORY_CLASS_FRAGMENTS: [ "network", "cssparser", "exception", "console",
                              "input", "output", "security" ],

  // The fragment of a CSS class name that identifies each severity.
  SEVERITY_CLASS_FRAGMENTS: [ "error", "warn", "info", "log" ],

  // The indent of a console group in pixels.
  GROUP_INDENT: 12,

  // The default indent in pixels, applied even without any groups.
  GROUP_INDENT_DEFAULT: 6,
};

// A map from the console API call levels to the Web Console severities.
const CONSOLE_API_LEVELS_TO_SEVERITIES = {
  error: "error",
  exception: "error",
  assert: "error",
  warn: "warning",
  info: "info",
  log: "log",
  trace: "log",
  debug: "log",
  dir: "log",
  group: "log",
  groupCollapsed: "log",
  groupEnd: "log",
  time: "log",
  timeEnd: "log",
  count: "log"
};

// Array of known message source URLs we need to hide from output.
const IGNORED_SOURCE_URLS = ["debugger eval code", "self-hosted"];

// The maximum length of strings to be displayed by the Web Console.
const MAX_LONG_STRING_LENGTH = 200000;


/**
 * The ConsoleOutput object is used to manage output of messages in the Web
 * Console.
 *
 * @constructor
 * @param object owner
 *        The console output owner. This usually the WebConsoleFrame instance.
 *        Any other object can be used, as long as it has the following
 *        properties and methods:
 *          - window
 *          - document
 *          - outputMessage(category, methodOrNode[, methodArguments])
 *            TODO: this is needed temporarily, until bug 778766 is fixed.
 */
function ConsoleOutput(owner)
{
  this.owner = owner;
  this._onFlushOutputMessage = this._onFlushOutputMessage.bind(this);
}

ConsoleOutput.prototype = {
  /**
   * The output container.
   * @type DOMElement
   */
  get element() {
    return this.owner.outputNode;
  },

  /**
   * The document that holds the output.
   * @type DOMDocument
   */
  get document() {
    return this.owner ? this.owner.document : null;
  },

  /**
   * The DOM window that holds the output.
   * @type Window
   */
  get window() {
    return this.owner.window;
  },

  /**
   * Getter for the debugger WebConsoleClient.
   * @type object
   */
  get webConsoleClient() {
    return this.owner.webConsoleClient;
  },

  /**
   * Getter for the current toolbox debuggee target.
   * @type Target
   */
  get toolboxTarget() {
    return this.owner.owner.target;
  },

  /**
   * Release an actor.
   *
   * @private
   * @param string actorId
   *        The actor ID you want to release.
   */
  _releaseObject: function(actorId)
  {
    this.owner._releaseObject(actorId);
  },

  /**
   * Add a message to output.
   *
   * @param object ...args
   *        Any number of Message objects.
   * @return this
   */
  addMessage: function(...args)
  {
    for (let msg of args) {
      msg.init(this);
      this.owner.outputMessage(msg._categoryCompat, this._onFlushOutputMessage,
                               [msg]);
    }
    return this;
  },

  /**
   * Message renderer used for compatibility with the current Web Console output
   * implementation. This method is invoked for every message object that is
   * flushed to output. The message object is initialized and rendered, then it
   * is displayed.
   *
   * TODO: remove this method once bug 778766 is fixed.
   *
   * @private
   * @param object message
   *        The message object to render.
   * @return DOMElement
   *         The message DOM element that can be added to the console output.
   */
  _onFlushOutputMessage: function(message)
  {
    return message.render().element;
  },

  /**
   * Get an array of selected messages. This list is based on the text selection
   * start and end points.
   *
   * @param number [limit]
   *        Optional limit of selected messages you want. If no value is given,
   *        all of the selected messages are returned.
   * @return array
   *         Array of DOM elements for each message that is currently selected.
   */
  getSelectedMessages: function(limit)
  {
    let selection = this.window.getSelection();
    if (selection.isCollapsed) {
      return [];
    }

    if (selection.containsNode(this.element, true)) {
      return Array.slice(this.element.children);
    }

    let anchor = this.getMessageForElement(selection.anchorNode);
    let focus = this.getMessageForElement(selection.focusNode);
    if (!anchor || !focus) {
      return [];
    }

    let start, end;
    if (anchor.timestamp > focus.timestamp) {
      start = focus;
      end = anchor;
    } else {
      start = anchor;
      end = focus;
    }

    let result = [];
    let current = start;
    while (current) {
      result.push(current);
      if (current == end || (limit && result.length == limit)) {
        break;
      }
      current = current.nextSibling;
    }
    return result;
  },

  /**
   * Find the DOM element of a message for any given descendant.
   *
   * @param DOMElement elem
   *        The element to start the search from.
   * @return DOMElement|null
   *         The DOM element of the message, if any.
   */
  getMessageForElement: function(elem)
  {
    while (elem && elem.parentNode) {
      if (elem.classList && elem.classList.contains("message")) {
        return elem;
      }
      elem = elem.parentNode;
    }
    return null;
  },

  /**
   * Select all messages.
   */
  selectAllMessages: function()
  {
    let selection = this.window.getSelection();
    selection.removeAllRanges();
    let range = this.document.createRange();
    range.selectNodeContents(this.element);
    selection.addRange(range);
  },

  /**
   * Add a message to the selection.
   *
   * @param DOMElement elem
   *        The message element to select.
   */
  selectMessage: function(elem)
  {
    let selection = this.window.getSelection();
    selection.removeAllRanges();
    let range = this.document.createRange();
    range.selectNodeContents(elem);
    selection.addRange(range);
  },

  /**
   * Open an URL in a new tab.
   * @see WebConsole.openLink() in hudservice.js
   */
  openLink: function()
  {
    this.owner.owner.openLink.apply(this.owner.owner, arguments);
  },

  /**
   * Open the variables view to inspect an object actor.
   * @see JSTerm.openVariablesView() in webconsole.js
   */
  openVariablesView: function()
  {
    this.owner.jsterm.openVariablesView.apply(this.owner.jsterm, arguments);
  },

  /**
   * Destroy this ConsoleOutput instance.
   */
  destroy: function()
  {
    this.owner = null;
  },
}; // ConsoleOutput.prototype

/**
 * Message objects container.
 * @type object
 */
let Messages = {};

/**
 * The BaseMessage object is used for all types of messages. Every kind of
 * message should use this object as its base.
 *
 * @constructor
 */
Messages.BaseMessage = function()
{
  this.widgets = new Set();
  this._onClickAnchor = this._onClickAnchor.bind(this);
  this._repeatID = { uid: gSequenceId() };
  this.textContent = "";
};

Messages.BaseMessage.prototype = {
  /**
   * Reference to the ConsoleOutput owner.
   *
   * @type object|null
   *       This is |null| if the message is not yet initialized.
   */
  output: null,

  /**
   * Reference to the parent message object, if this message is in a group or if
   * it is otherwise owned by another message.
   *
   * @type object|null
   */
  parent: null,

  /**
   * Message DOM element.
   *
   * @type DOMElement|null
   *       This is |null| if the message is not yet rendered.
   */
  element: null,

  /**
   * Tells if this message is visible or not.
   * @type boolean
   */
  get visible() {
    return this.element && this.element.parentNode;
  },

  /**
   * The owner DOM document.
   * @type DOMElement
   */
  get document() {
    return this.output.document;
  },

  /**
   * Holds the text-only representation of the message.
   * @type string
   */
  textContent: null,

  /**
   * Set of widgets included in this message.
   * @type Set
   */
  widgets: null,

  // Properties that allow compatibility with the current Web Console output
  // implementation.
  _categoryCompat: null,
  _severityCompat: null,
  _categoryNameCompat: null,
  _severityNameCompat: null,
  _filterKeyCompat: null,

  /**
   * Object that is JSON-ified and used as a non-unique ID for tracking
   * duplicate messages.
   * @private
   * @type object
   */
  _repeatID: null,

  /**
   * Initialize the message.
   *
   * @param object output
   *        The ConsoleOutput owner.
   * @param object [parent=null]
   *        Optional: a different message object that owns this instance.
   * @return this
   */
  init: function(output, parent=null)
  {
    this.output = output;
    this.parent = parent;
    return this;
  },

  /**
   * Non-unique ID for this message object used for tracking duplicate messages.
   * Different message kinds can identify themselves based their own criteria.
   *
   * @return string
   */
  getRepeatID: function()
  {
    return JSON.stringify(this._repeatID);
  },

  /**
   * Render the message. After this method is invoked the |element| property
   * will point to the DOM element of this message.
   * @return this
   */
  render: function()
  {
    if (!this.element) {
      this.element = this._renderCompat();
    }
    return this;
  },

  /**
   * Prepare the message container for the Web Console, such that it is
   * compatible with the current implementation.
   * TODO: remove this once bug 778766 is fixed.
   *
   * @private
   * @return Element
   *         The DOM element that wraps the message.
   */
  _renderCompat: function()
  {
    let doc = this.output.document;
    let container = doc.createElementNS(XHTML_NS, "div");
    container.id = "console-msg-" + gSequenceId();
    container.className = "message";
    container.category = this._categoryCompat;
    container.severity = this._severityCompat;
    container.setAttribute("category", this._categoryNameCompat);
    container.setAttribute("severity", this._severityNameCompat);
    container.setAttribute("filter", this._filterKeyCompat);
    container.clipboardText = this.textContent;
    container.timestamp = this.timestamp;
    container._messageObject = this;

    return container;
  },

  /**
   * Add a click callback to a given DOM element.
   *
   * @private
   * @param Element element
   *        The DOM element to which you want to add a click event handler.
   * @param function [callback=this._onClickAnchor]
   *        Optional click event handler. The default event handler is
   *        |this._onClickAnchor|.
   */
  _addLinkCallback: function(element, callback = this._onClickAnchor)
  {
    // This is going into the WebConsoleFrame object instance that owns
    // the ConsoleOutput object. The WebConsoleFrame owner is the WebConsole
    // object instance from hudservice.js.
    // TODO: move _addMessageLinkCallback() into ConsoleOutput once bug 778766
    // is fixed.
    this.output.owner._addMessageLinkCallback(element, callback);
  },

  /**
   * The default |click| event handler for links in the output. This function
   * opens the anchor's link in a new tab.
   *
   * @private
   * @param Event event
   *        The DOM event that invoked this function.
   */
  _onClickAnchor: function(event)
  {
    this.output.openLink(event.target.href);
  },

  destroy: function()
  {
    // Destroy all widgets that have registered themselves in this.widgets
    for (let widget of this.widgets) {
      widget.destroy();
    }
  }
}; // Messages.BaseMessage.prototype


/**
 * The NavigationMarker is used to show a page load event.
 *
 * @constructor
 * @extends Messages.BaseMessage
 * @param string url
 *        The URL to display.
 * @param number timestamp
 *        The message date and time, milliseconds elapsed since 1 January 1970
 *        00:00:00 UTC.
 */
Messages.NavigationMarker = function(url, timestamp)
{
  Messages.BaseMessage.call(this);
  this._url = url;
  this.textContent = "------ " + url;
  this.timestamp = timestamp;
};

Messages.NavigationMarker.prototype = Heritage.extend(Messages.BaseMessage.prototype,
{
  /**
   * The address of the loading page.
   * @private
   * @type string
   */
  _url: null,

  /**
   * Message timestamp.
   *
   * @type number
   *       Milliseconds elapsed since 1 January 1970 00:00:00 UTC.
   */
  timestamp: 0,

  _categoryCompat: COMPAT.CATEGORIES.NETWORK,
  _severityCompat: COMPAT.SEVERITIES.LOG,
  _categoryNameCompat: "network",
  _severityNameCompat: "info",
  _filterKeyCompat: "networkinfo",

  /**
   * Prepare the DOM element for this message.
   * @return this
   */
  render: function()
  {
    if (this.element) {
      return this;
    }

    let url = this._url;
    let pos = url.indexOf("?");
    if (pos > -1) {
      url = url.substr(0, pos);
    }

    let doc = this.output.document;
    let urlnode = doc.createElementNS(XHTML_NS, "a");
    urlnode.className = "url";
    urlnode.textContent = url;
    urlnode.title = this._url;
    urlnode.href = this._url;
    urlnode.draggable = false;
    this._addLinkCallback(urlnode);

    let render = Messages.BaseMessage.prototype.render.bind(this);
    render().element.appendChild(urlnode);
    this.element.classList.add("navigation-marker");
    this.element.url = this._url;
    this.element.appendChild(doc.createTextNode("\n"));

    return this;
  },
}); // Messages.NavigationMarker.prototype


/**
 * The Simple message is used to show any basic message in the Web Console.
 *
 * @constructor
 * @extends Messages.BaseMessage
 * @param string|Node|function message
 *        The message to display.
 * @param object [options]
 *        Options for this message:
 *        - category: (string) category that this message belongs to. Defaults
 *        to no category.
 *        - severity: (string) severity of the message. Defaults to no severity.
 *        - timestamp: (number) date and time when the message was recorded.
 *        Defaults to |Date.now()|.
 *        - link: (string) if provided, the message will be wrapped in an anchor
 *        pointing to the given URL here.
 *        - linkCallback: (function) if provided, the message will be wrapped in
 *        an anchor. The |linkCallback| function will be added as click event
 *        handler.
 *        - location: object that tells the message source: url, line, column
 *        and lineText.
 *        - className: (string) additional element class names for styling
 *        purposes.
 *        - private: (boolean) mark this as a private message.
 *        - filterDuplicates: (boolean) true if you do want this message to be
 *        filtered as a potential duplicate message, false otherwise.
 */
Messages.Simple = function(message, options = {})
{
  Messages.BaseMessage.call(this);

  this.category = options.category;
  this.severity = options.severity;
  this.location = options.location;
  this.timestamp = options.timestamp || Date.now();
  this.private = !!options.private;

  this._message = message;
  this._className = options.className;
  this._link = options.link;
  this._linkCallback = options.linkCallback;
  this._filterDuplicates = options.filterDuplicates;
};

Messages.Simple.prototype = Heritage.extend(Messages.BaseMessage.prototype,
{
  /**
   * Message category.
   * @type string
   */
  category: null,

  /**
   * Message severity.
   * @type string
   */
  severity: null,

  /**
   * Message source location. Properties: url, line, column, lineText.
   * @type object
   */
  location: null,

  /**
   * Tells if this message comes from a private browsing context.
   * @type boolean
   */
  private: false,

  /**
   * Custom class name for the DOM element of the message.
   * @private
   * @type string
   */
  _className: null,

  /**
   * Message link - if this message is clicked then this URL opens in a new tab.
   * @private
   * @type string
   */
  _link: null,

  /**
   * Message click event handler.
   * @private
   * @type function
   */
  _linkCallback: null,

  /**
   * Tells if this message should be checked if it is a duplicate of another
   * message or not.
   */
  _filterDuplicates: false,

  /**
   * The raw message displayed by this Message object. This can be a function,
   * DOM node or a string.
   *
   * @private
   * @type mixed
   */
  _message: null,

  _afterMessage: null,
  _objectActors: null,
  _groupDepthCompat: 0,

  /**
   * Message timestamp.
   *
   * @type number
   *       Milliseconds elapsed since 1 January 1970 00:00:00 UTC.
   */
  timestamp: 0,

  get _categoryCompat() {
    return this.category ?
           COMPAT.CATEGORIES[this.category.toUpperCase()] : null;
  },
  get _severityCompat() {
    return this.severity ?
           COMPAT.SEVERITIES[this.severity.toUpperCase()] : null;
  },
  get _categoryNameCompat() {
    return this.category ?
           COMPAT.CATEGORY_CLASS_FRAGMENTS[this._categoryCompat] : null;
  },
  get _severityNameCompat() {
    return this.severity ?
           COMPAT.SEVERITY_CLASS_FRAGMENTS[this._severityCompat] : null;
  },

  get _filterKeyCompat() {
    return this._categoryCompat !== null && this._severityCompat !== null ?
           COMPAT.PREFERENCE_KEYS[this._categoryCompat][this._severityCompat] :
           null;
  },

  init: function()
  {
    Messages.BaseMessage.prototype.init.apply(this, arguments);
    this._groupDepthCompat = this.output.owner.groupDepth;
    this._initRepeatID();
    return this;
  },

  _initRepeatID: function()
  {
    if (!this._filterDuplicates) {
      return;
    }

    // Add the properties we care about for identifying duplicate messages.
    let rid = this._repeatID;
    delete rid.uid;

    rid.category = this.category;
    rid.severity = this.severity;
    rid.private = this.private;
    rid.location = this.location;
    rid.link = this._link;
    rid.linkCallback = this._linkCallback + "";
    rid.className = this._className;
    rid.groupDepth = this._groupDepthCompat;
    rid.textContent = "";
  },

  getRepeatID: function()
  {
    // No point in returning a string that includes other properties when there
    // is a unique ID.
    if (this._repeatID.uid) {
      return JSON.stringify({ uid: this._repeatID.uid });
    }

    return JSON.stringify(this._repeatID);
  },

  render: function()
  {
    if (this.element) {
      return this;
    }

    let timestamp = new Widgets.MessageTimestamp(this, this.timestamp).render();

    let icon = this.document.createElementNS(XHTML_NS, "span");
    icon.className = "icon";

    // Apply the current group by indenting appropriately.
    // TODO: remove this once bug 778766 is fixed.
    let iconMarginLeft = this._groupDepthCompat * COMPAT.GROUP_INDENT +
                         COMPAT.GROUP_INDENT_DEFAULT;
    icon.style.marginLeft = iconMarginLeft + "px";

    let body = this._renderBody();
    this._repeatID.textContent += "|" + body.textContent;

    let repeatNode = this._renderRepeatNode();
    let location = this._renderLocation();

    Messages.BaseMessage.prototype.render.call(this);
    if (this._className) {
      this.element.className += " " + this._className;
    }

    this.element.appendChild(timestamp.element);
    this.element.appendChild(icon);
    this.element.appendChild(body);
    if (repeatNode) {
      this.element.appendChild(repeatNode);
    }
    if (location) {
      this.element.appendChild(location);
    }
    this.element.appendChild(this.document.createTextNode("\n"));

    this.element.clipboardText = this.element.textContent;

    if (this.private) {
      this.element.setAttribute("private", true);
    }

    if (this._afterMessage) {
      this.element._outputAfterNode = this._afterMessage.element;
      this._afterMessage = null;
    }

    // TODO: handle object releasing in a more elegant way once all console
    // messages use the new API - bug 778766.
    this.element._objectActors = this._objectActors;
    this._objectActors = null;

    return this;
  },

  /**
   * Render the message body DOM element.
   * @private
   * @return Element
   */
  _renderBody: function()
  {
    let body = this.document.createElementNS(XHTML_NS, "span");
    body.className = "body devtools-monospace";

    let anchor, container = body;
    if (this._link || this._linkCallback) {
      container = anchor = this.document.createElementNS(XHTML_NS, "a");
      anchor.href = this._link || "#";
      anchor.draggable = false;
      this._addLinkCallback(anchor, this._linkCallback);
      body.appendChild(anchor);
    }

    if (typeof this._message == "function") {
      container.appendChild(this._message(this));
    } else if (this._message instanceof Ci.nsIDOMNode) {
      container.appendChild(this._message);
    } else {
      container.textContent = this._message;
    }

    return body;
  },

  /**
   * Render the repeat bubble DOM element part of the message.
   * @private
   * @return Element
   */
  _renderRepeatNode: function()
  {
    if (!this._filterDuplicates) {
      return null;
    }

    let repeatNode = this.document.createElementNS(XHTML_NS, "span");
    repeatNode.setAttribute("value", "1");
    repeatNode.className = "message-repeats";
    repeatNode.textContent = 1;
    repeatNode._uid = this.getRepeatID();
    return repeatNode;
  },

  /**
   * Render the message source location DOM element.
   * @private
   * @return Element
   */
  _renderLocation: function()
  {
    if (!this.location) {
      return null;
    }

    let {url, line} = this.location;
    if (IGNORED_SOURCE_URLS.indexOf(url) != -1) {
      return null;
    }

    // The ConsoleOutput owner is a WebConsoleFrame instance from webconsole.js.
    // TODO: move createLocationNode() into this file when bug 778766 is fixed.
    return this.output.owner.createLocationNode(url, line);
  },
}); // Messages.Simple.prototype


/**
 * The Extended message.
 *
 * @constructor
 * @extends Messages.Simple
 * @param array messagePieces
 *        The message to display given as an array of elements. Each array
 *        element can be a DOM node, function, ObjectActor, LongString or
 *        a string.
 * @param object [options]
 *        Options for rendering this message:
 *        - quoteStrings: boolean that tells if you want strings to be wrapped
 *        in quotes or not.
 */
Messages.Extended = function(messagePieces, options = {})
{
  Messages.Simple.call(this, null, options);

  this._messagePieces = messagePieces;

  if ("quoteStrings" in options) {
    this._quoteStrings = options.quoteStrings;
  }

  this._repeatID.quoteStrings = this._quoteStrings;
  this._repeatID.messagePieces = messagePieces + "";
  this._repeatID.actors = new Set(); // using a set to avoid duplicates
};

Messages.Extended.prototype = Heritage.extend(Messages.Simple.prototype,
{
  /**
   * The message pieces displayed by this message instance.
   * @private
   * @type array
   */
  _messagePieces: null,

  /**
   * Boolean that tells if the strings displayed in this message are wrapped.
   * @private
   * @type boolean
   */
  _quoteStrings: true,

  getRepeatID: function()
  {
    if (this._repeatID.uid) {
      return JSON.stringify({ uid: this._repeatID.uid });
    }

    // Sets are not stringified correctly. Temporarily switching to an array.
    let actors = this._repeatID.actors;
    this._repeatID.actors = [...actors];
    let result = JSON.stringify(this._repeatID);
    this._repeatID.actors = actors;
    return result;
  },

  render: function()
  {
    let result = this.document.createDocumentFragment();

    for (let i = 0; i < this._messagePieces.length; i++) {
      let separator = i > 0 ? this._renderBodyPieceSeparator() : null;
      if (separator) {
        result.appendChild(separator);
      }

      let piece = this._messagePieces[i];
      result.appendChild(this._renderBodyPiece(piece));
    }

    this._message = result;
    this._messagePieces = null;
    return Messages.Simple.prototype.render.call(this);
  },

  /**
   * Render the separator between the pieces of the message.
   *
   * @private
   * @return Element
   */
  _renderBodyPieceSeparator: function() { return null; },

  /**
   * Render one piece/element of the message array.
   *
   * @private
   * @param mixed piece
   *        Message element to display - this can be a LongString, ObjectActor,
   *        DOM node or a function to invoke.
   * @return Element
   */
  _renderBodyPiece: function(piece)
  {
    if (piece instanceof Ci.nsIDOMNode) {
      return piece;
    }
    if (typeof piece == "function") {
      return piece(this);
    }

    return this._renderValueGrip(piece);
  },

  /**
   * Render a grip that represents a value received from the server. This method
   * picks the appropriate widget to render the value with.
   *
   * @private
   * @param object grip
   *        The value grip received from the server.
   * @param object options
   *        Options for displaying the value. Available options:
   *        - noStringQuotes - boolean that tells the renderer to not use quotes
   *        around strings.
   *        - concise - boolean that tells the renderer to compactly display the
   *        grip. This is typically set to true when the object needs to be
   *        displayed in an array preview, or as a property value in object
   *        previews, etc.
   * @return DOMElement
   *         The DOM element that displays the given grip.
   */
  _renderValueGrip: function(grip, options = {})
  {
    let isPrimitive = VariablesView.isPrimitive({ value: grip });
    let isActorGrip = WebConsoleUtils.isActorGrip(grip);
    let noStringQuotes = !this._quoteStrings;
    if ("noStringQuotes" in options) {
      noStringQuotes = options.noStringQuotes;
    }

    if (isActorGrip) {
      this._repeatID.actors.add(grip.actor);

      if (!isPrimitive) {
        return this._renderObjectActor(grip, options);
      }
      if (grip.type == "longString") {
        let widget = new Widgets.LongString(this, grip, options).render();
        return widget.element;
      }
    }

    let result = this.document.createElementNS(XHTML_NS, "span");
    if (isPrimitive) {
      let className = this.getClassNameForValueGrip(grip);
      if (className) {
        result.className = className;
      }

      result.textContent = VariablesView.getString(grip, {
        noStringQuotes: noStringQuotes,
        concise: options.concise,
      });
    } else {
      result.textContent = grip;
    }

    return result;
  },

  /**
   * Get a CodeMirror-compatible class name for a given value grip.
   *
   * @param object grip
   *        Value grip from the server.
   * @return string
   *         The class name for the grip.
   */
  getClassNameForValueGrip: function(grip)
  {
    let map = {
      "number": "cm-number",
      "longstring": "cm-string",
      "string": "cm-string",
      "regexp": "cm-string-2",
      "boolean": "cm-atom",
      "-infinity": "cm-atom",
      "infinity": "cm-atom",
      "null": "cm-atom",
      "undefined": "cm-atom",
    };

    let className = map[typeof grip];
    if (!className && grip && grip.type) {
      className = map[grip.type.toLowerCase()];
    }
    if (!className && grip && grip.class) {
      className = map[grip.class.toLowerCase()];
    }

    return className;
  },

  /**
   * Display an object actor with the appropriate renderer.
   *
   * @private
   * @param object objectActor
   *        The ObjectActor to display.
   * @param object options
   *        Options to use for displaying the ObjectActor.
   * @see this._renderValueGrip for the available options.
   * @return DOMElement
   *         The DOM element that displays the object actor.
   */
  _renderObjectActor: function(objectActor, options = {})
  {
    let widget = null;
    let {preview} = objectActor;

    if (preview && preview.kind) {
      widget = Widgets.ObjectRenderers.byKind[preview.kind];
    }

    if (!widget || (widget.canRender && !widget.canRender(objectActor))) {
      widget = Widgets.ObjectRenderers.byClass[objectActor.class];
    }

    if (!widget || (widget.canRender && !widget.canRender(objectActor))) {
      widget = Widgets.JSObject;
    }

    let instance = new widget(this, objectActor, options).render();
    return instance.element;
  },
}); // Messages.Extended.prototype



/**
 * The JavaScriptEvalOutput message.
 *
 * @constructor
 * @extends Messages.Extended
 * @param object evalResponse
 *        The evaluation response packet received from the server.
 * @param string [errorMessage]
 *        Optional error message to display.
 */
Messages.JavaScriptEvalOutput = function(evalResponse, errorMessage)
{
  let severity = "log", msg, quoteStrings = true;

  if (errorMessage) {
    severity = "error";
    msg = errorMessage;
    quoteStrings = false;
  } else {
    msg = evalResponse.result;
  }

  let options = {
    className: "cm-s-mozilla",
    timestamp: evalResponse.timestamp,
    category: "output",
    severity: severity,
    quoteStrings: quoteStrings,
  };
  Messages.Extended.call(this, [msg], options);
};

Messages.JavaScriptEvalOutput.prototype = Messages.Extended.prototype;

/**
 * The ConsoleGeneric message is used for console API calls.
 *
 * @constructor
 * @extends Messages.Extended
 * @param object packet
 *        The Console API call packet received from the server.
 */
Messages.ConsoleGeneric = function(packet)
{
  let options = {
    className: "cm-s-mozilla",
    timestamp: packet.timeStamp,
    category: "webdev",
    severity: CONSOLE_API_LEVELS_TO_SEVERITIES[packet.level],
    private: packet.private,
    filterDuplicates: true,
    location: {
      url: packet.filename,
      line: packet.lineNumber,
    },
  };
  switch (packet.level) {
    case "count": {
      let counter = packet.counter, label = counter.label;
      if (!label) {
        label = l10n.getStr("noCounterLabel");
      }
      Messages.Extended.call(this, [label+ ": " + counter.count], options);
      break;
    }
    default:
      Messages.Extended.call(this, packet.arguments, options);
      break;
  }

  this._repeatID.consoleApiLevel = packet.level;
};

Messages.ConsoleGeneric.prototype = Heritage.extend(Messages.Extended.prototype,
{
  _renderBodyPieceSeparator: function()
  {
    return this.document.createTextNode(" ");
  },
}); // Messages.ConsoleGeneric.prototype

/**
 * The ConsoleTrace message is used for console.trace() calls.
 *
 * @constructor
 * @extends Messages.Simple
 * @param object packet
 *        The Console API call packet received from the server.
 */
Messages.ConsoleTrace = function(packet)
{
  let options = {
    className: "consoleTrace cm-s-mozilla",
    timestamp: packet.timeStamp,
    category: "webdev",
    severity: CONSOLE_API_LEVELS_TO_SEVERITIES[packet.level],
    private: packet.private,
    filterDuplicates: true,
    location: {
      url: packet.filename,
      line: packet.lineNumber,
    },
  };

  this._renderStack = this._renderStack.bind(this);
  Messages.Simple.call(this, this._renderStack, options);

  this._repeatID.consoleApiLevel = packet.level;
  this._stacktrace = this._repeatID.stacktrace = packet.stacktrace;
  this._arguments = packet.arguments;
};

Messages.ConsoleTrace.prototype = Heritage.extend(Messages.Simple.prototype,
{
  /**
   * Holds the stackframes received from the server.
   *
   * @private
   * @type array
   */
  _stacktrace: null,

  /**
   * Holds the arguments the content script passed to the console.trace()
   * method. This array is cleared when the message is initialized, and
   * associated actors are released.
   *
   * @private
   * @type array
   */
  _arguments: null,

  init: function()
  {
    let result = Messages.Simple.prototype.init.apply(this, arguments);

    // We ignore console.trace() arguments. Release object actors.
    if (Array.isArray(this._arguments)) {
      for (let arg of this._arguments) {
        if (WebConsoleUtils.isActorGrip(arg)) {
          this.output._releaseObject(arg.actor);
        }
      }
    }
    this._arguments = null;

    return result;
  },

  /**
   * Render the stack frames.
   *
   * @private
   * @return DOMElement
   */
  _renderStack: function()
  {
    let cmvar = this.document.createElementNS(XHTML_NS, "span");
    cmvar.className = "cm-variable";
    cmvar.textContent = "console";

    let cmprop = this.document.createElementNS(XHTML_NS, "span");
    cmprop.className = "cm-property";
    cmprop.textContent = "trace";

    let title = this.document.createElementNS(XHTML_NS, "span");
    title.className = "title devtools-monospace";
    title.appendChild(cmvar);
    title.appendChild(this.document.createTextNode("."));
    title.appendChild(cmprop);
    title.appendChild(this.document.createTextNode("():"));

    let repeatNode = Messages.Simple.prototype._renderRepeatNode.call(this);
    let location = Messages.Simple.prototype._renderLocation.call(this);
    if (location) {
      location.target = "jsdebugger";
    }

    let widget = new Widgets.Stacktrace(this, this._stacktrace).render();

    let body = this.document.createElementNS(XHTML_NS, "div");
    body.appendChild(title);
    if (repeatNode) {
      body.appendChild(repeatNode);
    }
    if (location) {
      body.appendChild(location);
    }
    body.appendChild(this.document.createTextNode("\n"));

    let frag = this.document.createDocumentFragment();
    frag.appendChild(body);
    frag.appendChild(widget.element);

    return frag;
  },

  _renderBody: function()
  {
    let body = Messages.Simple.prototype._renderBody.apply(this, arguments);
    body.classList.remove("devtools-monospace");
    return body;
  },

  // no-op for the message location and .repeats elements.
  // |this._renderStack| handles customized message output.
  _renderLocation: function() { },
  _renderRepeatNode: function() { },
}); // Messages.ConsoleTrace.prototype

let Widgets = {};

/**
 * The base widget class.
 *
 * @constructor
 * @param object message
 *        The owning message.
 */
Widgets.BaseWidget = function(message)
{
  this.message = message;
};

Widgets.BaseWidget.prototype = {
  /**
   * The owning message object.
   * @type object
   */
  message: null,

  /**
   * The DOM element of the rendered widget.
   * @type Element
   */
  element: null,

  /**
   * Getter for the DOM document that holds the output.
   * @type Document
   */
  get document() {
    return this.message.document;
  },

  /**
   * The ConsoleOutput instance that owns this widget instance.
   */
  get output() {
    return this.message.output;
  },

  /**
   * Render the widget DOM element.
   * @return this
   */
  render: function() { },

  /**
   * Destroy this widget instance.
   */
  destroy: function() { },

  /**
   * Helper for creating DOM elements for widgets.
   *
   * Usage:
   *   this.el("tag#id.class.names"); // create element "tag" with ID "id" and
   *   two class names, .class and .names.
   *
   *   this.el("span", { attr1: "value1", ... }) // second argument can be an
   *   object that holds element attributes and values for the new DOM element.
   *
   *   this.el("p", { attr1: "value1", ... }, "text content"); // the third
   *   argument can include the default .textContent of the new DOM element.
   *
   *   this.el("p", "text content"); // if the second argument is not an object,
   *   it will be used as .textContent for the new DOM element.
   *
   * @param string tagNameIdAndClasses
   *        Tag name for the new element, optionally followed by an ID and/or
   *        class names. Examples: "span", "div#fooId", "div.class.names",
   *        "p#id.class".
   * @param string|object [attributesOrTextContent]
   *        If this argument is an object it will be used to set the attributes
   *        of the new DOM element. Otherwise, the value becomes the
   *        .textContent of the new DOM element.
   * @param string [textContent]
   *        If this argument is provided the value is used as the textContent of
   *        the new DOM element.
   * @return DOMElement
   *         The new DOM element.
   */
  el: function(tagNameIdAndClasses)
  {
    let attrs, text;
    if (typeof arguments[1] == "object") {
      attrs = arguments[1];
      text = arguments[2];
    } else {
      text = arguments[1];
    }

    let tagName = tagNameIdAndClasses.split(/#|\./)[0];

    let elem = this.document.createElementNS(XHTML_NS, tagName);
    for (let name of Object.keys(attrs || {})) {
      elem.setAttribute(name, attrs[name]);
    }
    if (text !== undefined && text !== null) {
      elem.textContent = text;
    }

    let idAndClasses = tagNameIdAndClasses.match(/([#.][^#.]+)/g);
    for (let idOrClass of (idAndClasses || [])) {
      if (idOrClass.charAt(0) == "#") {
        elem.id = idOrClass.substr(1);
      } else {
        elem.classList.add(idOrClass.substr(1));
      }
    }

    return elem;
  },
};

/**
 * The timestamp widget.
 *
 * @constructor
 * @param object message
 *        The owning message.
 * @param number timestamp
 *        The UNIX timestamp to display.
 */
Widgets.MessageTimestamp = function(message, timestamp)
{
  Widgets.BaseWidget.call(this, message);
  this.timestamp = timestamp;
};

Widgets.MessageTimestamp.prototype = Heritage.extend(Widgets.BaseWidget.prototype,
{
  /**
   * The UNIX timestamp.
   * @type number
   */
  timestamp: 0,

  render: function()
  {
    if (this.element) {
      return this;
    }

    this.element = this.document.createElementNS(XHTML_NS, "span");
    this.element.className = "timestamp devtools-monospace";
    this.element.textContent = l10n.timestampString(this.timestamp) + " ";

    return this;
  },
}); // Widgets.MessageTimestamp.prototype


/**
 * Widget used for displaying ObjectActors that have no specialised renderers.
 *
 * @constructor
 * @param object message
 *        The owning message.
 * @param object objectActor
 *        The ObjectActor to display.
 * @param object [options]
 *        Options for displaying the given ObjectActor. See
 *        Messages.Extended.prototype._renderValueGrip for the available
 *        options.
 */
Widgets.JSObject = function(message, objectActor, options = {})
{
  Widgets.BaseWidget.call(this, message);
  this.objectActor = objectActor;
  this.options = options;
  this._onClick = this._onClick.bind(this);
};

Widgets.JSObject.prototype = Heritage.extend(Widgets.BaseWidget.prototype,
{
  /**
   * The ObjectActor displayed by the widget.
   * @type object
   */
  objectActor: null,

  render: function()
  {
    if (!this.element) {
      this._render();
    }

    return this;
  },

  _render: function()
  {
    let str = VariablesView.getString(this.objectActor, this.options);
    let className = this.message.getClassNameForValueGrip(this.objectActor);
    if (!className && this.objectActor.class == "Object") {
      className = "cm-variable";
    }

    this.element = this._anchor(str, { className: className });
  },

  /**
   * Render an anchor with a given text content and link.
   *
   * @private
   * @param string text
   *        Text to show in the anchor.
   * @param object [options]
   *        Available options:
   *        - onClick (function): "click" event handler.By default a click on
   *        the anchor opens the variables view for the current object actor
   *        (this.objectActor).
   *        - href (string): if given the string is used as a link, and clicks
   *        on the anchor open the link in a new tab.
   *        - appendTo (DOMElement): append the element to the given DOM
   *        element. If not provided, the anchor is appended to |this.element|
   *        if it is available. If |appendTo| is provided and if it is a falsy
   *        value, the anchor is not appended to any element.
   * @return DOMElement
   *         The DOM element of the new anchor.
   */
  _anchor: function(text, options = {})
  {
    if (!options.onClick && !options.href) {
      options.onClick = this._onClick;
    }

    let anchor = this.el("a", {
      class: options.className,
      draggable: false,
      href: options.href || "#",
    }, text);

    this.message._addLinkCallback(anchor, !options.href ? options.onClick : null);

    if (options.appendTo) {
      options.appendTo.appendChild(anchor);
    } else if (!("appendTo" in options) && this.element) {
      this.element.appendChild(anchor);
    }

    return anchor;
  },

  /**
   * The click event handler for objects shown inline.
   * @private
   */
  _onClick: function()
  {
    this.output.openVariablesView({
      label: VariablesView.getString(this.objectActor, { concise: true }),
      objectActor: this.objectActor,
      autofocus: true,
    });
  },

  /**
   * Add a string to the message.
   *
   * @private
   * @param string str
   *        String to add.
   * @param DOMElement [target = this.element]
   *        Optional DOM element to append the string to. The default is
   *        this.element.
   */
  _text: function(str, target = this.element)
  {
    target.appendChild(this.document.createTextNode(str));
  },
}); // Widgets.JSObject.prototype

Widgets.ObjectRenderers = {};
Widgets.ObjectRenderers.byKind = {};
Widgets.ObjectRenderers.byClass = {};

/**
 * Add an object renderer.
 *
 * @param object obj
 *        An object that represents the renderer. Properties:
 *        - byClass (string, optional): this renderer will be used for the given
 *        object class.
 *        - byKind (string, optional): this renderer will be used for the given
 *        object kind.
 *        One of byClass or byKind must be provided.
 *        - extends (object, optional): the renderer object extends the given
 *        object. Default: Widgets.JSObject.
 *        - canRender (function, optional): this method is invoked when
 *        a candidate object needs to be displayed. The method is invoked as
 *        a static method, as such, none of the properties of the renderer
 *        object will be available. You get one argument: the object actor grip
 *        received from the server. If the method returns true, then this
 *        renderer is used for displaying the object, otherwise not.
 *        - initialize (function, optional): the constructor of the renderer
 *        widget. This function is invoked with the following arguments: the
 *        owner message object instance, the object actor grip to display, and
 *        an options object. See Messages.Extended.prototype._renderValueGrip()
 *        for details about the options object.
 *        - render (function, required): the method that displays the given
 *        object actor.
 */
Widgets.ObjectRenderers.add = function(obj)
{
  let extendObj = obj.extends || Widgets.JSObject;

  let constructor = function() {
    if (obj.initialize) {
      obj.initialize.apply(this, arguments);
    } else {
      extendObj.apply(this, arguments);
    }
  };

  let proto = WebConsoleUtils.cloneObject(obj, false, function(key) {
    if (key == "initialize" || key == "canRender" ||
        (key == "render" && extendObj === Widgets.JSObject)) {
      return false;
    }
    return true;
  });

  if (extendObj === Widgets.JSObject) {
    proto._render = obj.render;
  }

  constructor.canRender = obj.canRender;
  constructor.prototype = Heritage.extend(extendObj.prototype, proto);

  if (obj.byClass) {
    Widgets.ObjectRenderers.byClass[obj.byClass] = constructor;
  } else if (obj.byKind) {
    Widgets.ObjectRenderers.byKind[obj.byKind] = constructor;
  } else {
    throw new Error("You are adding an object renderer without any byClass or " +
                    "byKind property.");
  }
};


/**
 * The widget used for displaying Date objects.
 */
Widgets.ObjectRenderers.add({
  byClass: "Date",

  render: function()
  {
    let {preview} = this.objectActor;
    this.element = this.el("span.class-" + this.objectActor.class);

    let anchorText = this.objectActor.class;
    let anchorClass = "cm-variable";
    if ("timestamp" in preview && typeof preview.timestamp != "number") {
      anchorText = new Date(preview.timestamp).toString(); // invalid date
      anchorClass = "";
    }

    this._anchor(anchorText, { className: anchorClass });

    if (!("timestamp" in preview) || typeof preview.timestamp != "number") {
      return;
    }

    this._text(" ");

    let elem = this.el("span.cm-string-2", new Date(preview.timestamp).toISOString());
    this.element.appendChild(elem);
  },
});

/**
 * The widget used for displaying Function objects.
 */
Widgets.ObjectRenderers.add({
  byClass: "Function",

  render: function()
  {
    let grip = this.objectActor;
    this.element = this.el("span.class-" + this.objectActor.class);

    // TODO: Bug 948484 - support arrow functions and ES6 generators
    let name = grip.userDisplayName || grip.displayName || grip.name || "";
    name = VariablesView.getString(name, { noStringQuotes: true });

    let str = this.options.concise ? name || "function " : "function " + name;

    if (this.options.concise) {
      this._anchor(name || "function", {
        className: name ? "cm-variable" : "cm-keyword",
      });
      if (!name) {
        this._text(" ");
      }
    } else if (name) {
      this.element.appendChild(this.el("span.cm-keyword", "function"));
      this._text(" ");
      this._anchor(name, { className: "cm-variable" });
    } else {
      this._anchor("function", { className: "cm-keyword" });
      this._text(" ");
    }

    this._text("(");

    // TODO: Bug 948489 - Support functions with destructured parameters and
    // rest parameters
    let params = grip.parameterNames || [];
    let shown = 0;
    for (let param of params) {
      if (shown > 0) {
        this._text(", ");
      }
      this.element.appendChild(this.el("span.cm-def", param));
      shown++;
    }

    this._text(")");
  },
}); // Widgets.ObjectRenderers.byClass.Function

/**
 * The widget used for displaying ArrayLike objects.
 */
Widgets.ObjectRenderers.add({
  byKind: "ArrayLike",

  render: function()
  {
    let {preview} = this.objectActor;
    let {items} = preview;
    this.element = this.el("span.kind-" + preview.kind);

    this._anchor(this.objectActor.class, { className: "cm-variable" });

    if (!items || this.options.concise) {
      this._text("[");
      this.element.appendChild(this.el("span.cm-number", preview.length));
      this._text("]");
      return this;
    }

    this._text(" [ ");

    let shown = 0;
    for (let item of items) {
      if (shown > 0) {
        this._text(", ");
      }

      if (item !== null) {
        let elem = this.message._renderValueGrip(item, { concise: true });
        this.element.appendChild(elem);
      } else if (shown == (items.length - 1)) {
        this._text(", ");
      }

      shown++;
    }

    if (shown < preview.length) {
      this._text(", ");

      let n = preview.length - shown;
      let str = VariablesView.stringifiers._getNMoreString(n);
      this._anchor(str);
    }

    this._text(" ]");
  },
}); // Widgets.ObjectRenderers.byKind.ArrayLike

/**
 * The widget used for displaying MapLike objects.
 */
Widgets.ObjectRenderers.add({
  byKind: "MapLike",

  render: function()
  {
    let {preview} = this.objectActor;
    let {entries} = preview;

    let container = this.element = this.el("span.kind-" + preview.kind);
    this._anchor(this.objectActor.class, { className: "cm-variable" });

    if (!entries || this.options.concise) {
      if (typeof preview.size == "number") {
        this._text("[");
        container.appendChild(this.el("span.cm-number", preview.size));
        this._text("]");
      }
      return;
    }

    this._text(" { ");

    let shown = 0;
    for (let [key, value] of entries) {
      if (shown > 0) {
        this._text(", ");
      }

      let keyElem = this.message._renderValueGrip(key, {
        concise: true,
        noStringQuotes: true,
      });

      // Strings are property names.
      if (keyElem.classList && keyElem.classList.contains("cm-string")) {
        keyElem.classList.remove("cm-string");
        keyElem.classList.add("cm-property");
      }

      container.appendChild(keyElem);

      this._text(": ");

      let valueElem = this.message._renderValueGrip(value, { concise: true });
      container.appendChild(valueElem);

      shown++;
    }

    if (typeof preview.size == "number" && shown < preview.size) {
      this._text(", ");

      let n = preview.size - shown;
      let str = VariablesView.stringifiers._getNMoreString(n);
      this._anchor(str);
    }

    this._text(" }");
  },
}); // Widgets.ObjectRenderers.byKind.MapLike

/**
 * The widget used for displaying objects with a URL.
 */
Widgets.ObjectRenderers.add({
  byKind: "ObjectWithURL",

  render: function()
  {
    this.element = this._renderElement(this.objectActor,
                                       this.objectActor.preview.url);
  },

  _renderElement: function(objectActor, url)
  {
    let container = this.el("span.kind-" + objectActor.preview.kind);

    this._anchor(objectActor.class, {
      className: "cm-variable",
      appendTo: container,
    });

    if (!VariablesView.isFalsy({ value: url })) {
      this._text(" \u2192 ", container);
      let shortUrl = WebConsoleUtils.abbreviateSourceURL(url, {
        onlyCropQuery: !this.options.concise
      });
      this._anchor(shortUrl, { href: url, appendTo: container });
    }

    return container;
  },
}); // Widgets.ObjectRenderers.byKind.ObjectWithURL

/**
 * The widget used for displaying objects with a string next to them.
 */
Widgets.ObjectRenderers.add({
  byKind: "ObjectWithText",

  render: function()
  {
    let {preview} = this.objectActor;
    this.element = this.el("span.kind-" + preview.kind);

    this._anchor(this.objectActor.class, { className: "cm-variable" });

    if (!this.options.concise) {
      this._text(" ");
      this.element.appendChild(this.el("span.cm-string",
                                       VariablesView.getString(preview.text)));
    }
  },
});

/**
 * The widget used for displaying DOM event previews.
 */
Widgets.ObjectRenderers.add({
  byKind: "DOMEvent",

  render: function()
  {
    let {preview} = this.objectActor;

    let container = this.element = this.el("span.kind-" + preview.kind);

    this._anchor(preview.type || this.objectActor.class,
                 { className: "cm-variable" });

    if (this.options.concise) {
      return;
    }

    if (preview.eventKind == "key" && preview.modifiers &&
        preview.modifiers.length) {
      this._text(" ");

      let mods = 0;
      for (let mod of preview.modifiers) {
        if (mods > 0) {
          this._text("-");
        }
        container.appendChild(this.el("span.cm-keyword", mod));
        mods++;
      }
    }

    this._text(" { ");

    let shown = 0;
    if (preview.target) {
      container.appendChild(this.el("span.cm-property", "target"));
      this._text(": ");
      let target = this.message._renderValueGrip(preview.target, { concise: true });
      container.appendChild(target);
      shown++;
    }

    for (let key of Object.keys(preview.properties || {})) {
      if (shown > 0) {
        this._text(", ");
      }

      container.appendChild(this.el("span.cm-property", key));
      this._text(": ");

      let value = preview.properties[key];
      let valueElem = this.message._renderValueGrip(value, { concise: true });
      container.appendChild(valueElem);

      shown++;
    }

    this._text(" }");
  },
}); // Widgets.ObjectRenderers.byKind.DOMEvent

/**
 * The widget used for displaying DOM node previews.
 */
Widgets.ObjectRenderers.add({
  byKind: "DOMNode",

  canRender: function(objectActor) {
    let {preview} = objectActor;
    if (!preview) {
      return false;
    }

    switch (preview.nodeType) {
      case Ci.nsIDOMNode.DOCUMENT_NODE:
      case Ci.nsIDOMNode.ATTRIBUTE_NODE:
      case Ci.nsIDOMNode.TEXT_NODE:
      case Ci.nsIDOMNode.COMMENT_NODE:
      case Ci.nsIDOMNode.DOCUMENT_FRAGMENT_NODE:
      case Ci.nsIDOMNode.ELEMENT_NODE:
        return true;
      default:
        return false;
    }
  },

  render: function()
  {
    switch (this.objectActor.preview.nodeType) {
      case Ci.nsIDOMNode.DOCUMENT_NODE:
        this._renderDocumentNode();
        break;
      case Ci.nsIDOMNode.ATTRIBUTE_NODE: {
        let {preview} = this.objectActor;
        this.element = this.el("span.attributeNode.kind-" + preview.kind);
        let attr = this._renderAttributeNode(preview.nodeName, preview.value, true);
        this.element.appendChild(attr);
        break;
      }
      case Ci.nsIDOMNode.TEXT_NODE:
        this._renderTextNode();
        break;
      case Ci.nsIDOMNode.COMMENT_NODE:
        this._renderCommentNode();
        break;
      case Ci.nsIDOMNode.DOCUMENT_FRAGMENT_NODE:
        this._renderDocumentFragmentNode();
        break;
      case Ci.nsIDOMNode.ELEMENT_NODE:
        this._renderElementNode();
        break;
      default:
        throw new Error("Unsupported nodeType: " + preview.nodeType);
    }
  },

  _renderDocumentNode: function()
  {
    let fn = Widgets.ObjectRenderers.byKind.ObjectWithURL.prototype._renderElement;
    this.element = fn.call(this, this.objectActor,
                           this.objectActor.preview.location);
    this.element.classList.add("documentNode");
  },

  _renderAttributeNode: function(nodeName, nodeValue, addLink)
  {
    let value = VariablesView.getString(nodeValue, { noStringQuotes: true });

    let fragment = this.document.createDocumentFragment();
    if (addLink) {
      this._anchor(nodeName, { className: "cm-attribute", appendTo: fragment });
    } else {
      fragment.appendChild(this.el("span.cm-attribute", nodeName));
    }

    this._text("=", fragment);
    fragment.appendChild(this.el("span.cm-string", '"' + escapeHTML(value) + '"'));

    return fragment;
  },

  _renderTextNode: function()
  {
    let {preview} = this.objectActor;
    this.element = this.el("span.textNode.kind-" + preview.kind);

    this._anchor(preview.nodeName, { className: "cm-variable" });
    this._text(" ");

    let text = VariablesView.getString(preview.textContent);
    this.element.appendChild(this.el("span.cm-string", text));
  },

  _renderCommentNode: function()
  {
    let {preview} = this.objectActor;
    let comment = "<!-- " + VariablesView.getString(preview.textContent, {
      noStringQuotes: true,
    }) + " -->";

    this.element = this._anchor(comment, {
      className: "kind-" + preview.kind + " commentNode cm-comment",
    });
  },

  _renderDocumentFragmentNode: function()
  {
    let {preview} = this.objectActor;
    let {childNodes} = preview;
    let container = this.element = this.el("span.documentFragmentNode.kind-" +
                                           preview.kind);

    this._anchor(this.objectActor.class, { className: "cm-variable" });

    if (!childNodes || this.options.concise) {
      this._text("[");
      container.appendChild(this.el("span.cm-number", preview.childNodesLength));
      this._text("]");
      return;
    }

    this._text(" [ ");

    let shown = 0;
    for (let item of childNodes) {
      if (shown > 0) {
        this._text(", ");
      }

      let elem = this.message._renderValueGrip(item, { concise: true });
      container.appendChild(elem);
      shown++;
    }

    if (shown < preview.childNodesLength) {
      this._text(", ");

      let n = preview.childNodesLength - shown;
      let str = VariablesView.stringifiers._getNMoreString(n);
      this._anchor(str);
    }

    this._text(" ]");
  },

  _renderElementNode: function()
  {
    let doc = this.document;
    let {attributes, nodeName} = this.objectActor.preview;

    this.element = this.el("span." + "kind-" + this.objectActor.preview.kind + ".elementNode");

    let openTag = this.el("span.cm-tag");
    openTag.textContent = "<";
    this.element.appendChild(openTag);

    let tagName = this._anchor(nodeName, {
      className: "cm-tag",
      appendTo: openTag
    });

    if (this.options.concise) {
      if (attributes.id) {
        tagName.appendChild(this.el("span.cm-attribute", "#" + attributes.id));
      }
      if (attributes.class) {
        tagName.appendChild(this.el("span.cm-attribute", "." + attributes.class.split(" ").join(".")));
      }
    } else {
      for (let name of Object.keys(attributes)) {
        let attr = this._renderAttributeNode(" " + name, attributes[name]);
        this.element.appendChild(attr);
      }
    }

    let closeTag = this.el("span.cm-tag");
    closeTag.textContent = ">";
    this.element.appendChild(closeTag);

    // Register this widget in the owner message so that it gets destroyed when
    // the message is destroyed.
    this.message.widgets.add(this);

    this.linkToInspector();
  },

  /**
   * If the DOMNode being rendered can be highlit in the page, this function
   * will attach mouseover/out event listeners to do so, and the inspector icon
   * to open the node in the inspector.
   * @return a promise (always the same) that resolves when the node has been
   * linked to the inspector, or rejects if it wasn't (either if no toolbox
   * could be found to access the inspector, or if the node isn't present in the
   * inspector, i.e. if the node is in a DocumentFragment or not part of the
   * tree, or not of type Ci.nsIDOMNode.ELEMENT_NODE).
   */
  linkToInspector: function()
  {
    if (this._linkedToInspector) {
      return this._linkedToInspector;
    }

    this._linkedToInspector = Task.spawn(function*() {
      // Checking the node type
      if (this.objectActor.preview.nodeType !== Ci.nsIDOMNode.ELEMENT_NODE) {
        throw null;
      }

      // Checking the presence of a toolbox
      let target = this.message.output.toolboxTarget;
      this.toolbox = gDevTools.getToolbox(target);
      if (!this.toolbox) {
        throw null;
      }

      // Checking that the inspector supports the node
      yield this.toolbox.initInspector();
      this._nodeFront = yield this.toolbox.walker.getNodeActorFromObjectActor(this.objectActor.actor);
      if (!this._nodeFront) {
        throw null;
      }

      // At this stage, the message may have been cleared already
      if (!this.document) {
        throw null;
      }

      this.highlightDomNode = this.highlightDomNode.bind(this);
      this.element.addEventListener("mouseover", this.highlightDomNode, false);
      this.unhighlightDomNode = this.unhighlightDomNode.bind(this);
      this.element.addEventListener("mouseout", this.unhighlightDomNode, false);

      this._openInspectorNode = this._anchor("", {
        className: "open-inspector",
        onClick: this.openNodeInInspector.bind(this)
      });
      this._openInspectorNode.title = l10n.getStr("openNodeInInspector");
    }.bind(this));

    return this._linkedToInspector;
  },

  /**
   * Highlight the DOMNode corresponding to the ObjectActor in the page.
   * @return a promise that resolves when the node has been highlighted, or
   * rejects if the node cannot be highlighted (detached from the DOM)
   */
  highlightDomNode: function()
  {
    return Task.spawn(function*() {
      yield this.linkToInspector();
      let isAttached = yield this.toolbox.walker.isInDOMTree(this._nodeFront);
      if (isAttached) {
        yield this.toolbox.highlighterUtils.highlightNodeFront(this._nodeFront);
      } else {
        throw null;
      }
    }.bind(this));
  },

  /**
   * Unhighlight a previously highlit node
   * @see highlightDomNode
   * @return a promise that resolves when the highlighter has been hidden
   */
  unhighlightDomNode: function()
  {
    return this.linkToInspector().then(() => {
      return this.toolbox.highlighterUtils.unhighlight();
    });
  },

  /**
   * Open the DOMNode corresponding to the ObjectActor in the inspector panel
   * @return a promise that resolves when the inspector has been switched to
   * and the node has been selected, or rejects if the node cannot be selected
   * (detached from the DOM). Note that in any case, the inspector panel will
   * be switched to.
   */
  openNodeInInspector: function()
  {
    return Task.spawn(function*() {
      yield this.linkToInspector();
      yield this.toolbox.selectTool("inspector");

      let isAttached = yield this.toolbox.walker.isInDOMTree(this._nodeFront);
      if (isAttached) {
        let onReady = this.toolbox.inspector.once("inspector-updated");
        yield this.toolbox.selection.setNodeFront(this._nodeFront, "console");
        yield onReady;
      } else {
        throw null;
      }
    }.bind(this));
  },

  destroy: function()
  {
    if (this.toolbox && this._nodeFront) {
      this.element.removeEventListener("mouseover", this.highlightDomNode, false);
      this.element.removeEventListener("mouseout", this.unhighlightDomNode, false);
      this._openInspectorNode.removeEventListener("mousedown", this.openNodeInInspector, true);
      this.toolbox = null;
      this._nodeFront = null;
    }
  },
}); // Widgets.ObjectRenderers.byKind.DOMNode

/**
 * The widget used for displaying generic JS object previews.
 */
Widgets.ObjectRenderers.add({
  byKind: "Object",

  render: function()
  {
    let {preview} = this.objectActor;
    let {ownProperties, safeGetterValues} = preview;

    if ((!ownProperties && !safeGetterValues) || this.options.concise) {
      this.element = this._anchor(this.objectActor.class,
                                  { className: "cm-variable" });
      return;
    }

    let container = this.element = this.el("span.kind-" + preview.kind);
    this._anchor(this.objectActor.class, { className: "cm-variable" });
    this._text(" { ");

    let addProperty = (str) => {
      container.appendChild(this.el("span.cm-property", str));
    };

    let shown = 0;
    for (let key of Object.keys(ownProperties || {})) {
      if (shown > 0) {
        this._text(", ");
      }

      let value = ownProperties[key];

      addProperty(key);
      this._text(": ");

      if (value.get) {
        addProperty("Getter");
      } else if (value.set) {
        addProperty("Setter");
      } else {
        let valueElem = this.message._renderValueGrip(value.value, { concise: true });
        container.appendChild(valueElem);
      }

      shown++;
    }

    let ownPropertiesShown = shown;

    for (let key of Object.keys(safeGetterValues || {})) {
      if (shown > 0) {
        this._text(", ");
      }

      addProperty(key);
      this._text(": ");

      let value = safeGetterValues[key].getterValue;
      let valueElem = this.message._renderValueGrip(value, { concise: true });
      container.appendChild(valueElem);

      shown++;
    }

    if (typeof preview.ownPropertiesLength == "number" &&
        ownPropertiesShown < preview.ownPropertiesLength) {
      this._text(", ");

      let n = preview.ownPropertiesLength - ownPropertiesShown;
      let str = VariablesView.stringifiers._getNMoreString(n);
      this._anchor(str);
    }

    this._text(" }");
  },
}); // Widgets.ObjectRenderers.byKind.Object

/**
 * The long string widget.
 *
 * @constructor
 * @param object message
 *        The owning message.
 * @param object longStringActor
 *        The LongStringActor to display.
 */
Widgets.LongString = function(message, longStringActor)
{
  Widgets.BaseWidget.call(this, message);
  this.longStringActor = longStringActor;
  this._onClick = this._onClick.bind(this);
  this._onSubstring = this._onSubstring.bind(this);
};

Widgets.LongString.prototype = Heritage.extend(Widgets.BaseWidget.prototype,
{
  /**
   * The LongStringActor displayed by the widget.
   * @type object
   */
  longStringActor: null,

  render: function()
  {
    if (this.element) {
      return this;
    }

    let result = this.element = this.document.createElementNS(XHTML_NS, "span");
    result.className = "longString cm-string";
    this._renderString(this.longStringActor.initial);
    result.appendChild(this._renderEllipsis());

    return this;
  },

  /**
   * Render the long string in the widget element.
   * @private
   * @param string str
   *        The string to display.
   */
  _renderString: function(str)
  {
    this.element.textContent = VariablesView.getString(str, {
      noStringQuotes: !this.message._quoteStrings,
      noEllipsis: true,
    });
  },

  /**
   * Render the anchor ellipsis that allows the user to expand the long string.
   *
   * @private
   * @return Element
   */
  _renderEllipsis: function()
  {
    let ellipsis = this.document.createElementNS(XHTML_NS, "a");
    ellipsis.className = "longStringEllipsis";
    ellipsis.textContent = l10n.getStr("longStringEllipsis");
    ellipsis.href = "#";
    ellipsis.draggable = false;
    this.message._addLinkCallback(ellipsis, this._onClick);

    return ellipsis;
  },

  /**
   * The click event handler for the ellipsis shown after the short string. This
   * function expands the element to show the full string.
   * @private
   */
  _onClick: function()
  {
    let longString = this.output.webConsoleClient.longString(this.longStringActor);
    let toIndex = Math.min(longString.length, MAX_LONG_STRING_LENGTH);

    longString.substring(longString.initial.length, toIndex, this._onSubstring);
  },

  /**
   * The longString substring response callback.
   *
   * @private
   * @param object response
   *        Response packet.
   */
  _onSubstring: function(response)
  {
    if (response.error) {
      Cu.reportError("LongString substring failure: " + response.error);
      return;
    }

    this.element.lastChild.remove();
    this.element.classList.remove("longString");

    this._renderString(this.longStringActor.initial + response.substring);

    this.output.owner.emit("messages-updated", new Set([this.message.element]));

    let toIndex = Math.min(this.longStringActor.length, MAX_LONG_STRING_LENGTH);
    if (toIndex != this.longStringActor.length) {
      this._logWarningAboutStringTooLong();
    }
  },

  /**
   * Inform user that the string he tries to view is too long.
   * @private
   */
  _logWarningAboutStringTooLong: function()
  {
    let msg = new Messages.Simple(l10n.getStr("longStringTooLong"), {
      category: "output",
      severity: "warning",
    });
    this.output.addMessage(msg);
  },
}); // Widgets.LongString.prototype


/**
 * The stacktrace widget.
 *
 * @constructor
 * @extends Widgets.BaseWidget
 * @param object message
 *        The owning message.
 * @param array stacktrace
 *        The stacktrace to display, array of frames as supplied by the server,
 *        over the remote protocol.
 */
Widgets.Stacktrace = function(message, stacktrace)
{
  Widgets.BaseWidget.call(this, message);
  this.stacktrace = stacktrace;
};

Widgets.Stacktrace.prototype = Heritage.extend(Widgets.BaseWidget.prototype,
{
  /**
   * The stackframes received from the server.
   * @type array
   */
  stacktrace: null,

  render: function()
  {
    if (this.element) {
      return this;
    }

    let result = this.element = this.document.createElementNS(XHTML_NS, "ul");
    result.className = "stacktrace devtools-monospace";

    for (let frame of this.stacktrace) {
      result.appendChild(this._renderFrame(frame));
    }

    return this;
  },

  /**
   * Render a frame object received from the server.
   *
   * @param object frame
   *        The stack frame to display. This object should have the following
   *        properties: functionName, filename and lineNumber.
   * @return DOMElement
   *         The DOM element to display for the given frame.
   */
  _renderFrame: function(frame)
  {
    let fn = this.document.createElementNS(XHTML_NS, "span");
    fn.className = "function";
    if (frame.functionName) {
      let span = this.document.createElementNS(XHTML_NS, "span");
      span.className = "cm-variable";
      span.textContent = frame.functionName;
      fn.appendChild(span);
      fn.appendChild(this.document.createTextNode("()"));
    } else {
      fn.classList.add("cm-comment");
      fn.textContent = l10n.getStr("stacktrace.anonymousFunction");
    }

    let location = this.output.owner.createLocationNode(frame.filename,
                                                        frame.lineNumber,
                                                        "jsdebugger");

    // .devtools-monospace sets font-size to 80%, however .body already has
    // .devtools-monospace. If we keep it here, the location would be rendered
    // smaller.
    location.classList.remove("devtools-monospace");

    let elem = this.document.createElementNS(XHTML_NS, "li");
    elem.appendChild(fn);
    elem.appendChild(location);
    elem.appendChild(this.document.createTextNode("\n"));

    return elem;
  },

}); // Widgets.Stacktrace.prototype


function gSequenceId()
{
  return gSequenceId.n++;
}
gSequenceId.n = 0;

exports.ConsoleOutput = ConsoleOutput;
exports.Messages = Messages;
exports.Widgets = Widgets;
