/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Here's the server side of the remote inspector.
 *
 * The WalkerActor is the client's view of the debuggee's DOM.  It's gives
 * the client a tree of NodeActor objects.
 *
 * The walker presents the DOM tree mostly unmodified from the source DOM
 * tree, but with a few key differences:
 *
 *  - Empty text nodes are ignored.  This is pretty typical of developer
 *    tools, but maybe we should reconsider that on the server side.
 *  - iframes with documents loaded have the loaded document as the child,
 *    the walker provides one big tree for the whole document tree.
 *
 * There are a few ways to get references to NodeActors:
 *
 *   - When you first get a WalkerActor reference, it comes with a free
 *     reference to the root document's node.
 *   - Given a node, you can ask for children, siblings, and parents.
 *   - You can issue querySelector and querySelectorAll requests to find
 *     other elements.
 *   - Requests that return arbitrary nodes from the tree (like querySelector
 *     and querySelectorAll) will also return any nodes the client hasn't
 *     seen in order to have a complete set of parents.
 *
 * Once you have a NodeFront, you should be able to answer a few questions
 * without further round trips, like the node's name, namespace/tagName,
 * attributes, etc.  Other questions (like a text node's full nodeValue)
 * might require another round trip.
 *
 * The protocol guarantees that the client will always know the parent of
 * any node that is returned by the server.  This means that some requests
 * (like querySelector) will include the extra nodes needed to satisfy this
 * requirement.  The client keeps track of this parent relationship, so the
 * node fronts form a tree that is a subset of the actual DOM tree.
 *
 *
 * We maintain this guarantee to support the ability to release subtrees on
 * the client - when a node is disconnected from the DOM tree we want to be
 * able to free the client objects for all the children nodes.
 *
 * So to be able to answer "all the children of a given node that we have
 * seen on the client side", we guarantee that every time we've seen a node,
 * we connect it up through its parents.
 */

const {Cc, Ci, Cu, Cr} = require("chrome");
const Services = require("Services");
const protocol = require("devtools/server/protocol");
const {Arg, Option, method, RetVal, types} = protocol;
const {LongStringActor, ShortLongString} = require("devtools/server/actors/string");
const {Promise: promise} = Cu.import("resource://gre/modules/Promise.jsm", {});
const object = require("sdk/util/object");
const events = require("sdk/event/core");
const {Unknown} = require("sdk/platform/xpcom");
const {Class} = require("sdk/core/heritage");
const {PageStyleActor} = require("devtools/server/actors/styles");
const {
  HighlighterActor,
  CustomHighlighterActor,
  HIGHLIGHTER_CLASSES
} = require("devtools/server/actors/highlighter");
const {getLayoutChangesObserver, releaseLayoutChangesObserver} =
  require("devtools/server/actors/layout");

const FONT_FAMILY_PREVIEW_TEXT = "The quick brown fox jumps over the lazy dog";
const FONT_FAMILY_PREVIEW_TEXT_SIZE = 20;
const PSEUDO_CLASSES = [":hover", ":active", ":focus"];
const HIDDEN_CLASS = "__fx-devtools-hide-shortcut__";
const XHTML_NS = "http://www.w3.org/1999/xhtml";
const IMAGE_FETCHING_TIMEOUT = 500;
// The possible completions to a ':' with added score to give certain values
// some preference.
const PSEUDO_SELECTORS = [
  [":active", 1],
  [":hover", 1],
  [":focus", 1],
  [":visited", 0],
  [":link", 0],
  [":first-letter", 0],
  [":first-child", 2],
  [":before", 2],
  [":after", 2],
  [":lang(", 0],
  [":not(", 3],
  [":first-of-type", 0],
  [":last-of-type", 0],
  [":only-of-type", 0],
  [":only-child", 2],
  [":nth-child(", 3],
  [":nth-last-child(", 0],
  [":nth-of-type(", 0],
  [":nth-last-of-type(", 0],
  [":last-child", 2],
  [":root", 0],
  [":empty", 0],
  [":target", 0],
  [":enabled", 0],
  [":disabled", 0],
  [":checked", 1],
  ["::selection", 0]
];


let HELPER_SHEET = ".__fx-devtools-hide-shortcut__ { visibility: hidden !important } ";
HELPER_SHEET += ":-moz-devtools-highlighted { outline: 2px dashed #F06!important; outline-offset: -2px!important } ";

Cu.import("resource://gre/modules/devtools/LayoutHelpers.jsm");

loader.lazyImporter(this, "gDevTools", "resource:///modules/devtools/gDevTools.jsm");

loader.lazyGetter(this, "DOMParser", function() {
  return Cc["@mozilla.org/xmlextras/domparser;1"].createInstance(Ci.nsIDOMParser);
});

exports.register = function(handle) {
  handle.addGlobalActor(InspectorActor, "inspectorActor");
  handle.addTabActor(InspectorActor, "inspectorActor");
};

exports.unregister = function(handle) {
  handle.removeGlobalActor(InspectorActor);
  handle.removeTabActor(InspectorActor);
};

// XXX: A poor man's makeInfallible until we move it out of transport.js
// Which should be very soon.
function makeInfallible(handler) {
  return function(...args) {
    try {
      return handler.apply(this, args);
    } catch(ex) {
      console.error(ex);
    }
    return undefined;
  }
}

// A resolve that hits the main loop first.
function delayedResolve(value) {
  let deferred = promise.defer();
  Services.tm.mainThread.dispatch(makeInfallible(function delayedResolveHandler() {
    deferred.resolve(value);
  }), 0);
  return deferred.promise;
}

types.addDictType("imageData", {
  // The image data
  data: "nullable:longstring",
  // The original image dimensions
  size: "json"
});

/**
 * We only send nodeValue up to a certain size by default.  This stuff
 * controls that size.
 */
exports.DEFAULT_VALUE_SUMMARY_LENGTH = 50;
var gValueSummaryLength = exports.DEFAULT_VALUE_SUMMARY_LENGTH;

exports.getValueSummaryLength = function() {
  return gValueSummaryLength;
};

exports.setValueSummaryLength = function(val) {
  gValueSummaryLength = val;
};

/**
 * Server side of the node actor.
 */
var NodeActor = exports.NodeActor = protocol.ActorClass({
  typeName: "domnode",

  initialize: function(walker, node) {
    protocol.Actor.prototype.initialize.call(this, null);
    this.walker = walker;
    this.rawNode = node;

    // Storing the original display of the node, to track changes when reflows
    // occur
    this.wasDisplayed = this.isDisplayed;
  },

  toString: function() {
    return "[NodeActor " + this.actorID + " for " + this.rawNode.toString() + "]";
  },

  /**
   * Instead of storing a connection object, the NodeActor gets its connection
   * from its associated walker.
   */
  get conn() this.walker.conn,

  isDocumentElement: function() {
    return this.rawNode.ownerDocument &&
        this.rawNode.ownerDocument.documentElement === this.rawNode;
  },

  // Returns the JSON representation of this object over the wire.
  form: function(detail) {
    if (detail === "actorid") {
      return this.actorID;
    }

    let parentNode = this.walker.parentNode(this);

    // Estimate the number of children.
    let numChildren = this.rawNode.childNodes.length;
    if (numChildren === 0 &&
        (this.rawNode.contentDocument || this.rawNode.getSVGDocument)) {
      // This might be an iframe with virtual children.
      numChildren = 1;
    }

    let form = {
      actor: this.actorID,
      baseURI: this.rawNode.baseURI,
      parent: parentNode ? parentNode.actorID : undefined,
      nodeType: this.rawNode.nodeType,
      namespaceURI: this.rawNode.namespaceURI,
      nodeName: this.rawNode.nodeName,
      numChildren: numChildren,

      // doctype attributes
      name: this.rawNode.name,
      publicId: this.rawNode.publicId,
      systemId: this.rawNode.systemId,

      attrs: this.writeAttrs(),

      pseudoClassLocks: this.writePseudoClassLocks(),

      isDisplayed: this.isDisplayed,
    };

    if (this.isDocumentElement()) {
      form.isDocumentElement = true;
    }

    if (this.rawNode.nodeValue) {
      // We only include a short version of the value if it's longer than
      // gValueSummaryLength
      if (this.rawNode.nodeValue.length > gValueSummaryLength) {
        form.shortValue = this.rawNode.nodeValue.substring(0, gValueSummaryLength);
        form.incompleteValue = true;
      } else {
        form.shortValue = this.rawNode.nodeValue;
      }
    }

    return form;
  },

  get computedStyle() {
    if (Cu.isDeadWrapper(this.rawNode) ||
        this.rawNode.nodeType !== Ci.nsIDOMNode.ELEMENT_NODE ||
        !this.rawNode.ownerDocument ||
        !this.rawNode.ownerDocument.defaultView) {
      return null;
    }
    return this.rawNode.ownerDocument.defaultView.getComputedStyle(this.rawNode);
  },

  /**
   * Is the node's display computed style value other than "none"
   */
  get isDisplayed() {
    let style = this.computedStyle;
    if (!style) {
      // Consider all non-element nodes as displayed
      return true;
    } else {
      return style.display !== "none";
    }
  },

  writeAttrs: function() {
    if (!this.rawNode.attributes) {
      return undefined;
    }
    return [{namespace: attr.namespace, name: attr.name, value: attr.value }
            for (attr of this.rawNode.attributes)];
  },

  writePseudoClassLocks: function() {
    if (this.rawNode.nodeType !== Ci.nsIDOMNode.ELEMENT_NODE) {
      return undefined;
    }
    let ret = undefined;
    for (let pseudo of PSEUDO_CLASSES) {
      if (DOMUtils.hasPseudoClassLock(this.rawNode, pseudo)) {
        ret = ret || [];
        ret.push(pseudo);
      }
    }
    return ret;
  },

  /**
   * Returns a LongStringActor with the node's value.
   */
  getNodeValue: method(function() {
    return new LongStringActor(this.conn, this.rawNode.nodeValue || "");
  }, {
    request: {},
    response: {
      value: RetVal("longstring")
    }
  }),

  /**
   * Set the node's value to a given string.
   */
  setNodeValue: method(function(value) {
    this.rawNode.nodeValue = value;
  }, {
    request: { value: Arg(0) },
    response: {}
  }),

  /**
   * Get the node's image data if any (for canvas and img nodes).
   * Returns an imageData object with the actual data being a LongStringActor
   * and a size json object.
   * The image data is transmitted as a base64 encoded png data-uri.
   * The method rejects if the node isn't an image or if the image is missing
   *
   * Accepts a maxDim request parameter to resize images that are larger. This
   * is important as the resizing occurs server-side so that image-data being
   * transfered in the longstring back to the client will be that much smaller
   */
  getImageData: method(function(maxDim) {
    // imageToImageData may fail if the node isn't an image
    try {
      let imageData = imageToImageData(this.rawNode, maxDim);
      return promise.resolve({
        data: LongStringActor(this.conn, imageData.data),
        size: imageData.size
      });
    } catch(e) {
      return promise.reject(new Error("Image not available"));
    }
  }, {
    request: {maxDim: Arg(0, "nullable:number")},
    response: RetVal("imageData")
  }),

  /**
   * Modify a node's attributes.  Passed an array of modifications
   * similar in format to "attributes" mutations.
   * {
   *   attributeName: <string>
   *   attributeNamespace: <optional string>
   *   newValue: <optional string> - If null or undefined, the attribute
   *     will be removed.
   * }
   *
   * Returns when the modifications have been made.  Mutations will
   * be queued for any changes made.
   */
  modifyAttributes: method(function(modifications) {
    let rawNode = this.rawNode;
    for (let change of modifications) {
      if (change.newValue == null) {
        if (change.attributeNamespace) {
          rawNode.removeAttributeNS(change.attributeNamespace, change.attributeName);
        } else {
          rawNode.removeAttribute(change.attributeName);
        }
      } else {
        if (change.attributeNamespace) {
          rawNode.setAttributeNS(change.attributeNamespace, change.attributeName, change.newValue);
        } else {
          rawNode.setAttribute(change.attributeName, change.newValue);
        }
      }
    }
  }, {
    request: {
      modifications: Arg(0, "array:json")
    },
    response: {}
  }),

  /**
   * Given the font and fill style, get the image data of a canvas with the
   * preview text and font.
   * Returns an imageData object with the actual data being a LongStringActor
   * and the width of the text as a string.
   * The image data is transmitted as a base64 encoded png data-uri.
   */
  getFontFamilyDataURL: method(function(font, fillStyle="black") {
    let doc = this.rawNode.ownerDocument;
    let canvas = doc.createElementNS(XHTML_NS, "canvas");
    let ctx = canvas.getContext("2d");
    let fontValue = FONT_FAMILY_PREVIEW_TEXT_SIZE + "px " + font + ", serif";

    // Get the correct preview text measurements and set the canvas dimensions
    ctx.font = fontValue;
    let textWidth = ctx.measureText(FONT_FAMILY_PREVIEW_TEXT).width;
    canvas.width = textWidth * 2;
    canvas.height = FONT_FAMILY_PREVIEW_TEXT_SIZE * 3;

    ctx.font = fontValue;
    ctx.fillStyle = fillStyle;

    // Align the text to be vertically center in the tooltip and
    // oversample the canvas for better text quality
    ctx.textBaseline = "top";
    ctx.scale(2, 2);
    ctx.fillText(FONT_FAMILY_PREVIEW_TEXT, 0, Math.round(FONT_FAMILY_PREVIEW_TEXT_SIZE / 3));

    let dataURL = canvas.toDataURL("image/png");

    return { data: LongStringActor(this.conn, dataURL), size: textWidth };
  }, {
    request: {font: Arg(0, "string"), fillStyle: Arg(1, "nullable:string")},
    response: RetVal("imageData")
  })
});

/**
 * Client side of the node actor.
 *
 * Node fronts are strored in a tree that mirrors the DOM tree on the
 * server, but with a few key differences:
 *  - Not all children will be necessary loaded for each node.
 *  - The order of children isn't guaranteed to be the same as the DOM.
 * Children are stored in a doubly-linked list, to make addition/removal
 * and traversal quick.
 *
 * Due to the order/incompleteness of the child list, it is safe to use
 * the parent node from clients, but the `children` request should be used
 * to traverse children.
 */
let NodeFront = protocol.FrontClass(NodeActor, {
  initialize: function(conn, form, detail, ctx) {
    this._parent = null; // The parent node
    this._child = null;  // The first child of this node.
    this._next = null;   // The next sibling of this node.
    this._prev = null;   // The previous sibling of this node.
    protocol.Front.prototype.initialize.call(this, conn, form, detail, ctx);
  },

  /**
   * Destroy a node front.  The node must have been removed from the
   * ownership tree before this is called, unless the whole walker front
   * is being destroyed.
   */
  destroy: function() {
    // If an observer was added on this node, shut it down.
    if (this.observer) {
      this.observer.disconnect();
      this.observer = null;
    }

    protocol.Front.prototype.destroy.call(this);
  },

  // Update the object given a form representation off the wire.
  form: function(form, detail, ctx) {
    if (detail === "actorid") {
      this.actorID = form;
      return;
    }
    // Shallow copy of the form.  We could just store a reference, but
    // eventually we'll want to update some of the data.
    this._form = object.merge(form);
    this._form.attrs = this._form.attrs ? this._form.attrs.slice() : [];

    if (form.parent) {
      // Get the owner actor for this actor (the walker), and find the
      // parent node of this actor from it, creating a standin node if
      // necessary.
      let parentNodeFront = ctx.marshallPool().ensureParentFront(form.parent);
      this.reparent(parentNodeFront);
    }
  },

  /**
   * Returns the parent NodeFront for this NodeFront.
   */
  parentNode: function() {
    return this._parent;
  },

  /**
   * Process a mutation entry as returned from the walker's `getMutations`
   * request.  Only tries to handle changes of the node's contents
   * themselves (character data and attribute changes), the walker itself
   * will keep the ownership tree up to date.
   */
  updateMutation: function(change) {
    if (change.type === "attributes") {
      // We'll need to lazily reparse the attributes after this change.
      this._attrMap = undefined;

      // Update any already-existing attributes.
      let found = false;
      for (let i = 0; i < this.attributes.length; i++) {
        let attr = this.attributes[i];
        if (attr.name == change.attributeName &&
            attr.namespace == change.attributeNamespace) {
          if (change.newValue !== null) {
            attr.value = change.newValue;
          } else {
            this.attributes.splice(i, 1);
          }
          found = true;
          break;
        }
      }
      // This is a new attribute.
      if (!found)  {
        this.attributes.push({
          name: change.attributeName,
          namespace: change.attributeNamespace,
          value: change.newValue
        });
      }
    } else if (change.type === "characterData") {
      this._form.shortValue = change.newValue;
      this._form.incompleteValue = change.incompleteValue;
    } else if (change.type === "pseudoClassLock") {
      this._form.pseudoClassLocks = change.pseudoClassLocks;
    }
  },

  // Some accessors to make NodeFront feel more like an nsIDOMNode

  get id() this.getAttribute("id"),

  get nodeType() this._form.nodeType,
  get namespaceURI() this._form.namespaceURI,
  get nodeName() this._form.nodeName,

  get baseURI() this._form.baseURI,

  get className() {
    return this.getAttribute("class") || '';
  },

  get hasChildren() this._form.numChildren > 0,
  get numChildren() this._form.numChildren,

  get tagName() this.nodeType === Ci.nsIDOMNode.ELEMENT_NODE ? this.nodeName : null,
  get shortValue() this._form.shortValue,
  get incompleteValue() !!this._form.incompleteValue,

  get isDocumentElement() !!this._form.isDocumentElement,

  // doctype properties
  get name() this._form.name,
  get publicId() this._form.publicId,
  get systemId() this._form.systemId,

  getAttribute: function(name) {
    let attr = this._getAttribute(name);
    return attr ? attr.value : null;
  },
  hasAttribute: function(name) {
    this._cacheAttributes();
    return (name in this._attrMap);
  },

  get hidden() {
    let cls = this.getAttribute("class");
    return cls && cls.indexOf(HIDDEN_CLASS) > -1;
  },

  get attributes() this._form.attrs,

  get pseudoClassLocks() this._form.pseudoClassLocks || [],
  hasPseudoClassLock: function(pseudo) {
    return this.pseudoClassLocks.some(locked => locked === pseudo);
  },

  get isDisplayed() {
    // The NodeActor's form contains the isDisplayed information as a boolean
    // starting from FF32. Before that, the property is missing
    return "isDisplayed" in this._form ? this._form.isDisplayed : true;
  },

  getNodeValue: protocol.custom(function() {
    if (!this.incompleteValue) {
      return delayedResolve(new ShortLongString(this.shortValue));
    } else {
      return this._getNodeValue();
    }
  }, {
    impl: "_getNodeValue"
  }),

  /**
   * Return a new AttributeModificationList for this node.
   */
  startModifyingAttributes: function() {
    return AttributeModificationList(this);
  },

  _cacheAttributes: function() {
    if (typeof(this._attrMap) != "undefined") {
      return;
    }
    this._attrMap = {};
    for (let attr of this.attributes) {
      this._attrMap[attr.name] = attr;
    }
  },

  _getAttribute: function(name) {
    this._cacheAttributes();
    return this._attrMap[name] || undefined;
  },

  /**
   * Set this node's parent.  Note that the children saved in
   * this tree are unordered and incomplete, so shouldn't be used
   * instead of a `children` request.
   */
  reparent: function(parent) {
    if (this._parent === parent) {
      return;
    }

    if (this._parent && this._parent._child === this) {
      this._parent._child = this._next;
    }
    if (this._prev) {
      this._prev._next = this._next;
    }
    if (this._next) {
      this._next._prev = this._prev;
    }
    this._next = null;
    this._prev = null;
    this._parent = parent;
    if (!parent) {
      // Subtree is disconnected, we're done
      return;
    }
    this._next = parent._child;
    if (this._next) {
      this._next._prev = this;
    }
    parent._child = this;
  },

  /**
   * Return all the known children of this node.
   */
  treeChildren: function() {
    let ret = [];
    for (let child = this._child; child != null; child = child._next) {
      ret.push(child);
    }
    return ret;
  },

  /**
   * Do we use a local target?
   * Useful to know if a rawNode is available or not.
   *
   * This will, one day, be removed. External code should
   * not need to know if the target is remote or not.
   */
  isLocal_toBeDeprecated: function() {
    return !!this.conn._transport._serverConnection;
  },

  /**
   * Get an nsIDOMNode for the given node front.  This only works locally,
   * and is only intended as a stopgap during the transition to the remote
   * protocol.  If you depend on this you're likely to break soon.
   */
  rawNode: function(rawNode) {
    if (!this.conn._transport._serverConnection) {
      console.warn("Tried to use rawNode on a remote connection.");
      return null;
    }
    let actor = this.conn._transport._serverConnection.getActor(this.actorID);
    if (!actor) {
      // Can happen if we try to get the raw node for an already-expired
      // actor.
      return null;
    }
    return actor.rawNode;
  }
});

/**
 * Returned from any call that might return a node that isn't connected to root by
 * nodes the child has seen, such as querySelector.
 */
types.addDictType("disconnectedNode", {
  // The actual node to return
  node: "domnode",

  // Nodes that are needed to connect the node to a node the client has already seen
  newParents: "array:domnode"
});

types.addDictType("disconnectedNodeArray", {
  // The actual node list to return
  nodes: "array:domnode",

  // Nodes that are needed to connect those nodes to the root.
  newParents: "array:domnode"
});

types.addDictType("dommutation", {});

/**
 * Server side of a node list as returned by querySelectorAll()
 */
var NodeListActor = exports.NodeListActor = protocol.ActorClass({
  typeName: "domnodelist",

  initialize: function(walker, nodeList) {
    protocol.Actor.prototype.initialize.call(this);
    this.walker = walker;
    this.nodeList = nodeList;
  },

  destroy: function() {
    protocol.Actor.prototype.destroy.call(this);
  },

  /**
   * Instead of storing a connection object, the NodeActor gets its connection
   * from its associated walker.
   */
  get conn() {
    return this.walker.conn;
  },

  /**
   * Items returned by this actor should belong to the parent walker.
   */
  marshallPool: function() {
    return this.walker;
  },

  // Returns the JSON representation of this object over the wire.
  form: function() {
    return {
      actor: this.actorID,
      length: this.nodeList.length
    }
  },

  /**
   * Get a single node from the node list.
   */
  item: method(function(index) {
    return this.walker.attachElement(this.nodeList[index]);
  }, {
    request: { item: Arg(0) },
    response: RetVal("disconnectedNode")
  }),

  /**
   * Get a range of the items from the node list.
   */
  items: method(function(start=0, end=this.nodeList.length) {
    let items = [this.walker._ref(item) for (item of Array.prototype.slice.call(this.nodeList, start, end))];
    let newParents = new Set();
    for (let item of items) {
      this.walker.ensurePathToRoot(item, newParents);
    }
    return {
      nodes: items,
      newParents: [node for (node of newParents)]
    }
  }, {
    request: {
      start: Arg(0, "nullable:number"),
      end: Arg(1, "nullable:number")
    },
    response: RetVal("disconnectedNodeArray")
  }),

  release: method(function() {}, { release: true })
});

/**
 * Client side of a node list as returned by querySelectorAll()
 */
var NodeListFront = exports.NodeListFront = protocol.FrontClass(NodeListActor, {
  initialize: function(client, form) {
    protocol.Front.prototype.initialize.call(this, client, form);
  },

  destroy: function() {
    protocol.Front.prototype.destroy.call(this);
  },

  marshallPool: function() {
    return this.parent();
  },

  // Update the object given a form representation off the wire.
  form: function(json) {
    this.length = json.length;
  },

  item: protocol.custom(function(index) {
    return this._item(index).then(response => {
      return response.node;
    });
  }, {
    impl: "_item"
  }),

  items: protocol.custom(function(start, end) {
    return this._items(start, end).then(response => {
      return response.nodes;
    });
  }, {
    impl: "_items"
  })
});

// Some common request/response templates for the dom walker

let nodeArrayMethod = {
  request: {
    node: Arg(0, "domnode"),
    maxNodes: Option(1),
    center: Option(1, "domnode"),
    start: Option(1, "domnode"),
    whatToShow: Option(1)
  },
  response: RetVal(types.addDictType("domtraversalarray", {
    nodes: "array:domnode"
  }))
};

let traversalMethod = {
  request: {
    node: Arg(0, "domnode"),
    whatToShow: Option(1)
  },
  response: {
    node: RetVal("nullable:domnode")
  }
}

/**
 * Server side of the DOM walker.
 */
var WalkerActor = protocol.ActorClass({
  typeName: "domwalker",

  events: {
    "new-mutations" : {
      type: "newMutations"
    },
    "picker-node-picked" : {
      type: "pickerNodePicked",
      node: Arg(0, "disconnectedNode")
    },
    "picker-node-hovered" : {
      type: "pickerNodeHovered",
      node: Arg(0, "disconnectedNode")
    },
    "highlighter-ready" : {
      type: "highlighter-ready"
    },
    "highlighter-hide" : {
      type: "highlighter-hide"
    },
    "display-change" : {
      type: "display-change",
      nodes: Arg(0, "array:domnode")
    }
  },

  /**
   * Create the WalkerActor
   * @param DebuggerServerConnection conn
   *    The server connection.
   */
  initialize: function(conn, tabActor, options) {
    protocol.Actor.prototype.initialize.call(this, conn);
    this.tabActor = tabActor;
    this.rootWin = tabActor.window;
    this.rootDoc = this.rootWin.document;
    this._refMap = new Map();
    this._pendingMutations = [];
    this._activePseudoClassLocks = new Set();

    this.layoutHelpers = new LayoutHelpers(this.rootWin);

    // Nodes which have been removed from the client's known
    // ownership tree are considered "orphaned", and stored in
    // this set.
    this._orphaned = new Set();

    // The client can tell the walker that it is interested in a node
    // even when it is orphaned with the `retainNode` method.  This
    // list contains orphaned nodes that were so retained.
    this._retainedOrphans = new Set();

    this.onMutations = this.onMutations.bind(this);
    this.onFrameLoad = this.onFrameLoad.bind(this);
    this.onFrameUnload = this.onFrameUnload.bind(this);

    events.on(tabActor, "will-navigate", this.onFrameUnload);
    events.on(tabActor, "navigate", this.onFrameLoad);

    // Ensure that the root document node actor is ready and
    // managed.
    this.rootNode = this.document();

    this.reflowObserver = getLayoutChangesObserver(this.tabActor);
    this._onReflows = this._onReflows.bind(this);
    this.reflowObserver.on("reflows", this._onReflows);
  },

  // Returns the JSON representation of this object over the wire.
  form: function() {
    return {
      actor: this.actorID,
      root: this.rootNode.form()
    }
  },

  toString: function() {
    return "[WalkerActor " + this.actorID + "]";
  },

  destroy: function() {
    this._destroyed = true;

    this.clearPseudoClassLocks();
    this._activePseudoClassLocks = null;

    this._hoveredNode = null;
    this.rootDoc = null;

    this.reflowObserver.off("reflows", this._onReflows);
    this.reflowObserver = null;
    releaseLayoutChangesObserver(this.tabActor);

    events.emit(this, "destroyed");
    protocol.Actor.prototype.destroy.call(this);
  },

  release: method(function() {}, { release: true }),

  unmanage: function(actor) {
    if (actor instanceof NodeActor) {
      if (this._activePseudoClassLocks &&
          this._activePseudoClassLocks.has(actor)) {
        this.clearPseudoClassLocks(actor);
      }
      this._refMap.delete(actor.rawNode);
    }
    protocol.Actor.prototype.unmanage.call(this, actor);
  },

  _ref: function(node) {
    let actor = this._refMap.get(node);
    if (actor) return actor;

    actor = new NodeActor(this, node);

    // Add the node actor as a child of this walker actor, assigning
    // it an actorID.
    this.manage(actor);
    this._refMap.set(node, actor);

    if (node.nodeType === Ci.nsIDOMNode.DOCUMENT_NODE) {
      this._watchDocument(actor);
    }
    return actor;
  },

  _onReflows: function(reflows) {
    // Going through the nodes the walker knows about, see which ones have
    // had their display changed and send a display-change event if any
    let changes = [];
    for (let [node, actor] of this._refMap) {
      let isDisplayed = actor.isDisplayed;
      if (isDisplayed !== actor.wasDisplayed) {
        changes.push(actor);
        // Updating the original value
        actor.wasDisplayed = isDisplayed;
      }
    }

    if (changes.length) {
      events.emit(this, "display-change", changes);
    }
  },

  /**
   * This is kept for backward-compatibility reasons with older remote targets.
   * Targets prior to bug 916443.
   *
   * pick/cancelPick are used to pick a node on click on the content
   * document. But in their implementation prior to bug 916443, they don't allow
   * highlighting on hover.
   * The client-side now uses the highlighter actor's pick and cancelPick
   * methods instead. The client-side uses the the highlightable trait found in
   * the root actor to determine which version of pick to use.
   *
   * As for highlight, the new highlighter actor is used instead of the walker's
   * highlight method. Same here though, the client-side uses the highlightable
   * trait to dertermine which to use.
   *
   * Keeping these actor methods for now allows newer client-side debuggers to
   * inspect fxos 1.2 remote targets or older firefox desktop remote targets.
   */
  pick: method(function() {}, {request: {}, response: RetVal("disconnectedNode")}),
  cancelPick: method(function() {}),
  highlight: method(function(node) {}, {request: {node: Arg(0, "nullable:domnode")}}),

  attachElement: function(node) {
    node = this._ref(node);
    let newParents = this.ensurePathToRoot(node);
    return {
      node: node,
      newParents: [parent for (parent of newParents)]
    };
  },

  /**
   * Watch the given document node for mutations using the DOM observer
   * API.
   */
  _watchDocument: function(actor) {
    let node = actor.rawNode;
    // Create the observer on the node's actor.  The node will make sure
    // the observer is cleaned up when the actor is released.
    actor.observer = new actor.rawNode.defaultView.MutationObserver(this.onMutations);
    actor.observer.observe(node, {
      attributes: true,
      characterData: true,
      childList: true,
      subtree: true
    });
  },

  /**
   * Return the document node that contains the given node,
   * or the root node if no node is specified.
   * @param NodeActor node
   *        The node whose document is needed, or null to
   *        return the root.
   */
  document: method(function(node) {
    let doc = node ? nodeDocument(node.rawNode) : this.rootDoc;
    return this._ref(doc);
  }, {
    request: { node: Arg(0, "nullable:domnode") },
    response: { node: RetVal("domnode") },
  }),

  /**
   * Return the documentElement for the document containing the
   * given node.
   * @param NodeActor node
   *        The node whose documentElement is requested, or null
   *        to use the root document.
   */
  documentElement: method(function(node) {
    let elt = node ? nodeDocument(node.rawNode).documentElement : this.rootDoc.documentElement;
    return this._ref(elt);
  }, {
    request: { node: Arg(0, "nullable:domnode") },
    response: { node: RetVal("domnode") },
  }),

  /**
   * Return all parents of the given node, ordered from immediate parent
   * to root.
   * @param NodeActor node
   *    The node whose parents are requested.
   * @param object options
   *    Named options, including:
   *    `sameDocument`: If true, parents will be restricted to the same
   *      document as the node.
   */
  parents: method(function(node, options={}) {
    let walker = documentWalker(node.rawNode, this.rootWin);
    let parents = [];
    let cur;
    while((cur = walker.parentNode())) {
      if (options.sameDocument && cur.ownerDocument != node.rawNode.ownerDocument) {
        break;
      }
      parents.push(this._ref(cur));
    }
    return parents;
  }, {
    request: {
      node: Arg(0, "domnode"),
      sameDocument: Option(1)
    },
    response: {
      nodes: RetVal("array:domnode")
    },
  }),

  parentNode: function(node) {
    let walker = documentWalker(node.rawNode, this.rootWin);
    let parent = walker.parentNode();
    if (parent) {
      return this._ref(parent);
    }
    return null;
  },

  /**
   * Mark a node as 'retained'.
   *
   * A retained node is not released when `releaseNode` is called on its
   * parent, or when a parent is released with the `cleanup` option to
   * `getMutations`.
   *
   * When a retained node's parent is released, a retained mode is added to
   * the walker's "retained orphans" list.
   *
   * Retained nodes can be deleted by providing the `force` option to
   * `releaseNode`.  They will also be released when their document
   * has been destroyed.
   *
   * Retaining a node makes no promise about its children;  They can
   * still be removed by normal means.
   */
  retainNode: method(function(node) {
    node.retained = true;
  }, {
    request: { node: Arg(0, "domnode") },
    response: {}
  }),

  /**
   * Remove the 'retained' mark from a node.  If the node was a
   * retained orphan, release it.
   */
  unretainNode: method(function(node) {
    node.retained = false;
    if (this._retainedOrphans.has(node)) {
      this._retainedOrphans.delete(node);
      this.releaseNode(node);
    }
  }, {
    request: { node: Arg(0, "domnode") },
    response: {},
  }),

  /**
   * Release actors for a node and all child nodes.
   */
  releaseNode: method(function(node, options={}) {
    if (node.retained && !options.force) {
      this._retainedOrphans.add(node);
      return;
    }

    if (node.retained) {
      // Forcing a retained node to go away.
      this._retainedOrphans.delete(node);
    }

    let walker = documentWalker(node.rawNode, this.rootWin);

    let child = walker.firstChild();
    while (child) {
      let childActor = this._refMap.get(child);
      if (childActor) {
        this.releaseNode(childActor, options);
      }
      child = walker.nextSibling();
    }

    node.destroy();
  }, {
    request: {
      node: Arg(0, "domnode"),
      force: Option(1)
    }
  }),

  /**
   * Add any nodes between `node` and the walker's root node that have not
   * yet been seen by the client.
   */
  ensurePathToRoot: function(node, newParents=new Set()) {
    if (!node) {
      return newParents;
    }
    let walker = documentWalker(node.rawNode, this.rootWin);
    let cur;
    while ((cur = walker.parentNode())) {
      let parent = this._refMap.get(cur);
      if (!parent) {
        // This parent didn't exist, so hasn't been seen by the client yet.
        newParents.add(this._ref(cur));
      } else {
        // This parent did exist, so the client knows about it.
        return newParents;
      }
    }
    return newParents;
  },

  /**
   * Return children of the given node.  By default this method will return
   * all children of the node, but there are options that can restrict this
   * to a more manageable subset.
   *
   * @param NodeActor node
   *    The node whose children you're curious about.
   * @param object options
   *    Named options:
   *    `maxNodes`: The set of nodes returned by the method will be no longer
   *       than maxNodes.
   *    `start`: If a node is specified, the list of nodes will start
   *       with the given child.  Mutally exclusive with `center`.
   *    `center`: If a node is specified, the given node will be as centered
   *       as possible in the list, given how close to the ends of the child
   *       list it is.  Mutually exclusive with `start`.
   *    `whatToShow`: A bitmask of node types that should be included.  See
   *       https://developer.mozilla.org/en-US/docs/Web/API/NodeFilter.
   *
   * @returns an object with three items:
   *    hasFirst: true if the first child of the node is included in the list.
   *    hasLast: true if the last child of the node is included in the list.
   *    nodes: Child nodes returned by the request.
   */
  children: method(function(node, options={}) {
    if (options.center && options.start) {
      throw Error("Can't specify both 'center' and 'start' options.");
    }
    let maxNodes = options.maxNodes || -1;
    if (maxNodes == -1) {
      maxNodes = Number.MAX_VALUE;
    }

    // We're going to create a few document walkers with the same filter,
    // make it easier.
    let filteredWalker = (node) => {
      return documentWalker(node, this.rootWin, options.whatToShow);
    };

    // Need to know the first and last child.
    let rawNode = node.rawNode;
    let firstChild = filteredWalker(rawNode).firstChild();
    let lastChild = filteredWalker(rawNode).lastChild();

    if (!firstChild) {
      // No children, we're done.
      return { hasFirst: true, hasLast: true, nodes: [] };
    }

    let start;
    if (options.center) {
      start = options.center.rawNode;
    } else if (options.start) {
      start = options.start.rawNode;
    } else {
      start = firstChild;
    }

    let nodes = [];

    // Start by reading backward from the starting point if we're centering...
    let backwardWalker = filteredWalker(start);
    if (start != firstChild && options.center) {
      backwardWalker.previousSibling();
      let backwardCount = Math.floor(maxNodes / 2);
      let backwardNodes = this._readBackward(backwardWalker, backwardCount);
      nodes = backwardNodes;
    }

    // Then read forward by any slack left in the max children...
    let forwardWalker = filteredWalker(start);
    let forwardCount = maxNodes - nodes.length;
    nodes = nodes.concat(this._readForward(forwardWalker, forwardCount));

    // If there's any room left, it means we've run all the way to the end.
    // If we're centering, check if there are more items to read at the front.
    let remaining = maxNodes - nodes.length;
    if (options.center && remaining > 0 && nodes[0].rawNode != firstChild) {
      let firstNodes = this._readBackward(backwardWalker, remaining);

      // Then put it all back together.
      nodes = firstNodes.concat(nodes);
    }

    return {
      hasFirst: nodes[0].rawNode == firstChild,
      hasLast: nodes[nodes.length - 1].rawNode == lastChild,
      nodes: nodes
    };
  }, nodeArrayMethod),

  /**
   * Return siblings of the given node.  By default this method will return
   * all siblings of the node, but there are options that can restrict this
   * to a more manageable subset.
   *
   * If `start` or `center` are not specified, this method will center on the
   * node whose siblings are requested.
   *
   * @param NodeActor node
   *    The node whose children you're curious about.
   * @param object options
   *    Named options:
   *    `maxNodes`: The set of nodes returned by the method will be no longer
   *       than maxNodes.
   *    `start`: If a node is specified, the list of nodes will start
   *       with the given child.  Mutally exclusive with `center`.
   *    `center`: If a node is specified, the given node will be as centered
   *       as possible in the list, given how close to the ends of the child
   *       list it is.  Mutually exclusive with `start`.
   *    `whatToShow`: A bitmask of node types that should be included.  See
   *       https://developer.mozilla.org/en-US/docs/Web/API/NodeFilter.
   *
   * @returns an object with three items:
   *    hasFirst: true if the first child of the node is included in the list.
   *    hasLast: true if the last child of the node is included in the list.
   *    nodes: Child nodes returned by the request.
   */
  siblings: method(function(node, options={}) {
    let parentNode = documentWalker(node.rawNode, this.rootWin).parentNode();
    if (!parentNode) {
      return {
        hasFirst: true,
        hasLast: true,
        nodes: [node]
      };
    }

    if (!(options.start || options.center)) {
      options.center = node;
    }

    return this.children(this._ref(parentNode), options);
  }, nodeArrayMethod),

  /**
   * Get the next sibling of a given node.  Getting nodes one at a time
   * might be inefficient, be careful.
   *
   * @param object options
   *    Named options:
   *    `whatToShow`: A bitmask of node types that should be included.  See
   *       https://developer.mozilla.org/en-US/docs/Web/API/NodeFilter.
   */
  nextSibling: method(function(node, options={}) {
    let walker = documentWalker(node.rawNode, this.rootWin, options.whatToShow || Ci.nsIDOMNodeFilter.SHOW_ALL);
    let sibling = walker.nextSibling();
    return sibling ? this._ref(sibling) : null;
  }, traversalMethod),

  /**
   * Get the previous sibling of a given node.  Getting nodes one at a time
   * might be inefficient, be careful.
   *
   * @param object options
   *    Named options:
   *    `whatToShow`: A bitmask of node types that should be included.  See
   *       https://developer.mozilla.org/en-US/docs/Web/API/NodeFilter.
   */
  previousSibling: method(function(node, options={}) {
    let walker = documentWalker(node.rawNode, this.rootWin, options.whatToShow || Ci.nsIDOMNodeFilter.SHOW_ALL);
    let sibling = walker.previousSibling();
    return sibling ? this._ref(sibling) : null;
  }, traversalMethod),

  /**
   * Helper function for the `children` method: Read forward in the sibling
   * list into an array with `count` items, including the current node.
   */
  _readForward: function(walker, count) {
    let ret = [];
    let node = walker.currentNode;
    do {
      ret.push(this._ref(node));
      node = walker.nextSibling();
    } while (node && --count);
    return ret;
  },

  /**
   * Helper function for the `children` method: Read backward in the sibling
   * list into an array with `count` items, including the current node.
   */
  _readBackward: function(walker, count) {
    let ret = [];
    let node = walker.currentNode;
    do {
      ret.push(this._ref(node));
      node = walker.previousSibling();
    } while(node && --count);
    ret.reverse();
    return ret;
  },

  /**
   * Return the first node in the document that matches the given selector.
   * See https://developer.mozilla.org/en-US/docs/Web/API/Element.querySelector
   *
   * @param NodeActor baseNode
   * @param string selector
   */
  querySelector: method(function(baseNode, selector) {
    if (!baseNode) {
      return {}
    };
    let node = baseNode.rawNode.querySelector(selector);

    if (!node) {
      return {}
    };

    return this.attachElement(node);
  }, {
    request: {
      node: Arg(0, "domnode"),
      selector: Arg(1)
    },
    response: RetVal("disconnectedNode")
  }),

  /**
   * Return a NodeListActor with all nodes that match the given selector.
   * See https://developer.mozilla.org/en-US/docs/Web/API/Element.querySelectorAll
   *
   * @param NodeActor baseNode
   * @param string selector
   */
  querySelectorAll: method(function(baseNode, selector) {
    let nodeList = null;

    try {
      nodeList = baseNode.rawNode.querySelectorAll(selector);
    } catch(e) {
      // Bad selector. Do nothing as the selector can come from a searchbox.
    }

    return new NodeListActor(this, nodeList);
  }, {
    request: {
      node: Arg(0, "domnode"),
      selector: Arg(1)
    },
    response: {
      list: RetVal("domnodelist")
    }
  }),

  /**
   * Returns a list of matching results for CSS selector autocompletion.
   *
   * @param string query
   *        The selector query being completed
   * @param string completing
   *        The exact token being completed out of the query
   * @param string selectorState
   *        One of "pseudo", "id", "tag", "class", "null"
   */
  getSuggestionsForQuery: method(function(query, completing, selectorState) {
    let sugs = {
      classes: new Map,
      tags: new Map
    };
    let result = [];
    let nodes = null;
    // Filtering and sorting the results so that protocol transfer is miminal.
    switch (selectorState) {
      case "pseudo":
        result = PSEUDO_SELECTORS.filter(item => {
          return item[0].startsWith(":" + completing);
        });
        break;

      case "class":
        if (!query) {
          nodes = this.rootDoc.querySelectorAll("[class]");
        }
        else {
          nodes = this.rootDoc.querySelectorAll(query);
        }
        for (let node of nodes) {
          for (let className of node.className.split(" ")) {
            sugs.classes.set(className, (sugs.classes.get(className)|0) + 1);
          }
        }
        sugs.classes.delete("");
        // Editing the style editor may make the stylesheet have errors and
        // thus the page's elements' styles start changing with a transition.
        // That transition comes from the `moz-styleeditor-transitioning` class.
        sugs.classes.delete("moz-styleeditor-transitioning");
        sugs.classes.delete(HIDDEN_CLASS);
        for (let [className, count] of sugs.classes) {
          if (className.startsWith(completing)) {
            result.push(["." + className, count]);
          }
        }
        break;

      case "id":
        if (!query) {
          nodes = this.rootDoc.querySelectorAll("[id]");
        }
        else {
          nodes = this.rootDoc.querySelectorAll(query);
        }
        for (let node of nodes) {
          if (node.id.startsWith(completing)) {
            result.push(["#" + node.id, 1]);
          }
        }
        break;

      case "tag":
        if (!query) {
          nodes = this.rootDoc.getElementsByTagName("*");
        }
        else {
          nodes = this.rootDoc.querySelectorAll(query);
        }
        for (let node of nodes) {
          let tag = node.tagName.toLowerCase();
          sugs.tags.set(tag, (sugs.tags.get(tag)|0) + 1);
        }
        for (let [tag, count] of sugs.tags) {
          if ((new RegExp("^" + completing + ".*", "i")).test(tag)) {
            result.push([tag, count]);
          }
        }
        break;

      case "null":
        nodes = this.rootDoc.querySelectorAll(query);
        for (let node of nodes) {
          node.id && result.push(["#" + node.id, 1]);
          let tag = node.tagName.toLowerCase();
          sugs.tags.set(tag, (sugs.tags.get(tag)|0) + 1);
          for (let className of node.className.split(" ")) {
            sugs.classes.set(className, (sugs.classes.get(className)|0) + 1);
          }
        }
        for (let [tag, count] of sugs.tags) {
          tag && result.push([tag, count]);
        }
        sugs.classes.delete("");
        // Editing the style editor may make the stylesheet have errors and
        // thus the page's elements' styles start changing with a transition.
        // That transition comes from the `moz-styleeditor-transitioning` class.
        sugs.classes.delete("moz-styleeditor-transitioning");
        sugs.classes.delete(HIDDEN_CLASS);
        for (let [className, count] of sugs.classes) {
          className && result.push(["." + className, count]);
        }
    }

    // Sort alphabetically in increaseing order.
    result = result.sort();
    // Sort based on count in decreasing order.
    result = result.sort(function(a, b) {
      return b[1] - a[1];
    });

    result.slice(0, 25);

    return {
      query: query,
      suggestions: result
    };
  }, {
    request: {
      query: Arg(0),
      completing: Arg(1),
      selectorState: Arg(2)
    },
    response: {
      list: RetVal("array:array:string")
    }
  }),

  /**
   * Add a pseudo-class lock to a node.
   *
   * @param NodeActor node
   * @param string pseudo
   *    A pseudoclass: ':hover', ':active', ':focus'
   * @param options
   *    Options object:
   *    `parents`: True if the pseudo-class should be added
   *      to parent nodes.
   *
   * @returns An empty packet.  A "pseudoClassLock" mutation will
   *    be queued for any changed nodes.
   */
  addPseudoClassLock: method(function(node, pseudo, options={}) {
    this._addPseudoClassLock(node, pseudo);

    if (!options.parents) {
      return;
    }

    let walker = documentWalker(node.rawNode, this.rootWin);
    let cur;
    while ((cur = walker.parentNode())) {
      let curNode = this._ref(cur);
      this._addPseudoClassLock(curNode, pseudo);
    }
  }, {
    request: {
      node: Arg(0, "domnode"),
      pseudoClass: Arg(1),
      parents: Option(2)
    },
    response: {}
  }),

  _queuePseudoClassMutation: function(node) {
    this.queueMutation({
      target: node.actorID,
      type: "pseudoClassLock",
      pseudoClassLocks: node.writePseudoClassLocks()
    });
  },

  _addPseudoClassLock: function(node, pseudo) {
    if (node.rawNode.nodeType !== Ci.nsIDOMNode.ELEMENT_NODE) {
      return false;
    }
    DOMUtils.addPseudoClassLock(node.rawNode, pseudo);
    this._activePseudoClassLocks.add(node);
    this._queuePseudoClassMutation(node);
    return true;
  },

  _installHelperSheet: function(node) {
    if (!this.installedHelpers) {
      this.installedHelpers = new WeakMap;
    }
    let win = node.rawNode.ownerDocument.defaultView;
    if (!this.installedHelpers.has(win)) {
      let { Style } = require("sdk/stylesheet/style");
      let { attach } = require("sdk/content/mod");
      let style = Style({source: HELPER_SHEET, type: "agent" });
      attach(style, win);
      this.installedHelpers.set(win, style);
    }
  },

  hideNode: method(function(node) {
    this._installHelperSheet(node);
    node.rawNode.classList.add(HIDDEN_CLASS);
  }, {
    request: { node: Arg(0, "domnode") }
  }),

  unhideNode: method(function(node) {
    node.rawNode.classList.remove(HIDDEN_CLASS);
  }, {
    request: { node: Arg(0, "domnode") }
  }),

  /**
   * Remove a pseudo-class lock from a node.
   *
   * @param NodeActor node
   * @param string pseudo
   *    A pseudoclass: ':hover', ':active', ':focus'
   * @param options
   *    Options object:
   *    `parents`: True if the pseudo-class should be removed
   *      from parent nodes.
   *
   * @returns An empty response.  "pseudoClassLock" mutations
   *    will be emitted for any changed nodes.
   */
  removePseudoClassLock: method(function(node, pseudo, options={}) {
    this._removePseudoClassLock(node, pseudo);

    if (!options.parents) {
      return;
    }

    let walker = documentWalker(node.rawNode, this.rootWin);
    let cur;
    while ((cur = walker.parentNode())) {
      let curNode = this._ref(cur);
      this._removePseudoClassLock(curNode, pseudo);
    }
  }, {
    request: {
      node: Arg(0, "domnode"),
      pseudoClass: Arg(1),
      parents: Option(2)
    },
    response: {}
  }),

  _removePseudoClassLock: function(node, pseudo) {
    if (node.rawNode.nodeType != Ci.nsIDOMNode.ELEMENT_NODE) {
      return false;
    }
    DOMUtils.removePseudoClassLock(node.rawNode, pseudo);
    if (!node.writePseudoClassLocks()) {
      this._activePseudoClassLocks.delete(node);
    }

    this._queuePseudoClassMutation(node);
    return true;
  },

  /**
   * Clear all the pseudo-classes on a given node or all nodes.
   * @param {NodeActor} node Optional node to clear pseudo-classes on
   */
  clearPseudoClassLocks: method(function(node) {
    if (node) {
      DOMUtils.clearPseudoClassLocks(node.rawNode);
      this._activePseudoClassLocks.delete(node);
      this._queuePseudoClassMutation(node);
    } else {
      for (let locked of this._activePseudoClassLocks) {
        DOMUtils.clearPseudoClassLocks(locked.rawNode);
        this._activePseudoClassLocks.delete(locked);
        this._queuePseudoClassMutation(locked);
      }
    }
  }, {
    request: {
      node: Arg(0, "nullable:domnode")
    },
    response: {}
  }),

  /**
   * Get a node's innerHTML property.
   */
  innerHTML: method(function(node) {
    return LongStringActor(this.conn, node.rawNode.innerHTML);
  }, {
    request: {
      node: Arg(0, "domnode")
    },
    response: {
      value: RetVal("longstring")
    }
  }),

  /**
   * Get a node's outerHTML property.
   */
  outerHTML: method(function(node) {
    return LongStringActor(this.conn, node.rawNode.outerHTML);
  }, {
    request: {
      node: Arg(0, "domnode")
    },
    response: {
      value: RetVal("longstring")
    }
  }),

  /**
   * Set a node's outerHTML property.
   */
  setOuterHTML: method(function(node, value) {
    let parsedDOM = DOMParser.parseFromString(value, "text/html");
    let rawNode = node.rawNode;
    let parentNode = rawNode.parentNode;

    // Special case for head and body.  Setting document.body.outerHTML
    // creates an extra <head> tag, and document.head.outerHTML creates
    // an extra <body>.  So instead we will call replaceChild with the
    // parsed DOM, assuming that they aren't trying to set both tags at once.
    if (rawNode.tagName === "BODY") {
      if (parsedDOM.head.innerHTML === "") {
        parentNode.replaceChild(parsedDOM.body, rawNode);
      } else {
        rawNode.outerHTML = value;
      }
    } else if (rawNode.tagName === "HEAD") {
      if (parsedDOM.body.innerHTML === "") {
        parentNode.replaceChild(parsedDOM.head, rawNode);
      } else {
        rawNode.outerHTML = value;
      }
    } else if (node.isDocumentElement()) {
      // Unable to set outerHTML on the document element.  Fall back by
      // setting attributes manually, then replace the body and head elements.
      let finalAttributeModifications = [];
      let attributeModifications = {};
      for (let attribute of rawNode.attributes) {
        attributeModifications[attribute.name] = null;
      }
      for (let attribute of parsedDOM.documentElement.attributes) {
        attributeModifications[attribute.name] = attribute.value;
      }
      for (let key in attributeModifications) {
        finalAttributeModifications.push({
          attributeName: key,
          newValue: attributeModifications[key]
        });
      }
      node.modifyAttributes(finalAttributeModifications);
      rawNode.replaceChild(parsedDOM.head, rawNode.querySelector("head"));
      rawNode.replaceChild(parsedDOM.body, rawNode.querySelector("body"));
    } else {
      rawNode.outerHTML = value;
    }
  }, {
    request: {
      node: Arg(0, "domnode"),
      value: Arg(1),
    },
    response: {
    }
  }),

  /**
   * Removes a node from its parent node.
   *
   * @returns The node's nextSibling before it was removed.
   */
  removeNode: method(function(node) {
    if ((node.rawNode.ownerDocument &&
         node.rawNode.ownerDocument.documentElement === this.rawNode) ||
         node.rawNode.nodeType === Ci.nsIDOMNode.DOCUMENT_NODE) {
      throw Error("Cannot remove document or document elements.");
    }
    let nextSibling = this.nextSibling(node);
    if (node.rawNode.parentNode) {
      node.rawNode.parentNode.removeChild(node.rawNode);
      // Mutation events will take care of the rest.
    }
    return nextSibling;
  }, {
    request: {
      node: Arg(0, "domnode")
    },
    response: {
      nextSibling: RetVal("nullable:domnode")
    }
  }),

  /**
   * Insert a node into the DOM.
   */
  insertBefore: method(function(node, parent, sibling) {
    parent.rawNode.insertBefore(node.rawNode, sibling ? sibling.rawNode : null);
  }, {
    request: {
      node: Arg(0, "domnode"),
      parent: Arg(1, "domnode"),
      sibling: Arg(2, "nullable:domnode")
    },
    response: {}
  }),

  /**
   * Get any pending mutation records.  Must be called by the client after
   * the `new-mutations` notification is received.  Returns an array of
   * mutation records.
   *
   * Mutation records have a basic structure:
   *
   * {
   *   type: attributes|characterData|childList,
   *   target: <domnode actor ID>,
   * }
   *
   * And additional attributes based on the mutation type:
   *
   * `attributes` type:
   *   attributeName: <string> - the attribute that changed
   *   attributeNamespace: <string> - the attribute's namespace URI, if any.
   *   newValue: <string> - The new value of the attribute, if any.
   *
   * `characterData` type:
   *   newValue: <string> - the new shortValue for the node
   *   [incompleteValue: true] - True if the shortValue was truncated.
   *
   * `childList` type is returned when the set of children for a node
   * has changed.  Includes extra data, which can be used by the client to
   * maintain its ownership subtree.
   *
   *   added: array of <domnode actor ID> - The list of actors *previously
   *     seen by the client* that were added to the target node.
   *   removed: array of <domnode actor ID> The list of actors *previously
   *     seen by the client* that were removed from the target node.
   *
   * Actors that are included in a MutationRecord's `removed` but
   * not in an `added` have been removed from the client's ownership
   * tree (either by being moved under a node the client has seen yet
   * or by being removed from the tree entirely), and is considered
   * 'orphaned'.
   *
   * Keep in mind that if a node that the client hasn't seen is moved
   * into or out of the target node, it will not be included in the
   * removedNodes and addedNodes list, so if the client is interested
   * in the new set of children it needs to issue a `children` request.
   */
  getMutations: method(function(options={}) {
    let pending = this._pendingMutations || [];
    this._pendingMutations = [];

    if (options.cleanup) {
      for (let node of this._orphaned) {
        // Release the orphaned node.  Nodes or children that have been
        // retained will be moved to this._retainedOrphans.
        this.releaseNode(node);
      }
      this._orphaned = new Set();
    }

    return pending;
  }, {
    request: {
      cleanup: Option(0)
    },
    response: {
      mutations: RetVal("array:dommutation")
    }
  }),

  queueMutation: function(mutation) {
    if (!this.actorID || this._destroyed) {
      // We've been destroyed, don't bother queueing this mutation.
      return;
    }
    // We only send the `new-mutations` notification once, until the client
    // fetches mutations with the `getMutations` packet.
    let needEvent = this._pendingMutations.length === 0;

    this._pendingMutations.push(mutation);

    if (needEvent) {
      events.emit(this, "new-mutations");
    }
  },

  /**
   * Handles mutations from the DOM mutation observer API.
   *
   * @param array[MutationRecord] mutations
   *    See https://developer.mozilla.org/en-US/docs/Web/API/MutationObserver#MutationRecord
   */
  onMutations: function(mutations) {
    for (let change of mutations) {
      let targetActor = this._refMap.get(change.target);
      if (!targetActor) {
        continue;
      }
      let targetNode = change.target;
      let mutation = {
        type: change.type,
        target: targetActor.actorID,
      }

      if (mutation.type === "attributes") {
        mutation.attributeName = change.attributeName;
        mutation.attributeNamespace = change.attributeNamespace || undefined;
        mutation.newValue = targetNode.getAttribute(mutation.attributeName);
      } else if (mutation.type === "characterData") {
        if (targetNode.nodeValue.length > gValueSummaryLength) {
          mutation.newValue = targetNode.nodeValue.substring(0, gValueSummaryLength);
          mutation.incompleteValue = true;
        } else {
          mutation.newValue = targetNode.nodeValue;
        }
      } else if (mutation.type === "childList") {
        // Get the list of removed and added actors that the client has seen
        // so that it can keep its ownership tree up to date.
        let removedActors = [];
        let addedActors = [];
        for (let removed of change.removedNodes) {
          let removedActor = this._refMap.get(removed);
          if (!removedActor) {
            // If the client never encountered this actor we don't need to
            // mention that it was removed.
            continue;
          }
          // While removed from the tree, nodes are saved as orphaned.
          this._orphaned.add(removedActor);
          removedActors.push(removedActor.actorID);
        }
        for (let added of change.addedNodes) {
          let addedActor = this._refMap.get(added);
          if (!addedActor) {
            // If the client never encounted this actor we don't need to tell
            // it about its addition for ownership tree purposes - if the
            // client wants to see the new nodes it can ask for children.
            continue;
          }
          // The actor is reconnected to the ownership tree, unorphan
          // it and let the client know so that its ownership tree is up
          // to date.
          this._orphaned.delete(addedActor);
          addedActors.push(addedActor.actorID);
        }
        mutation.numChildren = change.target.childNodes.length;
        mutation.removed = removedActors;
        mutation.added = addedActors;
      }
      this.queueMutation(mutation);
    }
  },

  onFrameLoad: function({ window, isTopLevel }) {
    if (!this.rootDoc && isTopLevel) {
      this.rootDoc = window.document;
      this.rootNode = this.document();
      this.queueMutation({
        type: "newRoot",
        target: this.rootNode.form()
      });
    }
    let frame = this.layoutHelpers.getFrameElement(window);
    let frameActor = this._refMap.get(frame);
    if (!frameActor) {
      return;
    }

    this.queueMutation({
      type: "frameLoad",
      target: frameActor.actorID,
    });

    // Send a childList mutation on the frame.
    this.queueMutation({
      type: "childList",
      target: frameActor.actorID,
      added: [],
      removed: []
    })
  },

  // Returns true if domNode is in window or a subframe.
  _childOfWindow: function(window, domNode) {
    let win = nodeDocument(domNode).defaultView;
    while (win) {
      if (win === window) {
        return true;
      }
      win = this.layoutHelpers.getFrameElement(win);
    }
    return false;
  },

  onFrameUnload: function({ window }) {
    // Any retained orphans that belong to this document
    // or its children need to be released, and a mutation sent
    // to notify of that.
    let releasedOrphans = [];

    for (let retained of this._retainedOrphans) {
      if (Cu.isDeadWrapper(retained.rawNode) ||
          this._childOfWindow(window, retained.rawNode)) {
        this._retainedOrphans.delete(retained);
        releasedOrphans.push(retained.actorID);
        this.releaseNode(retained, { force: true });
      }
    }

    if (releasedOrphans.length > 0) {
      this.queueMutation({
        target: this.rootNode.actorID,
        type: "unretained",
        nodes: releasedOrphans
      });
    }

    let doc = window.document;
    let documentActor = this._refMap.get(doc);
    if (!documentActor) {
      return;
    }

    if (this.rootDoc === doc) {
      this.rootDoc = null;
      this.rootNode = null;
    }

    this.queueMutation({
      type: "documentUnload",
      target: documentActor.actorID
    });

    let walker = documentWalker(doc, this.rootWin);
    let parentNode = walker.parentNode();
    if (parentNode) {
      // Send a childList mutation on the frame so that clients know
      // they should reread the children list.
      this.queueMutation({
        type: "childList",
        target: this._refMap.get(parentNode).actorID,
        added: [],
        removed: []
      });
    }

    // Need to force a release of this node, because those nodes can't
    // be accessed anymore.
    this.releaseNode(documentActor, { force: true });
  },

  /**
   * Check if a node is attached to the DOM tree of the current page.
   * @param {nsIDomNode} rawNode
   * @return {Boolean} false if the node is removed from the tree or within a
   * document fragment
   */
  _isInDOMTree: function(rawNode) {
    let walker = documentWalker(rawNode, this.rootWin);
    let current = walker.currentNode;

    // Reaching the top of tree
    while (walker.parentNode()) {
      current = walker.currentNode;
    }

    // The top of the tree is a fragment or is not rootDoc, hence rawNode isn't
    // attached
    if (current.nodeType === Ci.nsIDOMNode.DOCUMENT_FRAGMENT_NODE ||
        current !== this.rootDoc) {
      return false;
    }

    // Otherwise the top of the tree is rootDoc, hence rawNode is in rootDoc
    return true;
  },

  /**
   * @see _isInDomTree
   */
  isInDOMTree: method(function(node) {
    return node ? this._isInDOMTree(node.rawNode) : false;
  }, {
    request: { node: Arg(0, "domnode") },
    response: { attached: RetVal("boolean") }
  }),

  /**
   * Given an ObjectActor (identified by its ID), commonly used in the debugger,
   * webconsole and variablesView, return the corresponding inspector's NodeActor
   */
  getNodeActorFromObjectActor: method(function(objectActorID) {
    let debuggerObject = this.conn.getActor(objectActorID).obj;
    let rawNode = debuggerObject.unsafeDereference();

    if (!this._isInDOMTree(rawNode)) {
      return null;
    }

    // This is a special case for the document object whereby it is considered
    // as document.documentElement (the <html> node)
    if (rawNode.defaultView && rawNode === rawNode.defaultView.document) {
      rawNode = rawNode.documentElement;
    }

    return this.attachElement(rawNode);
  }, {
    request: {
      objectActorID: Arg(0, "string")
    },
    response: {
      nodeFront: RetVal("nullable:disconnectedNode")
    }
  }),
});

/**
 * Client side of the DOM walker.
 */
var WalkerFront = exports.WalkerFront = protocol.FrontClass(WalkerActor, {
  // Set to true if cleanup should be requested after every mutation list.
  autoCleanup: true,

  /**
   * This is kept for backward-compatibility reasons with older remote target.
   * Targets previous to bug 916443
   */
  pick: protocol.custom(function() {
    return this._pick().then(response => {
      return response.node;
    });
  }, {impl: "_pick"}),

  initialize: function(client, form) {
    this._createRootNodePromise();
    protocol.Front.prototype.initialize.call(this, client, form);
    this._orphaned = new Set();
    this._retainedOrphans = new Set();
  },

  destroy: function() {
    protocol.Front.prototype.destroy.call(this);
  },

  // Update the object given a form representation off the wire.
  form: function(json) {
    this.actorID = json.actor;
    this.rootNode = types.getType("domnode").read(json.root, this);
    this._rootNodeDeferred.resolve(this.rootNode);
  },

  /**
   * Clients can use walker.rootNode to get the current root node of the
   * walker, but during a reload the root node might be null.  This
   * method returns a promise that will resolve to the root node when it is
   * set.
   */
  getRootNode: function() {
    return this._rootNodeDeferred.promise;
  },

  /**
   * Create the root node promise, triggering the "new-root" notification
   * on resolution.
   */
  _createRootNodePromise: function() {
    this._rootNodeDeferred = promise.defer();
    this._rootNodeDeferred.promise.then(() => {
      events.emit(this, "new-root");
    });
  },

  /**
   * When reading an actor form off the wire, we want to hook it up to its
   * parent front.  The protocol guarantees that the parent will be seen
   * by the client in either a previous or the current request.
   * So if we've already seen this parent return it, otherwise create
   * a bare-bones stand-in node.  The stand-in node will be updated
   * with a real form by the end of the deserialization.
   */
  ensureParentFront: function(id) {
    let front = this.get(id);
    if (front) {
      return front;
    }

    return types.getType("domnode").read({ actor: id }, this, "standin");
  },

  /**
   * See the documentation for WalkerActor.prototype.retainNode for
   * information on retained nodes.
   *
   * From the client's perspective, `retainNode` can fail if the node in
   * question is removed from the ownership tree before the `retainNode`
   * request reaches the server.  This can only happen if the client has
   * asked the server to release nodes but hasn't gotten a response
   * yet: Either a `releaseNode` request or a `getMutations` with `cleanup`
   * set is outstanding.
   *
   * If either of those requests is outstanding AND releases the retained
   * node, this request will fail with noSuchActor, but the ownership tree
   * will stay in a consistent state.
   *
   * Because the protocol guarantees that requests will be processed and
   * responses received in the order they were sent, we get the right
   * semantics by setting our local retained flag on the node only AFTER
   * a SUCCESSFUL retainNode call.
   */
  retainNode: protocol.custom(function(node) {
    return this._retainNode(node).then(() => {
      node.retained = true;
    });
  }, {
    impl: "_retainNode",
  }),

  unretainNode: protocol.custom(function(node) {
    return this._unretainNode(node).then(() => {
      node.retained = false;
      if (this._retainedOrphans.has(node)) {
        this._retainedOrphans.delete(node);
        this._releaseFront(node);
      }
    });
  }, {
    impl: "_unretainNode"
  }),

  releaseNode: protocol.custom(function(node, options={}) {
    // NodeFront.destroy will destroy children in the ownership tree too,
    // mimicking what the server will do here.
    let actorID = node.actorID;
    this._releaseFront(node, !!options.force);
    return this._releaseNode({ actorID: actorID });
  }, {
    impl: "_releaseNode"
  }),

  querySelector: protocol.custom(function(queryNode, selector) {
    return this._querySelector(queryNode, selector).then(response => {
      return response.node;
    });
  }, {
    impl: "_querySelector"
  }),

  getNodeActorFromObjectActor: protocol.custom(function(objectActorID) {
    return this._getNodeActorFromObjectActor(objectActorID).then(response => {
      return response ? response.node : null;
    });
  }, {
    impl: "_getNodeActorFromObjectActor"
  }),

  _releaseFront: function(node, force) {
    if (node.retained && !force) {
      node.reparent(null);
      this._retainedOrphans.add(node);
      return;
    }

    if (node.retained) {
      // Forcing a removal.
      this._retainedOrphans.delete(node);
    }

    // Release any children
    for (let child of node.treeChildren()) {
      this._releaseFront(child, force);
    }

    // All children will have been removed from the node by this point.
    node.reparent(null);
    node.destroy();
  },

  /**
   * Get any unprocessed mutation records and process them.
   */
  getMutations: protocol.custom(function(options={}) {
    return this._getMutations(options).then(mutations => {
      let emitMutations = [];
      for (let change of mutations) {
        // The target is only an actorID, get the associated front.
        let targetID;
        let targetFront;

        if (change.type === "newRoot") {
          this.rootNode = types.getType("domnode").read(change.target, this);
          this._rootNodeDeferred.resolve(this.rootNode);
          targetID = this.rootNode.actorID;
          targetFront = this.rootNode;
        } else {
          targetID = change.target;
          targetFront = this.get(targetID);
        }

        if (!targetFront) {
          console.trace("Got a mutation for an unexpected actor: " + targetID + ", please file a bug on bugzilla.mozilla.org!");
          continue;
        }

        let emittedMutation = object.merge(change, { target: targetFront });

        if (change.type === "childList") {
          // Update the ownership tree according to the mutation record.
          let addedFronts = [];
          let removedFronts = [];
          for (let removed of change.removed) {
            let removedFront = this.get(removed);
            if (!removedFront) {
              console.error("Got a removal of an actor we didn't know about: " + removed);
              continue;
            }
            // Remove from the ownership tree
            removedFront.reparent(null);

            // This node is orphaned unless we get it in the 'added' list
            // eventually.
            this._orphaned.add(removedFront);
            removedFronts.push(removedFront);
          }
          for (let added of change.added) {
            let addedFront = this.get(added);
            if (!addedFront) {
              console.error("Got an addition of an actor we didn't know about: " + added);
              continue;
            }
            addedFront.reparent(targetFront)

            // The actor is reconnected to the ownership tree, unorphan
            // it.
            this._orphaned.delete(addedFront);
            addedFronts.push(addedFront);
          }
          // Before passing to users, replace the added and removed actor
          // ids with front in the mutation record.
          emittedMutation.added = addedFronts;
          emittedMutation.removed = removedFronts;
          targetFront._form.numChildren = change.numChildren;
        } else if (change.type === "frameLoad") {
          // Nothing we need to do here, except verify that we don't have any
          // document children, because we should have gotten a documentUnload
          // first.
          for (let child of targetFront.treeChildren()) {
            if (child.nodeType === Ci.nsIDOMNode.DOCUMENT_NODE) {
              console.trace("Got an unexpected frameLoad in the inspector, please file a bug on bugzilla.mozilla.org!");
            }
          }
        } else if (change.type === "documentUnload") {
          if (targetFront === this.rootNode) {
            this._createRootNodePromise();
          }

          // We try to give fronts instead of actorIDs, but these fronts need
          // to be destroyed now.
          emittedMutation.target = targetFront.actorID;
          emittedMutation.targetParent = targetFront.parentNode();

          // Release the document node and all of its children, even retained.
          this._releaseFront(targetFront, true);
        } else if (change.type === "unretained") {
          // Retained orphans were force-released without the intervention of
          // client (probably a navigated frame).
          for (let released of change.nodes) {
            let releasedFront = this.get(released);
            this._retainedOrphans.delete(released);
            this._releaseFront(releasedFront, true);
          }
        } else {
          targetFront.updateMutation(change);
        }

        emitMutations.push(emittedMutation);
      }

      if (options.cleanup) {
        for (let node of this._orphaned) {
          // This will move retained nodes to this._retainedOrphans.
          this._releaseFront(node);
        }
        this._orphaned = new Set();
      }

      events.emit(this, "mutations", emitMutations);
    });
  }, {
    impl: "_getMutations"
  }),

  /**
   * Handle the `new-mutations` notification by fetching the
   * available mutation records.
   */
  onMutations: protocol.preEvent("new-mutations", function() {
    // Fetch and process the mutations.
    this.getMutations({cleanup: this.autoCleanup}).then(null, console.error);
  }),

  isLocal: function() {
    return !!this.conn._transport._serverConnection;
  },

  // XXX hack during transition to remote inspector: get a proper NodeFront
  // for a given local node.  Only works locally.
  frontForRawNode: function(rawNode) {
    if (!this.isLocal()) {
      console.warn("Tried to use frontForRawNode on a remote connection.");
      return null;
    }
    let walkerActor = this.conn._transport._serverConnection.getActor(this.actorID);
    if (!walkerActor) {
      throw Error("Could not find client side for actor " + this.actorID);
    }
    let nodeActor = walkerActor._ref(rawNode);

    // Pass the node through a read/write pair to create the client side actor.
    let nodeType = types.getType("domnode");
    let returnNode = nodeType.read(nodeType.write(nodeActor, walkerActor), this);
    let top = returnNode;
    let extras = walkerActor.parents(nodeActor);
    for (let extraActor of extras) {
      top = nodeType.read(nodeType.write(extraActor, walkerActor), this);
    }

    if (top !== this.rootNode) {
      // Imported an already-orphaned node.
      this._orphaned.add(top);
      walkerActor._orphaned.add(this.conn._transport._serverConnection.getActor(top.actorID));
    }
    return returnNode;
  }
});

/**
 * Convenience API for building a list of attribute modifications
 * for the `modifyAttributes` request.
 */
var AttributeModificationList = Class({
  initialize: function(node) {
    this.node = node;
    this.modifications = [];
  },

  apply: function() {
    let ret = this.node.modifyAttributes(this.modifications);
    return ret;
  },

  destroy: function() {
    this.node = null;
    this.modification = null;
  },

  setAttributeNS: function(ns, name, value) {
    this.modifications.push({
      attributeNamespace: ns,
      attributeName: name,
      newValue: value
    });
  },

  setAttribute: function(name, value) {
    this.setAttributeNS(undefined, name, value);
  },

  removeAttributeNS: function(ns, name) {
    this.setAttributeNS(ns, name, undefined);
  },

  removeAttribute: function(name) {
    this.setAttributeNS(undefined, name, undefined);
  }
})

/**
 * Server side of the inspector actor, which is used to create
 * inspector-related actors, including the walker.
 */
var InspectorActor = protocol.ActorClass({
  typeName: "inspector",
  initialize: function(conn, tabActor) {
    protocol.Actor.prototype.initialize.call(this, conn);
    this.tabActor = tabActor;
  },

  get window() this.tabActor.window,

  getWalker: method(function(options={}) {
    if (this._walkerPromise) {
      return this._walkerPromise;
    }

    let deferred = promise.defer();
    this._walkerPromise = deferred.promise;

    let window = this.window;
    var domReady = () => {
      let tabActor = this.tabActor;
      window.removeEventListener("DOMContentLoaded", domReady, true);
      this.walker = WalkerActor(this.conn, tabActor, options);
      events.once(this.walker, "destroyed", () => {
        this._walkerPromise = null;
        this._pageStylePromise = null;
      });
      deferred.resolve(this.walker);
    };

    if (window.document.readyState === "loading") {
      window.addEventListener("DOMContentLoaded", domReady, true);
    } else {
      domReady();
    }

    return this._walkerPromise;
  }, {
    request: {},
    response: {
      walker: RetVal("domwalker")
    }
  }),

  getPageStyle: method(function() {
    if (this._pageStylePromise) {
      return this._pageStylePromise;
    }

    this._pageStylePromise = this.getWalker().then(walker => {
      return PageStyleActor(this);
    });
    return this._pageStylePromise;
  }, {
    request: {},
    response: {
      pageStyle: RetVal("pagestyle")
    }
  }),

  /**
   * The most used highlighter actor is the HighlighterActor which can be
   * conveniently retrieved by this method.
   * The same instance will always be returned by this method when called
   * several times.
   * The highlighter actor returned here is used to highlighter elements's
   * box-models from the markup-view, layout-view, console, debugger, ... as
   * well as select elements with the pointer (pick).
   *
   * @param {Boolean} autohide Optionally autohide the highlighter after an
   * element has been picked
   * @return {HighlighterActor}
   */
  getHighlighter: method(function (autohide) {
    if (this._highlighterPromise) {
      return this._highlighterPromise;
    }

    this._highlighterPromise = this.getWalker().then(walker => {
      return HighlighterActor(this, autohide);
    });
    return this._highlighterPromise;
  }, {
    request: {
      autohide: Arg(0, "boolean")
    },
    response: {
      highligter: RetVal("highlighter")
    }
  }),

  /**
   * If consumers need to display several highlighters at the same time or
   * different types of highlighters, then this method should be used, passing
   * the type name of the highlighter needed as argument.
   * A new instance will be created everytime the method is called, so it's up
   * to the consumer to release it when it is not needed anymore
   *
   * @param {String} type The type of highlighter to create
   * @return {Highlighter} The highlighter actor instance or null if the
   * typeName passed doesn't match any available highlighter
   */
  getHighlighterByType: method(function (typeName) {
    if (HIGHLIGHTER_CLASSES[typeName]) {
      return CustomHighlighterActor(this, typeName);
    } else {
      return null;
    }
  }, {
    request: {
      typeName: Arg(0)
    },
    response: {
      highlighter: RetVal("nullable:customhighlighter")
    }
  }),

  /**
   * Get the node's image data if any (for canvas and img nodes).
   * Returns an imageData object with the actual data being a LongStringActor
   * and a size json object.
   * The image data is transmitted as a base64 encoded png data-uri.
   * The method rejects if the node isn't an image or if the image is missing
   *
   * Accepts a maxDim request parameter to resize images that are larger. This
   * is important as the resizing occurs server-side so that image-data being
   * transfered in the longstring back to the client will be that much smaller
   */
  getImageDataFromURL: method(function(url, maxDim) {
    let deferred = promise.defer();
    let img = new this.window.Image();

    // On load, get the image data and send the response
    img.onload = () => {
      // imageToImageData throws an error if the image is missing
      try {
        let imageData = imageToImageData(img, maxDim);
        deferred.resolve({
          data: LongStringActor(this.conn, imageData.data),
          size: imageData.size
        });
      } catch (e) {
        deferred.reject(new Error("Image " + url+ " not available"));
      }
    }

    // If the URL doesn't point to a resource, reject
    img.onerror = () => {
      deferred.reject(new Error("Image " + url+ " not available"));
    }

    // If the request hangs for too long, kill it to avoid queuing up other requests
    // to the same actor, except if we're running tests
    if (!gDevTools.testing) {
      this.window.setTimeout(() => {
        deferred.reject(new Error("Image " + url + " could not be retrieved in time"));
      }, IMAGE_FETCHING_TIMEOUT);
    }

    img.src = url;

    return deferred.promise;
  }, {
    request: {url: Arg(0), maxDim: Arg(1, "nullable:number")},
    response: RetVal("imageData")
  })
});

/**
 * Client side of the inspector actor, which is used to create
 * inspector-related actors, including the walker.
 */
var InspectorFront = exports.InspectorFront = protocol.FrontClass(InspectorActor, {
  initialize: function(client, tabForm) {
    protocol.Front.prototype.initialize.call(this, client);
    this.actorID = tabForm.inspectorActor;

    // XXX: This is the first actor type in its hierarchy to use the protocol
    // library, so we're going to self-own on the client side for now.
    client.addActorPool(this);
    this.manage(this);
  },

  destroy: function() {
    delete this.walker;
    protocol.Front.prototype.destroy.call(this);
  },

  getWalker: protocol.custom(function() {
    return this._getWalker().then(walker => {
      this.walker = walker;
      return walker;
    });
  }, {
    impl: "_getWalker"
  }),

  getPageStyle: protocol.custom(function() {
    return this._getPageStyle().then(pageStyle => {
      // We need a walker to understand node references from the
      // node style.
      if (this.walker) {
        return pageStyle;
      }
      return this.getWalker().then(() => {
        return pageStyle;
      });
    });
  }, {
    impl: "_getPageStyle"
  })
});

function documentWalker(node, rootWin, whatToShow=Ci.nsIDOMNodeFilter.SHOW_ALL) {
  return new DocumentWalker(node, rootWin, whatToShow, whitespaceTextFilter, false);
}

// Exported for test purposes.
exports._documentWalker = documentWalker;

function nodeDocument(node) {
  return node.ownerDocument || (node.nodeType == Ci.nsIDOMNode.DOCUMENT_NODE ? node : null);
}

/**
 * Similar to a TreeWalker, except will dig in to iframes and it doesn't
 * implement the good methods like previousNode and nextNode.
 *
 * See TreeWalker documentation for explanations of the methods.
 */
function DocumentWalker(aNode, aRootWin, aShow, aFilter, aExpandEntityReferences) {
  let doc = nodeDocument(aNode);
  this.layoutHelpers = new LayoutHelpers(aRootWin);
  this.walker = doc.createTreeWalker(doc,
    aShow, aFilter, aExpandEntityReferences);
  this.walker.currentNode = aNode;
  this.filter = aFilter;
}

DocumentWalker.prototype = {
  get node() this.walker.node,
  get whatToShow() this.walker.whatToShow,
  get expandEntityReferences() this.walker.expandEntityReferences,
  get currentNode() this.walker.currentNode,
  set currentNode(aVal) this.walker.currentNode = aVal,

  /**
   * Called when the new node is in a different document than
   * the current node, creates a new treewalker for the document we've
   * run in to.
   */
  _reparentWalker: function(aNewNode) {
    if (!aNewNode) {
      return null;
    }
    let doc = nodeDocument(aNewNode);
    let walker = doc.createTreeWalker(doc,
      this.whatToShow, this.filter, this.expandEntityReferences);
    walker.currentNode = aNewNode;
    this.walker = walker;
    return aNewNode;
  },

  parentNode: function() {
    let currentNode = this.walker.currentNode;
    let parentNode = this.walker.parentNode();

    if (!parentNode) {
      if (currentNode && currentNode.nodeType == Ci.nsIDOMNode.DOCUMENT_NODE
          && currentNode.defaultView) {

        let window = currentNode.defaultView;
        let frame = this.layoutHelpers.getFrameElement(window);
        if (frame) {
          return this._reparentWalker(frame);
        }
      }
      return null;
    }

    return parentNode;
  },

  firstChild: function() {
    let node = this.walker.currentNode;
    if (!node)
      return null;
    if (node.contentDocument) {
      return this._reparentWalker(node.contentDocument);
    } else if (node.getSVGDocument) {
      return this._reparentWalker(node.getSVGDocument());
    }
    return this.walker.firstChild();
  },

  lastChild: function() {
    let node = this.walker.currentNode;
    if (!node)
      return null;
    if (node.contentDocument) {
      return this._reparentWalker(node.contentDocument);
    } else if (node.getSVGDocument) {
      return this._reparentWalker(node.getSVGDocument());
    }
    return this.walker.lastChild();
  },

  previousSibling: function DW_previousSibling() this.walker.previousSibling(),
  nextSibling: function DW_nextSibling() this.walker.nextSibling()
};

/**
 * A tree walker filter for avoiding empty whitespace text nodes.
 */
function whitespaceTextFilter(aNode) {
    if (aNode.nodeType == Ci.nsIDOMNode.TEXT_NODE &&
        !/[^\s]/.exec(aNode.nodeValue)) {
      return Ci.nsIDOMNodeFilter.FILTER_SKIP;
    } else {
      return Ci.nsIDOMNodeFilter.FILTER_ACCEPT;
    }
}

/**
 * Given an image DOMNode, return the image data-uri.
 * @param {DOMNode} node The image node
 * @param {Number} maxDim Optionally pass a maximum size you want the longest
 * side of the image to be resized to before getting the image data.
 * @return {Object} An object containing the data-uri and size-related information
 * {data: "...", size: {naturalWidth: 400, naturalHeight: 300, resized: true}}
 * @throws an error if the node isn't an image or if the image is missing
 */
function imageToImageData(node, maxDim) {
  let isImg = node.tagName.toLowerCase() === "img";
  let isCanvas = node.tagName.toLowerCase() === "canvas";

  if (!isImg && !isCanvas) {
    return null;
  }

  // Get the image resize ratio if a maxDim was provided
  let resizeRatio = 1;
  let imgWidth = node.naturalWidth || node.width;
  let imgHeight = node.naturalHeight || node.height;
  let imgMax = Math.max(imgWidth, imgHeight);
  if (maxDim && imgMax > maxDim) {
    resizeRatio = maxDim / imgMax;
  }

  // Extract the image data
  let imageData;
  // The image may already be a data-uri, in which case, save ourselves the
  // trouble of converting via the canvas.drawImage.toDataURL method
  if (isImg && node.src.startsWith("data:")) {
    imageData = node.src;
  } else {
    // Create a canvas to copy the rawNode into and get the imageData from
    let canvas = node.ownerDocument.createElementNS(XHTML_NS, "canvas");
    canvas.width = imgWidth * resizeRatio;
    canvas.height = imgHeight * resizeRatio;
    let ctx = canvas.getContext("2d");

    // Copy the rawNode image or canvas in the new canvas and extract data
    ctx.drawImage(node, 0, 0, canvas.width, canvas.height);
    imageData = canvas.toDataURL("image/png");
  }

  return {
    data: imageData,
    size: {
      naturalWidth: imgWidth,
      naturalHeight: imgHeight,
      resized: resizeRatio !== 1
    }
  }
}

loader.lazyGetter(this, "DOMUtils", function () {
  return Cc["@mozilla.org/inspector/dom-utils;1"].getService(Ci.inIDOMUtils);
});
